export type MCUFamily = 'avr' | 'arm' | 'esp32' | 'rp2040' | 'stm32';

export type AVRDevice =
  | 'ATmega328P'
  | 'ATmega32'
  | 'ATmega16'
  | 'ATmega64'
  | 'ATmega128';

export type SupportedMCU = AVRDevice;

export interface MemorySpec {
  flash: number;   // bytes
  ram: number;     // bytes
  eeprom: number;  // bytes
}

export interface PeripheralCount {
  usart: number;
  spi: number;
  i2c: number;
  adcChannels: number;
  timers: number;
  pwmChannels: number;
  externalInterrupts: number;
}

export interface PortSet {
  portA: boolean;
  portB: boolean;
  portC: boolean;
  portD: boolean;
  portE: boolean;
  portF: boolean;
  portG: boolean;
}

export interface MCUDefinition {
  name: SupportedMCU;
  family: MCUFamily;
  avrdudeId: string;
  memory: MemorySpec;
  peripherals: PeripheralCount;
  ports: PortSet;
  defaultClock: number;   // Hz
  maxClock: number;       // Hz
  voltageRange: [number, number]; // [min, max] in volts
}
