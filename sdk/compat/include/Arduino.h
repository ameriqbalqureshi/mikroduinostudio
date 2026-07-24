#pragma once
/*
 * MikroDuino Arduino Compatibility Layer
 *
 * Optional. Include this file in projects that need Arduino API compatibility.
 * Maps Arduino functions to MikroDuino Core internally.
 * This file does NOT affect projects that do not include it.
 *
 * Note: millis() and micros() require Timer0 overflow interrupt.
 * They must be explicitly started with Arduino::beginTimekeeping().
 */

#include <mikroduino/mikroduino.hpp>
#include <stdint.h>
#include <stdlib.h>
#include <avr/pgmspace.h>   // PROGMEM, pgm_read_byte, pgm_read_word, etc.

// --------------------------------------------------------------------------
// Pin mode / direction
// --------------------------------------------------------------------------
#define INPUT          0
#define OUTPUT         1
#define INPUT_PULLUP   2

// --------------------------------------------------------------------------
// Logic levels
// --------------------------------------------------------------------------
#define HIGH 1
#define LOW  0

// --------------------------------------------------------------------------
// Numeric base constants (for Serial.print(val, HEX) style calls)
// --------------------------------------------------------------------------
#define HEX 16
#define DEC 10
#define OCT  8
#define BIN  2

// --------------------------------------------------------------------------
// Arduino type aliases
// --------------------------------------------------------------------------
typedef bool     boolean;
typedef uint8_t  byte;
typedef uint16_t word;

// --------------------------------------------------------------------------
// Bit order (for shiftIn / shiftOut)
// --------------------------------------------------------------------------
#define LSBFIRST 0
#define MSBFIRST 1

// --------------------------------------------------------------------------
// Bit manipulation macros (used by many Arduino libraries)
// --------------------------------------------------------------------------
#define bit(b)                   (1UL << (b))
#define bitRead(value, bit)      (((value) >> (bit)) & 0x01)
#define bitSet(value, bit)       ((value) |= (1UL << (bit)))
#define bitClear(value, bit)     ((value) &= ~(1UL << (bit)))
#define bitWrite(value, bit, w)  ((w) ? bitSet(value, bit) : bitClear(value, bit))

// --------------------------------------------------------------------------
// Global interrupt enable / disable
// --------------------------------------------------------------------------
#include <avr/interrupt.h>
#define interrupts()    sei()
#define noInterrupts()  cli()

// --------------------------------------------------------------------------
// Math helpers Arduino code expects
// --------------------------------------------------------------------------
#define PI      3.14159265358979323846
#define HALF_PI 1.5707963267948966192
#define TWO_PI  6.28318530717958647692
#define DEG_TO_RAD 0.01745329251994329577
#define RAD_TO_DEG 57.2957795130823208768

#define min(a,b)    ((a)<(b)?(a):(b))
#define max(a,b)    ((a)>(b)?(a):(b))
#ifndef abs
#define abs(x)      ((x)>0?(x):-(x))
#endif
#define constrain(v,lo,hi) ((v)<(lo)?(lo):((v)>(hi)?(hi):(v)))
#define sq(x)       ((x)*(x))
#define map(val,fromLow,fromHigh,toLow,toHigh) \
    ((val - fromLow) * (toHigh - toLow) / (fromHigh - fromLow) + toLow)

// --------------------------------------------------------------------------
// Arduino pin → MikroDuino pin translation (ATmega328P layout by default)
// --------------------------------------------------------------------------
#ifdef MD_MCU_ATMEGA328P
  static const uint8_t ARDUINO_PIN_MAP[] = {
  /* D0  */ MikroDuino::PD0,
  /* D1  */ MikroDuino::PD1,
  /* D2  */ MikroDuino::PD2,
  /* D3  */ MikroDuino::PD3,
  /* D4  */ MikroDuino::PD4,
  /* D5  */ MikroDuino::PD5,
  /* D6  */ MikroDuino::PD6,
  /* D7  */ MikroDuino::PD7,
  /* D8  */ MikroDuino::PB0,
  /* D9  */ MikroDuino::PB1,
  /* D10 */ MikroDuino::PB2,
  /* D11 */ MikroDuino::PB3,
  /* D12 */ MikroDuino::PB4,
  /* D13 */ MikroDuino::PB5,
  /* A0  */ MikroDuino::PC0,
  /* A1  */ MikroDuino::PC1,
  /* A2  */ MikroDuino::PC2,
  /* A3  */ MikroDuino::PC3,
  /* A4  */ MikroDuino::PC4,
  /* A5  */ MikroDuino::PC5,
  };
  constexpr uint8_t A0 = 14;
  constexpr uint8_t A1 = 15;
  constexpr uint8_t A2 = 16;
  constexpr uint8_t A3 = 17;
  constexpr uint8_t A4 = 18;
  constexpr uint8_t A5 = 19;
#endif

// --------------------------------------------------------------------------
// Timekeeping (Timer0 based — must call Arduino::beginTimekeeping() first)
// --------------------------------------------------------------------------
namespace _arduino_time {
    extern volatile uint32_t overflow_count;
}

// --------------------------------------------------------------------------
// C-style function declarations
// --------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

