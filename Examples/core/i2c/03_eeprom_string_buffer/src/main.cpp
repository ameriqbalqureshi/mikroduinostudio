/*
 * I2C EEPROM — Page-Safe Buffer Read/Write — MikroDuino SDK
 *
 * Project 3 of 6 in the examples/i2c series. Extends project 2's
 * single-byte read/write to arbitrary-length buffers (strings, structs,
 * anything), and introduces the one AT24Cxx quirk that trips up almost
 * everyone the first time: PAGE WRITE WRAPAROUND.
 *
 * Hardware: identical to project 2 — AT24C32 EEPROM on SDA=PC4/SCL=PC5,
 * address 0x50, USART0 at 9600 8N1 on PD1/PD0, LED on PB5.
 *
 * The page wraparound problem:
 *   The AT24C32 buffers an entire I2C write internally before committing
 *   it to non-volatile memory, but that internal buffer is only ONE PAGE
 *   (32 bytes) wide, aligned to 32-byte boundaries. If a single I2C write
 *   transaction supplies more bytes than fit from the current address to
 *   the end of that page, the chip does NOT spill into the next page —
 *   the internal buffer's address pointer silently WRAPS BACK to the
 *   start of the *same* page, and those extra bytes overwrite data at the
 *   beginning of the page instead of continuing where you'd expect.
 *
 *   Example: writing 10 bytes starting at address 0x001C (28) on a
 *   32-byte-paged chip. The page containing 0x001C runs from 0x0018 to
 *   0x001F — only 4 bytes remain in that page. Bytes 5-10 of your write
 *   wrap around to 0x0018, clobbering the first 4 bytes you meant to
 *   leave alone. The fix is simple: split any write that would cross a
 *   page boundary into multiple page-aligned I2C.write() calls, ACK-poll
 *   after each one (writes cannot be pipelined — the chip is busy
 *   committing the previous page while you'd want to send the next).
 *
 *   READS have no such limit: I2C.read() / I2C.writeRead() can pull back
 *   an arbitrary number of sequential bytes in one transaction, because
 *   reading doesn't go through that small internal write buffer — the
 *   chip just keeps incrementing its address pointer and clocking out
 *   whatever byte is there, wrapping only at the very end of the whole
 *   4096-byte array.
 *
 * I2C concepts reused from project 2:
 *   - I2C.write(addr, buf, len), I2C.writeRead(addr, wbuf, wlen, rbuf, rlen)
 *   - ACK polling after each page write to wait out the internal write cycle
 *
 * No new I2CDriver methods are introduced here — this project is about
 * correctly *sequencing* the same primitives, which matters just as much
 * as knowing the API itself.
 */

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <string.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/usart.hpp>
#include <mikroduino/i2c.hpp>

using namespace MikroDuino;

static constexpr uint8_t  LED         = PB5;
static constexpr uint8_t  EEPROM_ADDR = 0x50;
static constexpr uint16_t PAGE_SIZE   = 32;   // AT24C32 page size in bytes

static void delay_ms(uint16_t ms) { while (ms--) _delay_ms(1); }

// Block until the EEPROM ACKs its own address again after a write —
// see project 2 for the full explanation of ACK polling.
static bool eepromWaitReady() {
    for (uint16_t attempts = 0; attempts < 200; ++attempts) {
        if (I2C.write(EEPROM_ADDR, nullptr, 0) == I2CResult::Ok) return true;
        _delay_us(50);
    }
    return false;
}

// Write ONE page-safe chunk: at most (PAGE_SIZE - offset-within-page)
// bytes, all falling within a single 32-byte page. Callers must never
// pass more than that, which is exactly what eepromWriteBuffer() below
// guarantees by construction.
static bool eepromWritePage(uint16_t memAddr, const uint8_t* data, uint8_t len) {
    uint8_t packet[2 + PAGE_SIZE];
    packet[0] = static_cast<uint8_t>(memAddr >> 8);
    packet[1] = static_cast<uint8_t>(memAddr & 0xFF);
    memcpy(&packet[2], data, len);

    if (I2C.write(EEPROM_ADDR, packet, static_cast<uint8_t>(2 + len)) != I2CResult::Ok)
        return false;

    return eepromWaitReady();
}

