/*
 * I2C RTC — DS1307 Live Clock + Square-Wave Control — MikroDuino SDK
 *
 * Project 5 of 6 in the examples/i2c series. Builds on project 4's
 * set/get functions with two additions:
 *
 *   1. A properly formatted, continuously-updating clock readout, with
 *      weekday names pulled from a flash (PROGMEM) table instead of a
 *      bare integer.
 *   2. Writing to the DS1307's Control register (0x07) — the first
 *      example in this series that modifies device *configuration*
 *      rather than the EEPROM's data or the RTC's date/time fields. A
 *      push button toggles the chip's square-wave output on and off,
 *      demonstrating a plain 2-byte I2C.write() (register pointer + one
 *      data byte) to a single register, as opposed to the 7-register
 *      burst write ds1307Set() used for the clock fields.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ Signal  │ Pin   │ Wiring                                    │
 *   ├─────────┼───────┼──────────────────────────────────────────┤
 *   │ SDA     │ PC4   │ DS1307 module SDA                         │
 *   │ SCL     │ PC5   │ DS1307 module SCL                         │
 *   │ TXD     │ PD1   │ USB-serial adapter                        │
 *   │ LED     │ PB5   │ On for as long as SQW output is enabled   │
 *   │ Button  │ PD2   │ Other leg to GND; internal pull-up used — │
 *   │         │       │ press = logic low                         │
 *   │ SQW     │ —     │ DS1307 module's SQW pin, optional: probe  │
 *   │         │       │ with a scope/logic analyser to see the    │
 *   │         │       │ 1 Hz square wave while it's enabled       │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * DS1307 Control register (0x07) bits used here:
 *   bit 4 (SQWE) — Square Wave Enable. 1 = SQW pin outputs a square wave
 *                  at the rate selected by RS1:RS0; 0 = SQW pin instead
 *                  reflects the OUT bit (bit 7) as a static level.
 *   bits 1:0 (RS1:RS0) — rate select. 00 = 1 Hz, 01 = 4.096 kHz,
 *                  10 = 8.192 kHz, 11 = 32.768 kHz. This example always
 *                  uses 00 (1 Hz) — slow enough to see blink an LED with,
 *                  if you wire SQW to a spare input pin.
 *
 * I2C concepts reused from project 4:
 *   - ds1307Set()/ds1307Get() — the same 7-register burst write/read.
 *
 * I2C concept introduced:
 *   - A single-register write via I2C.write(addr, {reg, value}, 2) — the
 *     minimal case of the same "register pointer followed by data" packet
 *     shape used everywhere else in this series, here carrying just one
 *     data byte instead of seven (RTC fields) or up to a page (EEPROM).
 */

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/usart.hpp>
#include <mikroduino/i2c.hpp>

using namespace MikroDuino;

static constexpr uint8_t LED         = PB5;
static constexpr uint8_t BUTTON      = PD2;
static constexpr uint8_t DS1307_ADDR = 0x68;
static constexpr uint8_t REG_CONTROL = 0x07;

static void delay_ms(uint16_t ms) { while (ms--) _delay_ms(1); }

static uint8_t bcdToDec(uint8_t bcd) { return static_cast<uint8_t>((bcd >> 4) * 10 + (bcd & 0x0F)); }
static uint8_t decToBcd(uint8_t dec) { return static_cast<uint8_t>(((dec / 10) << 4) | (dec % 10)); }

struct DateTime {
    uint8_t second, minute, hour, dayOfWeek, day, month, year;
};

static bool ds1307Set(const DateTime& dt) {
    uint8_t packet[8] = {
        0x00,
        decToBcd(dt.second), decToBcd(dt.minute), decToBcd(dt.hour),
        decToBcd(dt.dayOfWeek), decToBcd(dt.day), decToBcd(dt.month), decToBcd(dt.year)
    };
    return I2C.write(DS1307_ADDR, packet, sizeof(packet)) == I2CResult::Ok;
}

static bool ds1307Get(DateTime& dt) {
    uint8_t reg = 0x00;
    uint8_t raw[7];
    if (I2C.writeRead(DS1307_ADDR, &reg, 1, raw, 7) != I2CResult::Ok) return false;
    dt.second    = bcdToDec(raw[0] & 0x7F);
    dt.minute    = bcdToDec(raw[1]);
    dt.hour      = bcdToDec(raw[2] & 0x3F);
    dt.dayOfWeek = bcdToDec(raw[3]);
    dt.day       = bcdToDec(raw[4]);
    dt.month     = bcdToDec(raw[5]);
    dt.year      = bcdToDec(raw[6]);
    return true;
}

static bool ds1307ClockHalted() {
    uint8_t reg = 0x00, seconds = 0;
    if (I2C.writeRead(DS1307_ADDR, &reg, 1, &seconds, 1) != I2CResult::Ok) return true;
    return (seconds & 0x80) != 0;
}

