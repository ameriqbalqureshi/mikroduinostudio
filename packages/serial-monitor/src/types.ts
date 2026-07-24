export type SerialDataView = 'text' | 'hex' | 'binary';
export type LineEnding = 'none' | 'cr' | 'lf' | 'crlf';

export interface SerialConfig {
  port: string;
  baudRate: number;
  dataBits?: 5 | 6 | 7 | 8;
  stopBits?: 1 | 2;
  parity?: 'none' | 'even' | 'odd';
  autoReconnect?: boolean;
}

export interface SerialFrame {
  timestamp: Date;
  data: Buffer;
  direction: 'rx' | 'tx';
}

export type SerialEventCallback = (frame: SerialFrame) => void;
export type SerialStatusCallback = (connected: boolean, port: string) => void;