// Write an arbitrary-length buffer, automatically splitting at page
// boundaries so no single I2C.write() ever crosses one. This is the
// function to reach for whenever storing more than one byte.
static bool eepromWriteBuffer(uint16_t memAddr, const uint8_t* data, uint16_t len) {
    while (len > 0) {
        uint16_t offsetInPage = memAddr % PAGE_SIZE;
        uint16_t roomInPage   = PAGE_SIZE - offsetInPage;
        uint8_t  chunk        = static_cast<uint8_t>(len < roomInPage ? len : roomInPage);

        if (!eepromWritePage(memAddr, data, chunk)) return false;

        memAddr += chunk;
        data    += chunk;
        len     -= chunk;
    }
    return true;
}

// Sequential read — no page restriction, so this is a single I2C
// transaction regardless of length (see the header comment above).
static bool eepromReadBuffer(uint16_t memAddr, uint8_t* out, uint16_t len) {
    uint8_t addrBytes[2] = {
        static_cast<uint8_t>(memAddr >> 8),
        static_cast<uint8_t>(memAddr & 0xFF)
    };
    return I2C.writeRead(EEPROM_ADDR, addrBytes, 2, out, len) == I2CResult::Ok;
}

// Runs both demos once. Called in a loop from main() so the walkthrough
// is visible even if the terminal connects late, without resorting to
// recursive calls into main() (which would grow the call stack forever).
static void runDemos() {
    // ---- Demo 1: a short string, well within a single page ----
    {
        static const char MSG[] = "MikroDuino!";   // 12 bytes incl. NUL
        static constexpr uint16_t ADDR = 0x0000;

        USART0.writeLine_P(PSTR("Demo 1: short string (single page)"));
        GPIO::set(LED);
        bool ok = eepromWriteBuffer(ADDR, reinterpret_cast<const uint8_t*>(MSG), sizeof(MSG));
        GPIO::clear(LED);
        USART0.write_P(PSTR("  wrote \"")); USART0.write(MSG); USART0.write_P(PSTR("\" -> "));
        USART0.writeLine_P(ok ? PSTR("OK") : PSTR("FAILED"));

        char readback[sizeof(MSG)];
        eepromReadBuffer(ADDR, reinterpret_cast<uint8_t*>(readback), sizeof(MSG));
        USART0.write_P(PSTR("  read back \"")); USART0.write(readback); USART0.write_P(PSTR("\"  "));
        USART0.writeLine_P(strcmp(readback, MSG) == 0 ? PSTR("[PASS]") : PSTR("[MISMATCH]"));
    }

    USART0.writeLine_P(PSTR(""));

    // ---- Demo 2: a buffer that deliberately straddles a page boundary ----
    // Starting at 0x001C (28), only 4 bytes remain before the 32-byte page
    // boundary at 0x0020. Without the splitting logic in
    // eepromWriteBuffer(), this 40-byte write would wrap and corrupt the
    // first 4 bytes of the page instead of spilling into the next one.
    {
        static constexpr uint16_t ADDR = 0x001C;
        static uint8_t payload[40];
        for (uint8_t i = 0; i < 40; ++i) payload[i] = static_cast<uint8_t>('A' + (i % 26));

        USART0.writeLine_P(PSTR("Demo 2: 40-byte buffer straddling a page boundary at 0x001C"));
        GPIO::set(LED);
        bool ok = eepromWriteBuffer(ADDR, payload, sizeof(payload));
        GPIO::clear(LED);
        USART0.write_P(PSTR("  eepromWriteBuffer() split across pages -> "));
        USART0.writeLine_P(ok ? PSTR("OK") : PSTR("FAILED"));

        uint8_t readback[40];
        eepromReadBuffer(ADDR, readback, sizeof(readback));
        bool match = true;
        for (uint8_t i = 0; i < 40 && match; ++i) match = (readback[i] == payload[i]);
        USART0.write_P(PSTR("  read back 40 bytes  "));
        USART0.writeLine_P(match ? PSTR("[PASS]") : PSTR("[MISMATCH - wraparound bug!]"));
    }

    USART0.writeLine_P(PSTR(""));
    USART0.writeLine_P(PSTR("Demo complete. Repeating in 5 s..."));
}

int main() {
    GPIO::output(LED);
    GPIO::inputPullup(PC4);
    GPIO::inputPullup(PC5);

    USART0.begin(9600);
    I2C.beginMaster(100000UL);

    USART0.writeLine_P(PSTR("I2C EEPROM: page-safe buffer read/write"));
    USART0.writeLine_P(PSTR("========================================"));
    USART0.writeLine_P(PSTR(""));

    while (true) {
        runDemos();
        delay_ms(5000);
    }
}