void     pinMode(uint8_t pin, uint8_t mode);
void     digitalWrite(uint8_t pin, uint8_t value);
int      digitalRead(uint8_t pin);
int      analogRead(uint8_t pin);
void     analogWrite(uint8_t pin, uint8_t value);

void     delay(uint32_t ms);
void     delayMicroseconds(uint32_t us);

uint32_t millis(void);
uint32_t micros(void);

uint32_t pulseIn(uint8_t pin, uint8_t state, uint32_t timeout);

// Bit-shift serial protocol helpers
void    shiftOut(uint8_t dataPin, uint8_t clockPin, uint8_t bitOrder, uint8_t val);
uint8_t shiftIn (uint8_t dataPin, uint8_t clockPin, uint8_t bitOrder);

// Tone generation stubs — hardware timer required; not yet implemented.
// Libraries that call tone() will compile but the function does nothing.
void tone(uint8_t pin, unsigned int frequency, unsigned long duration = 0);
void noTone(uint8_t pin);

// Cooperative scheduling stub — called by some libraries in long loops.
inline void yield() {}

#ifdef __cplusplus
}
#endif

// --------------------------------------------------------------------------
// Random — avr-libc provides long random(void) and srandom(); wrap them
// to match the two Arduino signatures.  C++ overloads avoid the extern "C"
// conflict with avr-libc's no-arg random().
// --------------------------------------------------------------------------
#ifdef __cplusplus
inline void    randomSeed(uint32_t seed) { srandom(seed); }
inline int32_t random(int32_t maxVal)    { return static_cast<int32_t>(::random() % static_cast<unsigned long>(maxVal)); }
inline int32_t random(int32_t minVal, int32_t maxVal) {
    return minVal + static_cast<int32_t>(::random() % static_cast<unsigned long>(maxVal - minVal));
}
#endif

// --------------------------------------------------------------------------
// Arduino String class
// --------------------------------------------------------------------------
#ifdef __cplusplus
#include "WString.h"
#endif

// --------------------------------------------------------------------------
// Serial — wraps MikroDuino::USART0
// --------------------------------------------------------------------------
#ifdef __cplusplus

#include <stdlib.h>   // dtostrf

class HardwareSerial {
public:
    void begin(uint32_t baud) { MikroDuino::USART0.begin(baud); }
    void end()                { MikroDuino::USART0.end(); }
    void flush()              { MikroDuino::USART0.flush(); }

    // --- write ---
    void write(uint8_t b)     { MikroDuino::USART0.write(b); }

    // --- print (no newline) ---
    void print(const char* s) { MikroDuino::USART0.write(s); }
    void print(int32_t v)     { MikroDuino::USART0.writeInt(v); }
    void print(uint32_t v, uint8_t base = DEC) {
        MikroDuino::USART0.writeInt(static_cast<int32_t>(v), base);
    }
    void print(int v)         { print(static_cast<int32_t>(v)); }
    void print(unsigned int v, uint8_t base = DEC) { print(static_cast<uint32_t>(v), base); }
    void print(float v,  uint8_t decimals = 2) {
        char buf[16];
        dtostrf(v, 0, decimals, buf);   // AVR-libc float→string
        MikroDuino::USART0.write(buf);
    }
    void print(double v, uint8_t decimals = 2) {
        print(static_cast<float>(v), decimals);
    }
    void print(char c)        { MikroDuino::USART0.write(static_cast<uint8_t>(c)); }

    // --- println (with CRLF) ---
    void println()            { MikroDuino::USART0.writeLine(""); }
    void println(const char* s)           { MikroDuino::USART0.writeLine(s); }
    void println(int32_t v)               { print(v);           println(); }
    void println(uint32_t v, uint8_t base = DEC) { print(v, base); println(); }
    void println(int v)                   { print(v);           println(); }
    void println(unsigned int v, uint8_t base = DEC) { print(v, base); println(); }
    void println(float v,  uint8_t decimals = 2) { print(v, decimals); println(); }
    void println(double v, uint8_t decimals = 2) { print(v, decimals); println(); }
    void println(char c)                  { print(c);           println(); }

    // --- String overloads ---
    void print(const String& s)   { MikroDuino::USART0.write(s.c_str()); }
    void println(const String& s) { print(s); println(); }

    // --- receive ---
    int  available() { return MikroDuino::USART0.available() ? 1 : 0; }
    int  read()      { return MikroDuino::USART0.read(); }
    int  peek()      { return MikroDuino::USART0.available()
                              ? static_cast<int>(MikroDuino::USART0.read()) : -1; }
};

extern HardwareSerial Serial;

// --------------------------------------------------------------------------
// Arduino compatibility initialiser
// --------------------------------------------------------------------------
namespace Arduino {
    // Call once in setup() to enable millis() / micros() via Timer0
    void beginTimekeeping();
}

// --------------------------------------------------------------------------
// ARDUINO_MAIN — optional macro that generates a main() from setup()/loop().
// Place it once in your .cpp file, outside any function.
//
//   #include <Arduino.h>
//   ARDUINO_MAIN()
//   void setup() { ... }
//   void loop()  { ... }
// --------------------------------------------------------------------------
#define ARDUINO_MAIN()      \
    extern void setup();    \
    extern void loop();     \
    int main() {            \
        setup();            \
        for (;;) loop();    \
        return 0;           \
    }

#endif // __cplusplus
