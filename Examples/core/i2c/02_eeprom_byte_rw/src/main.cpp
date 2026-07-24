/*
 * I2C EEPROM — Single-Byte Read/Write — MikroDuino SDK
 *
 * Project 2 of 6 in the examples/i2c series. Talks to an external I2C
 * EEPROM chip (AT24C32 family — the same chip found on the back of most
 * DS1307/DS3231 RTC breakout modules) using nothing but I2CDriver::write()
 * and I2CDriver::read(). No EEPROM-specific driver class exists in the SDK
 * for this chip on purpose: this whole series builds the protocol by hand
 * from the datasheet, using only the generic I2C master primitives.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ Signal  │ Pin   │ Wiring                                    │
 *   ├─────────┼───────┼──────────────────────────────────────────┤
 *   │ SDA     │ PC4   │ AT24C32 pin 5 (SDA)                       │
 *   │ SCL     │ PC5   │ AT24C32 pin 6 (SCL)                       │
 *   │ —       │ —     │ AT24C32 pins A0/A1/A2 -> GND (address 0x50)│
 *   │ —       │ —     │ AT24C32 pin WP -> GND (write enabled)     │
 *   │ TXD     │ PD1   │ USB-serial adapter                        │
 *   │ LED     │ PB5   │ Pulses on each write cycle                │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * AT24C32 essentials used throughout this series:
 *   - 4096 bytes total, organised as 128 pages of 32 bytes each.
 *   - 7-bit I2C address is 0b1010(A2)(A1)(A0) — with all three address
 *     pins grounded that's 0x50, the value nearly every breakout ships
 *     wired for.
 *   - The chip's internal 12-bit memory address is sent as TWO bytes
 *     (high byte first) immediately after the device address — this is
 *     different from the ATmega's own on-chip EEPROM, which the SDK's
 *     EEPROMDriver (mikroduino/eeprom.hpp) talks to over a completely
 *     different, register-based interface, not I2C at all. Don't confuse
 *     the two: EEPROMDriver == internal AVR EEPROM, this file == external
 *     I2C EEPROM chip.
 *   - After any write, the chip needs up to ~10 ms internally to actually
 *     commit the byte to non-volatile memory. During that window it will
 *     NACK its own address if you try to start a new transaction — the
 *     datasheet calls this "ACK polling" and it's the standard, fast way
 *     to know a write has finished (much faster than a blind delay).
 *
 * I2C concepts introduced:
 *   - I2C.write(addr, buf, len) — a single WRITE transaction: START,
 *     address+W, then `len` data bytes, then STOP. For a "byte write" to
 *     the EEPROM, buf holds {addrHigh, addrLow, dataByte} — the memory
 *     address and the payload go out as one contiguous write, exactly as
 *     the AT24Cxx datasheet specifies.
 *   - I2C.read(addr, buf, len) — a single READ transaction: START,
 *     address+R, then `len` data bytes (each ACKed except the last), then
 *     STOP. Used here only after a writeRead() has already pointed the
 *     EEPROM's internal address pointer at the byte we want (see below).
 *   - I2CResult — every I2C call returns this. NackAddress after a write
 *     is expected and normal during the ACK-poll wait; any other error
 *     (Timeout, BusError, ArbitrationLost) means the wiring or bus itself
 *     has a problem worth checking with project 1's bus scanner.
 */

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/usart.hpp>
#include <mikroduino/i2c.hpp>

using namespace MikroDuino;

static constexpr uint8_t LED = PB5;

// AT24C32 7-bit I2C address with all address pins (A0-A2) tied to GND.
static constexpr uint8_t EEPROM_ADDR = 0x50;

static void delay_ms(uint16_t ms) { while (ms--) _delay_ms(1); }

