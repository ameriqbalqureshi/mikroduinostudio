/*
 * MikroDuino Arduino Compatibility Layer — implementation
 */

#include "../include/Arduino.h"
#include <avr/interrupt.h>
#include <util/delay.h>

// --------------------------------------------------------------------------
// Timekeeping via Timer0 overflow (counts every 1.024 ms at 16 MHz / 64)
// --------------------------------------------------------------------------
namespace _arduino_time {
    volatile uint32_t overflow_count = 0;
}

ISR(TIMER0_OVF_vect) {
    _arduino_time::overflow_count++;
}

namespace Arduino {
    void beginTimekeeping() {
        // Timer0 fast PWM, prescaler 64 → ~976 Hz overflow at 16 MHz
        // Matches Arduino millis() behaviour
        TCCR0A = (1<<WGM01)|(1<<WGM00);
        TCCR0B = (1<<CS01)|(1<<CS00);  // clk/64
        BITSET(TIMSK0, TOIE0);
        sei();
    }
}

// --------------------------------------------------------------------------
// GPIO
// --------------------------------------------------------------------------
void pinMode(uint8_t pin, uint8_t mode) {
#ifdef MD_MCU_ATMEGA328P
    uint8_t mdPin = (pin < sizeof(ARDUINO_PIN_MAP)) ? ARDUINO_PIN_MAP[pin] : MikroDuino::PB0;
#else
    uint8_t mdPin = pin;
#endif
    if (mode == OUTPUT)       MikroDuino::GPIO::output(mdPin);
    else if (mode == INPUT_PULLUP) MikroDuino::GPIO::inputPullup(mdPin);
    else                      MikroDuino::GPIO::input(mdPin);
}

void digitalWrite(uint8_t pin, uint8_t value) {
#ifdef MD_MCU_ATMEGA328P
    uint8_t mdPin = (pin < sizeof(ARDUINO_PIN_MAP)) ? ARDUINO_PIN_MAP[pin] : MikroDuino::PB0;
#else
    uint8_t mdPin = pin;
#endif
    MikroDuino::GPIO::write(mdPin, value != 0);
}

int digitalRead(uint8_t pin) {
#ifdef MD_MCU_ATMEGA328P
    uint8_t mdPin = (pin < sizeof(ARDUINO_PIN_MAP)) ? ARDUINO_PIN_MAP[pin] : MikroDuino::PB0;
#else
    uint8_t mdPin = pin;
#endif
    return MikroDuino::GPIO::read(mdPin) ? HIGH : LOW;
}

// --------------------------------------------------------------------------
// ADC
// --------------------------------------------------------------------------
int analogRead(uint8_t pin) {
    // pin is 0-5 for A0-A5 on 328P (or 14-19 mapped back)
    uint8_t ch = (pin >= 14) ? (pin - 14) : pin;
    MikroDuino::ADC_Driver.begin();
    return static_cast<int>(MikroDuino::ADC_Driver.read(ch));
}

void analogWrite(uint8_t pin, uint8_t value) {
    // Only OC1A (D9) and OC1B (D10) supported via PWM1 for simplicity
    // For full PWM support use MikroDuino::PWM1 directly
    if (pin == 9) {
        MikroDuino::PWM1.begin(490);
        MikroDuino::PWM1.rawA(static_cast<uint16_t>(value) * (MikroDuino::PWM1.top() / 255));
    } else if (pin == 10) {
        MikroDuino::PWM1.begin(490);
        MikroDuino::PWM1.rawB(static_cast<uint16_t>(value) * (MikroDuino::PWM1.top() / 255));
    }
}

// --------------------------------------------------------------------------
// Timing
// --------------------------------------------------------------------------
uint32_t millis(void) {
    // Each overflow = 256 / (16e6/64) = 1.024 ms → 0.9765625 ms rounded
    uint32_t ov;
    {
        ATOMIC_BLOCK_START;
        ov = _arduino_time::overflow_count;
        ATOMIC_BLOCK_END;
    }
    return (ov * 64UL * 256UL) / (F_CPU / 1000UL);
}

uint32_t micros(void) {
    uint32_t ov;
    uint8_t tcnt;
    {
        ATOMIC_BLOCK_START;
        ov   = _arduino_time::overflow_count;
        tcnt = TCNT0;
        ATOMIC_BLOCK_END;
    }
    uint32_t ticks = (ov * 256UL + tcnt) * 64UL;
    return ticks / (F_CPU / 1000000UL);
}

void delay(uint32_t ms) {
    while (ms--) _delay_ms(1.0);
}

void delayMicroseconds(uint32_t us) {
    while (us--) _delay_us(1.0);
}

// --------------------------------------------------------------------------
// pulseIn
// --------------------------------------------------------------------------
uint32_t pulseIn(uint8_t pin, uint8_t state, uint32_t timeout) {
    uint32_t start = micros();
    while (digitalRead(pin) == state)
        if (micros() - start > timeout) return 0;
    while (digitalRead(pin) != state)
        if (micros() - start > timeout) return 0;
    uint32_t pulseStart = micros();
    while (digitalRead(pin) == state)
        if (micros() - start > timeout) return 0;
    return micros() - pulseStart;
}

// --------------------------------------------------------------------------
// Bit-bang serial shift protocol
// --------------------------------------------------------------------------
void shiftOut(uint8_t dataPin, uint8_t clockPin, uint8_t bitOrder, uint8_t val) {
    for (uint8_t i = 0; i < 8; ++i) {
        uint8_t bit = (bitOrder == LSBFIRST) ? (val & (1 << i)) : (val & (0x80 >> i));
        digitalWrite(dataPin, bit ? HIGH : LOW);
        digitalWrite(clockPin, HIGH);
        digitalWrite(clockPin, LOW);
    }
}

uint8_t shiftIn(uint8_t dataPin, uint8_t clockPin, uint8_t bitOrder) {
    uint8_t val = 0;
    for (uint8_t i = 0; i < 8; ++i) {
        digitalWrite(clockPin, HIGH);
        uint8_t bit = static_cast<uint8_t>(digitalRead(dataPin));
        if (bitOrder == LSBFIRST) val |= static_cast<uint8_t>(bit << i);
        else                      val |= static_cast<uint8_t>(bit << (7 - i));
        digitalWrite(clockPin, LOW);
    }
    return val;
}

// --------------------------------------------------------------------------
// Tone stubs — timer allocation conflicts with user timers; not implemented.
// Libraries that call tone() compile fine; the pin stays silent.
// --------------------------------------------------------------------------
void tone(uint8_t /*pin*/, unsigned int /*frequency*/, unsigned long /*duration*/) {}
void noTone(uint8_t /*pin*/) {}

// --------------------------------------------------------------------------
// Serial instance
// --------------------------------------------------------------------------
HardwareSerial Serial;

// --------------------------------------------------------------------------
// operator delete stubs
// avr-libc exposes only free(); it does not provide operator delete in any
// form. C++14/17 virtual destructors emit sized-delete calls. Supply all
// four variants so the linker is always satisfied.
// --------------------------------------------------------------------------
#include <stdlib.h>
void operator delete  (void *ptr)              noexcept { free(ptr); }
void operator delete[](void *ptr)              noexcept { free(ptr); }
void operator delete  (void *ptr, unsigned int)noexcept { free(ptr); }
void operator delete[](void *ptr, unsigned int)noexcept { free(ptr); }
