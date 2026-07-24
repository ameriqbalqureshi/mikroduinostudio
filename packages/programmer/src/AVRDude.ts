import { spawn } from 'child_process';
import { getMCUDefinition } from '@mikroduino/shared';
import type { ProgrammerConfig, FlashOptions, FuseBytes, ProgressCallback } from './types.js';

const PROGRAMMER_ID: Record<string, string> = {
  USBASP:  'usbasp',
  AVRISP:  'avrisp2',
  STK500:  'stk500v2',
  ARDUINO: 'arduino',
  DRAGON:  'dragon_jtag',
  JTAG:    'jtag2',
};

export class AVRDude {
  constructor(private readonly config: ProgrammerConfig) {}

  // ── Signature read ─────────────────────────────────────────────────────────

  async readSignature(onProgress?: ProgressCallback): Promise<string> {
    const output = await this.run(
      ['-n', ...this.baseArgs()],
      onProgress
    );
    const m = /Device signature = (0x[0-9A-Fa-f]+)/.exec(output);
    return m ? m[1] : 'unknown';
  }

  // ── Flash write ────────────────────────────────────────────────────────────

  async writeFlash(opts: FlashOptions, onProgress?: ProgressCallback): Promise<void> {
    const args = [
      ...this.baseArgs(),
      `-U`, `flash:w:${opts.hexFile}:i`,
    ];
    if (opts.verifyAfterWrite) args.push('-V');
    await this.run(args, onProgress);
  }

  // ── Flash read ─────────────────────────────────────────────────────────────

  async readFlash(outputFile: string, onProgress?: ProgressCallback): Promise<void> {
    await this.run([
      ...this.baseArgs(),
      '-U', `flash:r:${outputFile}:i`,
    ], onProgress);
  }

  // ── EEPROM ─────────────────────────────────────────────────────────────────

  async writeEEPROM(hexFile: string, onProgress?: ProgressCallback): Promise<void> {
    await this.run([
      ...this.baseArgs(),
      '-U', `eeprom:w:${hexFile}:i`,
    ], onProgress);
  }

  async readEEPROM(outputFile: string, onProgress?: ProgressCallback): Promise<void> {
    await this.run([
      ...this.baseArgs(),
      '-U', `eeprom:r:${outputFile}:i`,
    ], onProgress);
  }

  // ── Fuse bits ──────────────────────────────────────────────────────────────

  async readFuses(onProgress?: ProgressCallback): Promise<FuseBytes> {
    const output = await this.run([
      ...this.baseArgs(),
      '-U', 'lfuse:r:-:h',
      '-U', 'hfuse:r:-:h',
      '-U', 'efuse:r:-:h',
    ], onProgress);

    const low      = parseInt(/lfuse.*?(0x[0-9A-Fa-f]+)/i.exec(output)?.[1] ?? '0xff', 16);
    const high     = parseInt(/hfuse.*?(0x[0-9A-Fa-f]+)/i.exec(output)?.[1] ?? '0xff', 16);
    const extended = parseInt(/efuse.*?(0x[0-9A-Fa-f]+)/i.exec(output)?.[1] ?? '0xff', 16);

    return { low, high, extended };
  }

  async writeFuses(fuses: FuseBytes, onProgress?: ProgressCallback): Promise<void> {
    const args = [
      ...this.baseArgs(),
      '-U', `lfuse:w:${fuses.low}:m`,
      '-U', `hfuse:w:${fuses.high}:m`,
    ];
    if (fuses.extended !== undefined) {
      args.push('-U', `efuse:w:${fuses.extended}:m`);
    }
    await this.run(args, onProgress);
  }

  // ── Erase ──────────────────────────────────────────────────────────────────

  async chipErase(onProgress?: ProgressCallback): Promise<void> {
    await this.run([...this.baseArgs(), '-e'], onProgress);
  }

  // ── Private ────────────────────────────────────────────────────────────────

  private baseArgs(): string[] {
    const mcuDef = getMCUDefinition(this.config.mcu);
    const progId = PROGRAMMER_ID[this.config.programmerType] ?? 'usbasp';

    const args = [
      '-p', mcuDef.avrdudeId,
      '-c', progId,
    ];

    if (this.config.port) {
      args.push('-P', this.config.port);
    }
    if (this.config.baudRate && this.config.programmerType !== 'USBASP') {
      args.push('-b', String(this.config.baudRate));
    }

    return args;
  }

  private run(args: string[], onProgress?: ProgressCallback): Promise<string> {
    return new Promise((resolve, reject) => {
      const proc = spawn(this.config.avrdudePath, args, { shell: false });
      let output = '';

      const onData = (chunk: Buffer): void => {
        const text = chunk.toString('utf8');
        output += text;

        // Parse progress from avrdude output: "Writing | ######### | 45%"
        const pMatch = /(\d+)%/.exec(text);
        if (pMatch && onProgress) {
          onProgress(parseInt(pMatch[1], 10), text.trim());
        }
      };

      proc.stdout.on('data', onData);
      proc.stderr.on('data', onData); // avrdude writes progress to stderr

      proc.on('error', reject);
      proc.on('close', (code) => {
        if (code === 0) resolve(output);
        else reject(new Error(`avrdude exited with code ${code ?? 'null'}\n${output}`));
      });
    });
  }
}