// Write one byte to `memAddr` and block until the internal write cycle
// finishes, using ACK polling instead of a fixed delay.
//
// The write itself is a single I2C.write() carrying three bytes: the
// 12-bit memory address split into high/low, followed by the data byte.
// After that, the chip goes "busy" internally and stops ACKing its own
// address — we just keep trying a zero-length write (START + address +
// STOP, no data) until it ACKs again, which is the moment the byte is
// safely committed to non-volatile memory.
static bool eepromWriteByte(uint16_t memAddr, uint8_t value) {
    uint8_t packet[3] = {
        static_cast<uint8_t>(memAddr >> 8),    // address high byte
        static_cast<uint8_t>(memAddr & 0xFF),  // address low byte
        value                                   // data byte
    };

    if (I2C.write(EEPROM_ADDR, packet, 3) != I2CResult::Ok) {
        return false;   // bus-level failure — device not present / wiring fault
    }

    // ACK poll: a zero-length write is just START + SLA+W + STOP. The
    // AT24C32 NACKs the address byte while its write cycle is still in
    // progress, and ACKs again the instant it's ready. This typically
    // resolves in 1-2 attempts (~5 ms) rather than needing a worst-case
    // 10 ms blind delay every time.
    for (uint16_t attempts = 0; attempts < 200; ++attempts) {
        if (I2C.write(EEPROM_ADDR, nullptr, 0) == I2CResult::Ok) {
            return true;
        }
        _delay_us(50);
    }
    return false;   // gave up after ~10 ms — chip not responding
}

// Read one byte from `memAddr`.
//
// This needs TWO I2C phases even though it's "just a read": first the
// 2-byte memory address must be written into the EEPROM's internal
// address pointer (no data yet, no STOP — a repeated START follows),
// then a read transaction pulls back the byte the pointer now points at.
// I2CDriver::writeRead() does exactly this: write(..., stop=false) then
// read(..., stop=true), joined by a repeated START rather than a full
// STOP/START pair. Repeated start matters here — some I2C devices release
// the bus or lose their place if another master intervenes between a
// full STOP and the next START; the EEPROM in particular requires the
// address-then-read to happen as one atomic transaction.
static bool eepromReadByte(uint16_t memAddr, uint8_t& outValue) {
    uint8_t addrBytes[2] = {
        static_cast<uint8_t>(memAddr >> 8),
        static_cast<uint8_t>(memAddr & 0xFF)
    };
    return I2C.writeRead(EEPROM_ADDR, addrBytes, 2, &outValue, 1) == I2CResult::Ok;
}

static void print_hex8(uint8_t v) {
    char buf[3];
    buf[0] = "0123456789ABCDEF"[v >> 4];
    buf[1] = "0123456789ABCDEF"[v & 0x0F];
    buf[2] = '\0';
    USART0.write(buf);
}

int main() {
    GPIO::output(LED);
    GPIO::inputPullup(PC4);
    GPIO::inputPullup(PC5);

    USART0.begin(9600);
    I2C.beginMaster(100000UL);

    USART0.writeLine_P(PSTR("I2C EEPROM: single-byte read/write"));
    USART0.writeLine_P(PSTR("==================================="));

    static constexpr uint16_t TEST_ADDR = 0x0000;
    uint8_t counter = 0;

    while (true) {
        // Write the counter value, then immediately read it back to
        // confirm round-trip correctness.
        USART0.write_P(PSTR("write(0x0000, "));
        print_hex8(counter);
        USART0.write_P(PSTR(") ... "));

        GPIO::set(LED);
        bool wroteOk = eepromWriteByte(TEST_ADDR, counter);
        GPIO::clear(LED);

        if (!wroteOk) {
            USART0.writeLine_P(PSTR("FAILED (no ACK — check wiring)"));
            delay_ms(1000);
            continue;
        }
        USART0.writeLine_P(PSTR("done"));

        uint8_t readBack = 0;
        USART0.write_P(PSTR("read(0x0000)  = "));
        if (eepromReadByte(TEST_ADDR, readBack)) {
            print_hex8(readBack);
            USART0.write_P(PSTR("  "));
            USART0.writeLine_P(readBack == counter ? PSTR("[PASS]") : PSTR("[MISMATCH]"));
        } else {
            USART0.writeLine_P(PSTR("FAILED (no ACK)"));
        }

        USART0.writeLine_P(PSTR(""));
        ++counter;   // next pass writes a different value, proving it isn't stale data
        delay_ms(1500);
    }
}
