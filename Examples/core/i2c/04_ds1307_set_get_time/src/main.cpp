/*
 * I2C RTC — DS1307 Set/Get Time — MikroDuino SDK
 *
 * Project 4 of 6 in the examples/i2c series. Switches to a second I2C
 * device family — the DS1307 real-time clock chip — and, like the EEPROM
 * projects before it, talks to it using nothing but the generic
 * I2CDriver primitives (write/writeRead). There is no DS1307-specific
 * class in this file; the register map comes straight from the datasheet.
 *
 * (The SDK does ship a ready-made module driver for the sibling chip,
 * DS3231, at sdk/modules/DS3231/ — deliberately not used here. This
 * series' whole point is to show what that kind of driver does
 * internally, one I2C transaction at a time.)
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ Signal  │ Pin   │ Wiring                                    │
 *   ├─────────┼───────┼──────────────────────────────────────────┤
 *   │ SDA     │ PC4   │ DS1307 module SDA                         │
 *   │ SCL     │ PC5   │ DS1307 module SCL                         │
 *   │ TXD     │ PD1   │ USB-serial adapter                        │
 *   │ LED     │ PB5   │ Blinks once per readback                  │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * Most DS1307 breakout boards also carry a battery holder and an onboard
 * AT24C32 EEPROM (the same chip from projects 2-3) sharing the bus at
 * address 0x50 — project 6 puts both chips to use together.
 *
 * DS1307 register map (7-bit I2C address 0x68), all values BCD-encoded:
 *
 *   Reg   Field         Notes
 *   0x00  Seconds       bit 7 = CH (Clock Halt) — 1 stops the oscillator.
 *                        Must be 0 for the clock to run at all.
 *   0x01  Minutes
 *   0x02  Hours         bit 6 = 12/24-hour select. This example always
 *                        writes 0 here to force 24-hour mode.
 *   0x03  Day of week   1-7 (meaning is arbitrary — this example uses
 *                        1=Sunday, matching common convention)
 *   0x04  Date          1-31
 *   0x05  Month         1-12
 *   0x06  Year          00-99 (offset from 2000)
 *   0x07  Control       square-wave output config — used in project 5
 *
 * BCD (Binary-Coded Decimal): each byte packs two base-10 digits, one per
 * nibble. 37 seconds is stored as 0x37, not 37 (0x25) — the chip does
 * this internally so it can drive a 7-segment display directly without
 * any binary-to-decimal conversion in hardware. Software talking to it
 * has to do the conversion instead: decToBcd()/bcdToDec() below.
 *
 * I2C concepts reused from earlier projects:
 *   - I2C.write(addr, buf, len)     — writing the 7 clock registers
 *   - I2C.writeRead(addr, w,wl,r,rl) — pointing at register 0x00 then
 *     burst-reading all 7 registers in one transaction, exactly like the
 *     EEPROM's "write address, repeated start, read data" pattern in
 *     project 2 — the DS1307 auto-increments its internal register
 *     pointer on each clocked-out byte, just like the EEPROM does with
 *     its memory address.
 */

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/usart.hpp>
#include <mikroduino/i2c.hpp>

using namespace MikroDuino;

static constexpr uint8_t LED       = PB5;
static constexpr uint8_t DS1307_ADDR = 0x68;

static void delay_ms(uint16_t ms) { while (ms--) _delay_ms(1); }

static uint8_t bcdToDec(uint8_t bcd) { return static_cast<uint8_t>((bcd >> 4) * 10 + (bcd & 0x0F)); }
static uint8_t decToBcd(uint8_t dec) { return static_cast<uint8_t>(((dec / 10) << 4) | (dec % 10)); }

// Plain field-by-field time, in ordinary decimal — the BCD conversion
// happens only at the I2C boundary, so the rest of the program never
// has to think about it.
struct DateTime {
    uint8_t second;     // 0-59
    uint8_t minute;     // 0-59
    uint8_t hour;       // 0-23
    uint8_t dayOfWeek;  // 1-7 (1 = Sunday)
    uint8_t day;        // 1-31
    uint8_t month;      // 1-12
    uint8_t year;        // 0-99, offset from 2000
};

