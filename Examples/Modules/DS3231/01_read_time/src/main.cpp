/*
 * DS3231 Basics — begin(), now(), isRunning() — MikroDuino Module SDK
 *
 * The simplest possible use of the DS3231 module: read the current time
 * off the RTC once a second and print it over USART. This is project 1
 * of 6 in the examples/Modules/DS3231 series, which walks the DS3231
 * class from a single now() up to a capstone clock/alarm dashboard
 * combining DS3231, LCD, Button, and EEPROM.
 *
 * Related series: examples/i2c/04-06 already build a DS1307 real-time
 * clock entirely out of raw I2CDriver calls, on purpose, to show what a
 * module driver like this one does internally. This series is the
 * other half of that story — the same chip family (DS3231 is a
 * drop-in-compatible, temperature-compensated successor to the DS1307)
 * used through its ready-made DS3231 class instead.
 *
 * Hardware (ATmega328P @ 16 MHz, DS3231 RTC breakout — most boards also
 * carry a CR2032 battery holder that keeps the clock running through a
 * power loss):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ Signal  │ Pin   │ Wiring                                    │
 *   ├─────────┼───────┼──────────────────────────────────────────┤
 *   │ SDA     │ PC4   │ I2C data — shared bus, address 0x68        │
 *   │ SCL     │ PC5   │ I2C clock                                  │
 *   │ VCC     │ —     │ 3.3-5.5V                                   │
 *   │ GND     │ —     │ Ground                                     │
 *   │ TXD     │ PD1   │ USB-serial adapter, 9600 8N1               │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * DS3231 concepts introduced:
 *   - DS3231(i2c = I2C) — the constructor takes an I2CDriver& and
 *     defaults to the SDK's global I2C singleton, so `DS3231 rtc;` with
 *     no arguments is the normal case; the parameter exists mainly so a
 *     project with more than one I2C bus could pass a different one.
 *     Nothing on the wire happens yet — the object just records which
 *     bus to use.
 *   - begin() — reads the chip's status register and clears the OSF
 *     (Oscillator Stop Flag) if it's set. OSF gets set by the DS3231
 *     itself whenever its oscillator has been stopped for any reason
 *     (first power-up with no battery yet, or a fully drained backup
 *     battery) — it's the chip's own "the time I'm about to report
 *     might not be trustworthy" flag. IMPORTANT: begin() does NOT call
 *     I2C.beginMaster() — the I2C bus itself must already be
 *     initialised before begin() (or any other DS3231 method) is
 *     called, exactly as the raw-I2C DS1307 examples require.
 *   - isRunning() — returns whether OSF is currently clear. This
 *     project checks it BEFORE calling begin() specifically to learn
 *     whether the oscillator HAD been stopped — begin() would otherwise
 *     silently clear that evidence before it could be reported.
 *   - now() — reads all 7 clock registers in one I2C burst and returns
 *     a DateTime{second, minute, hour, dayOfWeek, day, month, year} —
 *     the same field layout and 1=Sunday convention the raw-I2C DS1307
 *     examples use, just returned as a ready-made struct instead of
 *     something this project's own code would have to decode from BCD.
 */

#include <util/delay.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/usart.hpp>
#include <mikroduino/i2c.hpp>
#include <DS3231.hpp>

using namespace MikroDuino;

DS3231 rtc;   // uses the global I2C bus (mikroduino/i2c.hpp)

static void printField2(uint8_t v) {
    USART0.write(static_cast<char>('0' + (v / 10)));
    USART0.write(static_cast<char>('0' + (v % 10)));
}

int main() {
    // TWI pins also work with the internal pull-ups as a fallback if a
    // board's external pull-up resistors are missing or too weak —
    // harmless to enable even when they're not needed.
    GPIO::inputPullup(PC4);
    GPIO::inputPullup(PC5);

    USART0.begin(9600);
    USART0.writeLine_P(PSTR("DS3231 basic time read"));
    USART0.writeLine_P(PSTR("========================"));

    I2C.beginMaster(100000UL);   // must happen before any DS3231 call

    bool wasHalted = !rtc.isRunning();   // check BEFORE begin() clears OSF
    rtc.begin();

    if (wasHalted) {
        USART0.writeLine_P(PSTR("WARNING: oscillator was stopped - time below is not trustworthy"));
        USART0.writeLine_P(PSTR("until a real date/time is written with set() (see project 2)."));
    } else {
        USART0.writeLine_P(PSTR("Oscillator already running - clock has been keeping time."));
    }
    USART0.writeLine_P(PSTR(""));

    while (true) {
        DateTime now = rtc.now();

        USART0.write_P(PSTR("20"));
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

        _delay_ms(1000);   // the RTC free-runs internally; polling once a
                            // second is enough to see the seconds field advance
    }
}
