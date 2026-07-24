import { SerialPort } from 'serialport';
import type { SerialConfig, SerialEventCallback, SerialStatusCallback } from './types.js';

export class SerialConnection {
  private _port: SerialPort | null = null;
  private _onData?: SerialEventCallback;
  private _onStatus?: SerialStatusCallback;
  private _config: SerialConfig | null = null;
  private _reconnectTimer: ReturnType<typeof setTimeout> | null = null;

  onData(cb: SerialEventCallback): void    { this._onData = cb; }
  onStatus(cb: SerialStatusCallback): void { this._onStatus = cb; }

  async open(config: SerialConfig): Promise<void> {
    this._config = config;
    await this.connect(config);
  }

  async close(): Promise<void> {
    if (this._reconnectTimer) {
      clearTimeout(this._reconnectTimer);
      this._reconnectTimer = null;
    }
    if (this._port?.isOpen) {
      await new Promise<void>((res, rej) => this._port!.close(e => e ? rej(e) : res()));
    }
    this._port = null;
  }

  send(data: string | Buffer): void {
    if (!this._port?.isOpen) return;

    const buf = typeof data === 'string' ? Buffer.from(data, 'utf8') : data;
    this._port.write(buf);

    this._onData?.({
      timestamp: new Date(),
      data:      buf,
      direction: 'tx',
    });
  }

  get isConnected(): boolean {
    return this._port?.isOpen ?? false;
  }

  // ── Static utility ─────────────────────────────────────────────────────────

  static async listPorts(): Promise<string[]> {
    const ports = await SerialPort.list();
    return ports.map(p => p.path);
  }

  // ── Private ────────────────────────────────────────────────────────────────

  private async connect(config: SerialConfig): Promise<void> {
    const port = new SerialPort({
      path:     config.port,
      baudRate: config.baudRate,
      dataBits: config.dataBits ?? 8,
      stopBits: config.stopBits ?? 1,
      parity:   config.parity  ?? 'none',
      autoOpen: false,
    });

    await new Promise<void>((resolve, reject) => {
      port.open(err => err ? reject(err) : resolve());
    });

    port.on('data', (chunk: Buffer) => {
      this._onData?.({
        timestamp: new Date(),
        data:      chunk,
        direction: 'rx',
      });
    });

    port.on('close', () => {
      this._onStatus?.(false, config.port);
      if (config.autoReconnect && this._config) {
        this._reconnectTimer = setTimeout(() => {
          void this.connect(this._config!);
        }, 2000);
      }
    });

    port.on('error', (err: Error) => {
      this._onStatus?.(false, config.port);
      if (config.autoReconnect && this._config) {
        this._reconnectTimer = setTimeout(() => {
          void this.connect(this._config!);
        }, 2000);
      }
      void err; // suppress unused warning
    });

    this._port = port;
    this._onStatus?.(true, config.port);
  }
}
