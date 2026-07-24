import type { ProgrammerType, SupportedMCU } from '@mikroduino/shared';

export interface ProgrammerConfig {
  avrdudePath: string;
  programmerType: ProgrammerType;
  port: string;           // e.g. COM3, /dev/ttyUSB0, usb
  baudRate?: number;
  mcu: SupportedMCU;
}

export interface FlashOptions {
  hexFile: string;
  verifyAfterWrite?: boolean;
}

export interface FuseBytes {
  low: number;
  high: number;
  extended?: number;
}

export type ProgressCallback = (percent: number, message: string) => void;
