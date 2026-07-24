/*
 * MAX7219 8x8 LED Matrix — Raw SPI Register Protocol — MikroDuino SDK
 *
 * Project 4 of 6 in the examples/spi series. Switches to a second SPI
 * device family — the MAX7219 LED display driver — and, like the shift
 * register projects before it, talks to it using nothing but
 * SPIDriver::transfer(). There is no MAX7219-specific class in this file
 * on purpose: the SDK does ship a full-featured module driver for this
 * exact chip at sdk/modules/MAX72xx/, but this series' whole point is to
 * build the protocol by hand from the datasheet so you can see what a
 * driver like that is doing internally, one register write at a time.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ Signal  │ Pin   │ Wiring                                    │
 *   ├─────────┼───────┼──────────────────────────────────────────┤
 *   │ MOSI    │ PB3   │ MAX7219 DIN                               │
 *   │ SCK     │ PB5   │ MAX7219 CLK                                │
 *   │ CS      │ PB1   │ MAX7219 LOAD (a.k.a. CS)                  │
 *   │ TXD     │ PD1   │ USB-serial adapter                        │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * A different CS pin (PB1) is used here than the shift-register projects'
 * PB2, so that project 6's capstone can put both devices on the same
 * physical bus (shared MOSI/SCK) with independent chip selects, exactly
 * as real multi-device SPI systems are wired.
 *
 * MAX7219 protocol essentials:
 *   Every write is a 16-bit frame: an 8-bit REGISTER ADDRESS followed by
 *   an 8-bit DATA byte, both clocked in MSB-first while CS (LOAD) is
 *   held low, then latched into the chip on the rising edge of CS — the
 *   same "assert CS, clock bytes, deassert CS to latch" shape used by
 *   the 74HC595 in projects 1-3, just with two bytes per frame instead
 *   of one.
 *
 *   Register map used in this project:
 *     0x01-0x08  Digit0-Digit7  — the 8 rows of the matrix. In "no
 *                decode" mode (used here, not the BCD 7-segment mode)
 *                each bit of a digit register lights one column of that
 *                row: bit 7 = leftmost column, bit 0 = rightmost.
 *     0x09       Decode Mode    — 0x00 = no decode (raw bit-per-column,
 *                what a dot matrix needs); 0xFF would decode digits as
 *                BCD 7-segment characters instead, meant for numeric
 *                displays, not this chip.
 *     0x0A       Intensity      — brightness, 0x00 (dimmest) to 0x0F
 *                (brightest).
 *     0x0B       Scan Limit     — how many of the 8 digit rows are
 *                actually driven. Must be 0x07 (all 8) for a full 8x8
 *                matrix; a lower value would dim/disable the last rows.
 *     0x0C       Shutdown       — 0x00 = shutdown (outputs off, low
 *                power), 0x01 = normal operation. Chips power up in
 *                shutdown mode, so this must be set before anything else
 *                will light up.
 *     0x0D       Display Test   — 0x01 forces every LED on regardless of
 *                digit register contents (useful to verify wiring);
 *                0x00 = normal operation.
 *
 * SPI concepts reused from projects 1-3:
 *   - SPI.beginMaster(), SPI.transfer(uint8_t) — a register frame is
 *     just two back-to-back transfer() calls between one CS low/high
 *     pair, exactly like the daisy-chain project's two-byte sends.
 */

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/usart.hpp>
#include <mikroduino/spi.hpp>

using namespace MikroDuino;

static constexpr uint8_t CS = PB1;   // MAX7219 LOAD

static MD_INLINE void cs_low()  { GPIO::clear(CS); }
static MD_INLINE void cs_high() { GPIO::set(CS);   }

// MAX7219 register addresses (see header comment for the full map).
namespace MAX7219Reg {
    constexpr uint8_t NoOp       = 0x00;
    constexpr uint8_t Digit0     = 0x01;   // Digit0..Digit7 = 0x01..0x08
    constexpr uint8_t DecodeMode = 0x09;
    constexpr uint8_t Intensity  = 0x0A;
    constexpr uint8_t ScanLimit  = 0x0B;
    constexpr uint8_t Shutdown   = 0x0C;
    constexpr uint8_t DisplayTest = 0x0D;
}

static void delay_ms(uint16_t ms) { while (ms--) _delay_ms(1); }

// Send one 16-bit register frame: address byte then data byte, both
// under a single CS assertion so the MAX7219 latches them together.
static void max7219Write(uint8_t reg, uint8_t data) {
    cs_low();
    SPI.transfer(reg);
    SPI.transfer(data);
    cs_high();   // rising edge on LOAD latches the frame into the chip
}

