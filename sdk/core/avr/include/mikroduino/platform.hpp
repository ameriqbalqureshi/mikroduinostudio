#pragma once
/*
 * MikroDuino Platform Detection
 * Detects the target MCU and exposes capability flags.
 */

#include <avr/io.h>
#include <stdint.h>

// --------------------------------------------------------------------------
// MCU detection
// --------------------------------------------------------------------------
#if defined(__AVR_ATmega328P__)
  #define MD_MCU_ATMEGA328P  1
  #define MD_MCU_NAME        "ATmega328P"
  #define MD_FLASH_SIZE      32768UL
  #define MD_RAM_SIZE        2048UL
  #define MD_EEPROM_SIZE     1024UL
  #define MD_HAS_PORTA       0
  #define MD_HAS_PORTB       1
  #define MD_HAS_PORTC       1
  #define MD_HAS_PORTD       1
  #define MD_HAS_PORTE       0
  #define MD_HAS_PORTF       0
  #define MD_HAS_PORTG       0
  #define MD_USART_COUNT     1
  #define MD_ADC_CHANNELS    6
  #define MD_TIMER_COUNT     3
  #define MD_EXT_INT_COUNT   2

#elif defined(__AVR_ATmega32__)
  #define MD_MCU_ATMEGA32    1
  #define MD_MCU_NAME        "ATmega32"
  #define MD_FLASH_SIZE      32768UL
  #define MD_RAM_SIZE        2048UL
  #define MD_EEPROM_SIZE     1024UL
  #define MD_HAS_PORTA       1
  #define MD_HAS_PORTB       1
  #define MD_HAS_PORTC       1
  #define MD_HAS_PORTD       1
  #define MD_HAS_PORTE       0
  #define MD_HAS_PORTF       0
  #define MD_HAS_PORTG       0
  #define MD_USART_COUNT     1
  #define MD_ADC_CHANNELS    8
  #define MD_TIMER_COUNT     3
  #define MD_EXT_INT_COUNT   3

#elif defined(__AVR_ATmega16__)
  #define MD_MCU_ATMEGA16    1
  #define MD_MCU_NAME        "ATmega16"
  #define MD_FLASH_SIZE      16384UL
  #define MD_RAM_SIZE        1024UL
  #define MD_EEPROM_SIZE     512UL
  #define MD_HAS_PORTA       1
  #define MD_HAS_PORTB       1
  #define MD_HAS_PORTC       1
  #define MD_HAS_PORTD       1
  #define MD_HAS_PORTE       0
  #define MD_HAS_PORTF       0
  #define MD_HAS_PORTG       0
  #define MD_USART_COUNT     1
  #define MD_ADC_CHANNELS    8
  #define MD_TIMER_COUNT     3
  #define MD_EXT_INT_COUNT   3

#elif defined(__AVR_ATmega64__)
  #define MD_MCU_ATMEGA64    1
  #define MD_MCU_NAME        "ATmega64"
  #define MD_FLASH_SIZE      65536UL
  #define MD_RAM_SIZE        4096UL
  #define MD_EEPROM_SIZE     2048UL
  #define MD_HAS_PORTA       1
  #define MD_HAS_PORTB       1
  #define MD_HAS_PORTC       1
  #define MD_HAS_PORTD       1
  #define MD_HAS_PORTE       1
  #define MD_HAS_PORTF       1
  #define MD_HAS_PORTG       1
  #define MD_USART_COUNT     2
  #define MD_ADC_CHANNELS    8
  #define MD_TIMER_COUNT     4
  #define MD_EXT_INT_COUNT   8

#elif defined(__AVR_ATmega128__)
  #define MD_MCU_ATMEGA128   1
  #define MD_MCU_NAME        "ATmega128"
  #define MD_FLASH_SIZE      131072UL
  #define MD_RAM_SIZE        4096UL
  #define MD_EEPROM_SIZE     4096UL
  #define MD_HAS_PORTA       1
  #define MD_HAS_PORTB       1
  #define MD_HAS_PORTC       1
  #define MD_HAS_PORTD       1
  #define MD_HAS_PORTE       1
  #define MD_HAS_PORTF       1
  #define MD_HAS_PORTG       1
  #define MD_USART_COUNT     2
  #define MD_ADC_CHANNELS    8
  #define MD_TIMER_COUNT     4
  #define MD_EXT_INT_COUNT   8

#else
  #warning "MikroDuino: Unknown AVR target. Some features may not be available."
  #define MD_MCU_NAME        "Unknown"
  #define MD_FLASH_SIZE      0UL
  #define MD_RAM_SIZE        0UL
  #define MD_EEPROM_SIZE     0UL
  #define MD_HAS_PORTA       0
  #define MD_HAS_PORTB       1
  #define MD_HAS_PORTC       1
  #define MD_HAS_PORTD       1
  #define MD_HAS_PORTE       0
  #define MD_HAS_PORTF       0
  #define MD_HAS_PORTG       0
  #define MD_USART_COUNT     1
  #define MD_ADC_CHANNELS    6
  #define MD_TIMER_COUNT     3
  #define MD_EXT_INT_COUNT   2
#endif

// --------------------------------------------------------------------------
// F_CPU guard
// --------------------------------------------------------------------------
#ifndef F_CPU
  #define F_CPU 16000000UL
  #warning "F_CPU not defined. Defaulting to 16 MHz."
#endif