// Write all 7 clock registers in a single I2C transaction: START,
// address+W, register pointer 0x00, then 7 data bytes, STOP. The DS1307
// accepts a burst write across consecutive registers the same way the
// EEPROM accepts a burst write across consecutive memory addresses.
static bool ds1307Set(const DateTime& dt) {
    uint8_t packet[8] = {
        0x00,                              // starting register: Seconds
        decToBcd(dt.second),               // bit7 (CH) = 0 -> oscillator runs
        decToBcd(dt.minute),
        decToBcd(dt.hour),                 // bit6 = 0 -> 24-hour mode
        decToBcd(dt.dayOfWeek),
        decToBcd(dt.day),
        decToBcd(dt.month),
        decToBcd(dt.year)
    };
    return I2C.write(DS1307_ADDR, packet, sizeof(packet)) == I2CResult::Ok;
}

// Point the DS1307's register pointer at 0x00, then read all 7 registers
// back in one burst using a repeated start (writeRead) — same pattern as
// eepromReadByte()/eepromReadBuffer() from projects 2-3, just against a
// different device address and a 1-byte "address" (register number)
// instead of the EEPROM's 2-byte memory address.
static bool ds1307Get(DateTime& dt) {
    uint8_t reg = 0x00;
    uint8_t raw[7];
    if (I2C.writeRead(DS1307_ADDR, &reg, 1, raw, 7) != I2CResult::Ok) return false;

    dt.second    = bcdToDec(raw[0] & 0x7F);   // mask off CH bit
    dt.minute    = bcdToDec(raw[1]);
    dt.hour      = bcdToDec(raw[2] & 0x3F);   // mask off 12/24 + AM/PM bits
    dt.dayOfWeek = bcdToDec(raw[3]);
    dt.day       = bcdToDec(raw[4]);
    dt.month     = bcdToDec(raw[5]);
    dt.year      = bcdToDec(raw[6]);
    return true;
}

// True if the oscillator is halted (CH bit set) — the state a brand-new
// or battery-drained DS1307 powers up in. Reads just the Seconds register
// via a 1-byte writeRead, rather than the full 7-byte burst above.
static bool ds1307ClockHalted() {
    uint8_t reg = 0x00;
    uint8_t seconds = 0;
    if (I2C.writeRead(DS1307_ADDR, &reg, 1, &seconds, 1) != I2CResult::Ok) {
        return true;   // treat a bus error as "not safe to trust" too
    }
    return (seconds & 0x80) != 0;
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
    GPIO::inputPullup(PC4);
    GPIO::inputPullup(PC5);

    USART0.begin(9600);
    I2C.beginMaster(100000UL);

    USART0.writeLine_P(PSTR("I2C RTC: DS1307 set/get time"));
    USART0.writeLine_P(PSTR("============================="));

    // Only (re)set the clock if it's actually halted — a DS1307 with a
    // healthy backup battery keeps running (and keeps correct time)
    // across resets and even full power loss to the ATmega. Stomping on
    // it with a fixed compile-time value on every reset would defeat the
    // whole point of having a battery-backed RTC.
    if (ds1307ClockHalted()) {
        USART0.writeLine_P(PSTR("CH bit set (oscillator halted) -> initialising clock"));

        // An arbitrary starting date/time — edit to taste, or wire up a
        // button/serial command to set it interactively (project 6 reuses
        // this same ds1307Set() to timestamp logged events).
        DateTime startup = { 0, 0, 12, 2, 20, 7, 26 };   // Mon 2026-07-20 12:00:00
        bool ok = ds1307Set(startup);
        USART0.writeLine_P(ok ? PSTR("ds1307Set() -> OK") : PSTR("ds1307Set() -> FAILED (check wiring)"));
    } else {
        USART0.writeLine_P(PSTR("CH bit clear -> RTC already running, leaving it alone"));
    }
    USART0.writeLine_P(PSTR(""));

    while (true) {
        DateTime now{};
        if (ds1307Get(now)) {
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
            USART0.write_P(PSTR("   dayOfWeek="));
            USART0.writeInt(now.dayOfWeek);
            USART0.writeLine_P(PSTR(""));
        } else {
            USART0.writeLine_P(PSTR("ds1307Get() FAILED (no ACK — check wiring)"));
        }

        GPIO::set(LED);
        _delay_ms(50);
        GPIO::clear(LED);

        delay_ms(1000);   // the chip free-runs internally; polling once a
                          // second is enough to see the seconds field advance
    }
}
