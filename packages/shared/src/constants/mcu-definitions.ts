import type { MCUDefinition, SupportedMCU } from '../types/mcu.js';

export const MCU_DEFINITIONS: Record<SupportedMCU, MCUDefinition> = {
  ATmega328P: {
    name: 'ATmega328P',
    family: 'avr',
    avrdudeId: 'm328p',
    memory: { flash: 32768, ram: 2048, eeprom: 1024 },
    peripherals: {
      usart: 1, spi: 1, i2c: 1,
      adcChannels: 6, timers: 3, pwmChannels: 6,
      externalInterrupts: 2,
    },
    ports: {
      portA: false, portB: true, portC: true, portD: true,
      portE: false, portF: false, portG: false,
    },
    defaultClock: 16_000_000,
    maxClock: 20_000_000,
    voltageRange: [1.8, 5.5],
  },

  ATmega32: {
    name: 'ATmega32',
    family: 'avr',
    avrdudeId: 'm32',
    memory: { flash: 32768, ram: 2048, eeprom: 1024 },
    peripherals: {
      usart: 1, spi: 1, i2c: 1,
      adcChannels: 8, timers: 3, pwmChannels: 4,
      externalInterrupts: 3,
    },
    ports: {
      portA: true, portB: true, portC: true, portD: true,
      portE: false, portF: false, portG: false,
    },
    defaultClock: 16_000_000,
    maxClock: 16_000_000,
    voltageRange: [2.7, 5.5],
  },

  ATmega16: {
    name: 'ATmega16',
    family: 'avr',
    avrdudeId: 'm16',
    memory: { flash: 16384, ram: 1024, eeprom: 512 },
    peripherals: {
      usart: 1, spi: 1, i2c: 1,
      adcChannels: 8, timers: 3, pwmChannels: 4,
      externalInterrupts: 3,
    },
    ports: {
      portA: true, portB: true, portC: true, portD: true,
      portE: false, portF: false, portG: false,
    },
    defaultClock: 8_000_000,
    maxClock: 16_000_000,
    voltageRange: [2.7, 5.5],
  },

  ATmega64: {
    name: 'ATmega64',
    family: 'avr',
    avrdudeId: 'm64',
    memory: { flash: 65536, ram: 4096, eeprom: 2048 },
    peripherals: {
      usart: 2, spi: 1, i2c: 1,
      adcChannels: 8, timers: 4, pwmChannels: 8,
      externalInterrupts: 8,
    },
    ports: {
      portA: true, portB: true, portC: true, portD: true,
      portE: true, portF: true, portG: true,
    },
    defaultClock: 16_000_000,
    maxClock: 16_000_000,
    voltageRange: [2.7, 5.5],
  },

  ATmega128: {
    name: 'ATmega128',
    family: 'avr',
    avrdudeId: 'm128',
    memory: { flash: 131072, ram: 4096, eeprom: 4096 },
    peripherals: {
      usart: 2, spi: 1, i2c: 1,
      adcChannels: 8, timers: 4, pwmChannels: 8,
      externalInterrupts: 8,
    },
    ports: {
      portA: true, portB: true, portC: true, portD: true,
      portE: true, portF: true, portG: true,
    },
    defaultClock: 16_000_000,
    maxClock: 16_000_000,
    voltageRange: [2.7, 5.5],
  },
};

export function getMCUDefinition(mcu: SupportedMCU): MCUDefinition {
  return MCU_DEFINITIONS[mcu];
}

export function isSupportedMCU(name: string): name is SupportedMCU {
  return Object.prototype.hasOwnProperty.call(MCU_DEFINITIONS, name);
}