// Write the Control register directly — a 2-byte I2C.write(): the
// register pointer (0x07) followed by the one value byte. No burst, no
// repeated start, just the smallest possible register write.
static bool ds1307SetControl(uint8_t value) {
    uint8_t packet[2] = { REG_CONTROL, value };
    return I2C.write(DS1307_ADDR, packet, 2) == I2CResult::Ok;
}

// Weekday names live in flash, not SRAM — the ATmega328P has only 2 KB of
// the latter, and a 7-entry string table has no business spending any of
// it. Index 1 = Sunday, matching the dayOfWeek convention used in
// ds1307Set()/ds1307Get() (arbitrary but consistent 1-7 mapping — the
// chip itself doesn't assign meaning to this field beyond "counts 1-7 and
// wraps").
static const char DAY0[] PROGMEM = "Sun";
static const char DAY1[] PROGMEM = "Mon";
static const char DAY2[] PROGMEM = "Tue";
static const char DAY3[] PROGMEM = "Wed";
static const char DAY4[] PROGMEM = "Thu";
static const char DAY5[] PROGMEM = "Fri";
static const char DAY6[] PROGMEM = "Sat";
static const char* const DAY_NAMES[7] PROGMEM = { DAY0, DAY1, DAY2, DAY3, DAY4, DAY5, DAY6 };

static void printDayName(uint8_t dayOfWeek1to7) {
    const char* p = reinterpret_cast<const char*>(pgm_read_word(&DAY_NAMES[(dayOfWeek1to7 - 1) % 7]));
    USART0.write_P(p);
}

static void printField2(uint8_t v) {
    char buf[3];
    buf[0] = static_cast<char>('0' + (v / 10));
    buf[1] = static_cast<char>('0' + (v % 10));
    buf[2] = '\0';
    USART0.write(buf);
}

int main() {
    GPIO::output(LED);
    GPIO::clear(LED);
    GPIO::inputPullup(BUTTON);
    GPIO::inputPullup(PC4);
    GPIO::inputPullup(PC5);

    USART0.begin(9600);
    I2C.beginMaster(100000UL);

    USART0.writeLine_P(PSTR("I2C RTC: DS1307 live clock + SQW control"));
    USART0.writeLine_P(PSTR("========================================="));
    USART0.writeLine_P(PSTR("Press the button to toggle 1 Hz square-wave output."));
    USART0.writeLine_P(PSTR(""));

    if (ds1307ClockHalted()) {
        DateTime startup = { 0, 0, 12, 2, 20, 7, 26 };   // Mon 2026-07-20 12:00:00
        ds1307Set(startup);
        USART0.writeLine_P(PSTR("Oscillator was halted -> initialised to a default time."));
    }

    // Start with the square wave disabled (SQWE=0, RS1:RS0=00).
    bool sqwEnabled = false;
    ds1307SetControl(0x00);

    bool lastButtonState = true;   // pulled up -> idle = true (not pressed)

    while (true) {
        // Simple polled debounce: act only on the falling edge, then wait
        // out the mechanical bounce with a short fixed delay. Good enough
        // for a single manually-operated button; sdk/utility/sched has a
        // proper Debounce class for anything more demanding.
        bool pressed = (GPIO::read(BUTTON) == false);
        if (pressed && lastButtonState) {
            delay_ms(30);   // debounce settle
            if (GPIO::read(BUTTON) == false) {
                sqwEnabled = !sqwEnabled;
                // SQWE lives at bit 4; RS1:RS0 = 00 selects the 1 Hz rate.
                uint8_t controlValue = sqwEnabled ? (1 << 4) : 0x00;
                bool ok = ds1307SetControl(controlValue);
                GPIO::write(LED, sqwEnabled);

                USART0.write_P(PSTR(">> SQW output "));
                USART0.write_P(sqwEnabled ? PSTR("ENABLED (1 Hz)") : PSTR("DISABLED"));
                USART0.writeLine_P(ok ? PSTR(" [OK]") : PSTR(" [WRITE FAILED]"));
            }
        }
        lastButtonState = !pressed;

        static uint16_t tick = 0;
        if (++tick >= 100) {   // ~1 second at the 10 ms loop delay below
            tick = 0;

            DateTime now{};
            if (ds1307Get(now)) {
                printDayName(now.dayOfWeek);
                USART0.write_P(PSTR(" 20"));
                printField2(now.year);
                USART0.write_P(PSTR("-"));
                printField2(now.month);
                USART0.write_P(PSTR("-"));
                printField2(now.day);
                USART0.write_P(PSTR("  "));
                printField2(now.hour);
                USART0.write_P(PSTR(":"));
                printField2(now.minute);
                USART0.write_P(PSTR(":"));
                printField2(now.second);
                USART0.writeLine_P(PSTR(""));
            } else {
                USART0.writeLine_P(PSTR("ds1307Get() FAILED (no ACK — check wiring)"));
            }
        }

        delay_ms(10);   // keeps the button responsive between second-ticks
    }
}