// Write all 8 row registers from an 8-byte bitmap in one pass.
static void max7219DrawBitmap(const uint8_t rows[8]) {
    for (uint8_t row = 0; row < 8; ++row) {
        max7219Write(static_cast<uint8_t>(MAX7219Reg::Digit0 + row), rows[row]);
    }
}

static void max7219Clear() {
    for (uint8_t row = 0; row < 8; ++row) {
        max7219Write(static_cast<uint8_t>(MAX7219Reg::Digit0 + row), 0x00);
    }
}

// Bring the chip out of its power-on state into a known, usable
// configuration. Every project in this series that talks to the MAX7219
// starts with this same sequence.
static void max7219Init() {
    max7219Write(MAX7219Reg::Shutdown, 0x00);      // hold in shutdown while configuring
    max7219Write(MAX7219Reg::DisplayTest, 0x00);   // ensure test mode is off
    max7219Write(MAX7219Reg::DecodeMode, 0x00);    // no decode -> raw column bits
    max7219Write(MAX7219Reg::ScanLimit, 0x07);     // drive all 8 rows
    max7219Write(MAX7219Reg::Intensity, 0x08);     // mid brightness (0x00-0x0F)
    max7219Clear();
    max7219Write(MAX7219Reg::Shutdown, 0x01);      // leave shutdown -> normal operation
}

// Bitmaps: one byte per row, bit 7 = leftmost column, bit 0 = rightmost.
static const uint8_t BITMAP_HEART[8] PROGMEM = {
    0b01100110,
    0b11111111,
    0b11111111,
    0b11111111,
    0b01111110,
    0b00111100,
    0b00011000,
    0b00000000,
};
static const uint8_t BITMAP_SMILEY[8] PROGMEM = {
    0b00111100,
    0b01000010,
    0b10100101,
    0b10000001,
    0b10100101,
    0b10011001,
    0b01000010,
    0b00111100,
};
static const uint8_t BITMAP_ARROW_UP[8] PROGMEM = {
    0b00011000,
    0b00111100,
    0b01111110,
    0b11011011,
    0b00011000,
    0b00011000,
    0b00011000,
    0b00011000,
};

// Bitmaps above live in flash (PROGMEM); max7219DrawBitmap() needs a
// plain SRAM array to index with a normal loop, so copy the 8 bytes out
// with pgm_read_byte() first — the same flash-vs-RAM distinction project
// 1 of the usart series makes for strings, just applied to a byte array
// instead of text.
static void max7219DrawBitmap_P(const uint8_t* bitmapProgmem) {
    uint8_t rows[8];
    for (uint8_t i = 0; i < 8; ++i) rows[i] = pgm_read_byte(&bitmapProgmem[i]);
    max7219DrawBitmap(rows);
}

int main() {
    SPI.beginMaster();   // defaults: Mode0, MSBFirst, DIV16 — fine for the MAX7219
    cs_high();

    USART0.begin(9600);
    USART0.writeLine_P(PSTR("MAX7219 8x8 matrix: raw register protocol"));
    USART0.writeLine_P(PSTR("==========================================="));

    max7219Init();
    USART0.writeLine_P(PSTR("max7219Init(): shutdown -> configure -> normal operation"));

    // Prove the wiring works independently of any bitmap data: DisplayTest
    // forces every LED on regardless of the digit registers' contents.
    USART0.writeLine_P(PSTR("Display test: all 64 LEDs on for 1s"));
    max7219Write(MAX7219Reg::DisplayTest, 0x01);
    delay_ms(1000);
    max7219Write(MAX7219Reg::DisplayTest, 0x00);
    max7219Clear();

    while (true) {
        USART0.writeLine_P(PSTR("Drawing: heart"));
        max7219DrawBitmap_P(BITMAP_HEART);
        delay_ms(1500);

        USART0.writeLine_P(PSTR("Drawing: smiley"));
        max7219DrawBitmap_P(BITMAP_SMILEY);
        delay_ms(1500);

        USART0.writeLine_P(PSTR("Drawing: arrow up"));
        max7219DrawBitmap_P(BITMAP_ARROW_UP);
        delay_ms(1500);

        // Sweep brightness through the full 0x00-0x0F intensity range on
        // the arrow bitmap, so Intensity's effect is visible on its own.
        USART0.writeLine_P(PSTR("Intensity sweep (0x00 - 0x0F)"));
        for (uint8_t level = 0; level <= 0x0F; ++level) {
            max7219Write(MAX7219Reg::Intensity, level);
            delay_ms(100);
        }
        max7219Write(MAX7219Reg::Intensity, 0x08);   // restore mid brightness

        max7219Clear();
        delay_ms(500);
    }
}
