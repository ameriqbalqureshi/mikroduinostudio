/*
 * SPI Buffer Transfers & Non-Blocking Polling — Daisy-Chained 74HC595 —
 * MikroDuino SDK
 *
 * Project 3 of 6 in the examples/spi series. Cascades TWO 74HC595 shift
 * registers into one 16-bit chain and introduces the buffer transfer
 * overload plus manual non-blocking transfers via transferComplete().
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ Signal  │ Pin   │ Wiring                                    │
 *   ├─────────┼───────┼──────────────────────────────────────────┤
 *   │ MOSI    │ PB3   │ U1 (near) pin 14 (SER)                    │
 *   │ SCK     │ PB5   │ U1 + U2 pin 11 (SRCLK) — tied together    │
 *   │ CS      │ PB2   │ U1 + U2 pin 12 (RCLK)  — tied together    │
 *   │ —       │ —     │ U1 pin 9 (QH') -> U2 pin 14 (SER)          │
 *   │ —       │ —     │ both /OE -> GND, both /SRCLR -> VCC       │
 *   │ —       │ —     │ U1 QA-QH, U2 QA-QH -> 16 LEDs + resistors │
 *   │ TXD     │ PD1   │ USB-serial adapter                        │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * Daisy-chaining shift registers: U1's serial OUTPUT pin (QH', pin 9 —
 * not one of the 8 parallel outputs) feeds U2's serial INPUT pin. SCK and
 * CS are shared by both chips — every clock pulse shifts a bit through
 * BOTH registers as one continuous 16-bit shift register, and a single
 * CS pulse latches both simultaneously. The consequence: whichever byte
 * you send FIRST ends up furthest down the chain (in U2, since it gets
 * pushed all the way through by the second byte following it), and the
 * byte sent SECOND stays in U1 (nearest to the microcontroller).
 *
 * SPI concepts introduced:
 *   - SPI.transfer(txBuf, rxBuf, len) — the buffer overload. Sends `len`
 *     bytes back-to-back under ONE CS assertion, receiving `len` bytes
 *     back into rxBuf at the same time (full-duplex, though this project
 *     ignores the RX side since the 74HC595 has no data output). This is
 *     exactly what a daisy chain needs: both bytes must go out before a
 *     single latch pulse, not one-byte-then-latch-then-next-byte.
 *   - Manual non-blocking transfer via transferComplete() — writing SPDR
 *     directly (rather than calling the blocking transfer()) starts a
 *     transfer without waiting for it. The caller can then do other work
 *     and later call SPI.transferComplete() to check whether the SPIF
 *     flag has been set by hardware, reading SPDR only once it has. This
 *     is the same mechanism transfer(uint8_t) uses internally, exposed
 *     for cases where blocking isn't acceptable — e.g. servicing other
 *     peripherals between bytes instead of spinning.
 *
 * SPI concept reused from projects 1-2:
 *   - SPI.beginMaster(), manual CS via GPIO::clear()/set().
 */

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/usart.hpp>
#include <mikroduino/spi.hpp>

using namespace MikroDuino;

static constexpr uint8_t CS = PB2;

static MD_INLINE void cs_low()  { GPIO::clear(CS); }
static MD_INLINE void cs_high() { GPIO::set(CS);   }

static void delay_ms(uint16_t ms) { while (ms--) _delay_ms(1); }

// Send a full 16-bit pattern across both chained registers in one CS
// pulse. Byte order matters here: the byte sent first travels furthest
// down the chain (ends up in U2 / the "far" bits), so the high byte of
// `pattern` (which we want to land in U2) goes out first.
static void shiftOut16(uint16_t pattern) {
    uint8_t txBuf[2] = {
        static_cast<uint8_t>(pattern >> 8),     // far chip  (U2) — sent first
        static_cast<uint8_t>(pattern & 0xFF)    // near chip (U1) — sent second
    };
    uint8_t rxBuf[2];   // unused (74HC595 has no MISO output) but required by the API

    cs_low();
    SPI.transfer(txBuf, rxBuf, 2);
    cs_high();
}

// The same 16-bit send, but built from two manual non-blocking transfers
// instead of one blocking SPI.transfer(txBuf,rxBuf,len) call. Demonstrates
// what that buffer overload is doing internally, one byte at a time:
//   1. Write SPDR directly -> hardware starts shifting immediately.
//   2. Poll transferComplete() (SPIF flag) instead of blocking.
//   3. Read SPDR to retrieve the received byte and clear SPIF.
// Functionally identical output to shiftOut16() above; only the plumbing
// differs, and only within a single CS assertion — SPIF still has to be
// polled once per byte since the hardware is genuinely still one byte
// wide, it just doesn't force the caller to spin doing nothing else.
static void shiftOut16NonBlocking(uint16_t pattern) {
    uint8_t hi = static_cast<uint8_t>(pattern >> 8);
    uint8_t lo = static_cast<uint8_t>(pattern & 0xFF);

    cs_low();

    SPDR = hi;                              // start byte 1 (goes to U2)
    while (!SPI.transferComplete()) {}      // caller could do other work here instead
    (void)SPDR;                             // read clears SPIF; RX byte unused

    SPDR = lo;                              // start byte 2 (goes to U1)
    while (!SPI.transferComplete()) {}
    (void)SPDR;

    cs_high();
}

int main() {
    SPI.beginMaster();
    cs_high();

    USART0.begin(9600);
    USART0.writeLine_P(PSTR("SPI daisy-chained 74HC595 (buffer transfer + polling)"));
    USART0.writeLine_P(PSTR("======================================================="));

    while (true) {
        // -------------------------------------------------------------
        // Marquee: a single lit LED marches across all 16 outputs using
        // the blocking buffer-transfer helper.
        // -------------------------------------------------------------
        USART0.writeLine_P(PSTR("shiftOut16(): marquee across 16 LEDs (blocking buffer transfer)"));
        for (uint8_t i = 0; i < 16; ++i) {
            shiftOut16(static_cast<uint16_t>(1UL << i));
            delay_ms(80);
        }

        // -------------------------------------------------------------
        // Same marquee, but sent with the manual non-blocking helper —
        // visually identical, different plumbing underneath.
        // -------------------------------------------------------------
        USART0.writeLine_P(PSTR("shiftOut16NonBlocking(): same marquee via transferComplete() polling"));
        for (uint8_t i = 0; i < 16; ++i) {
            shiftOut16NonBlocking(static_cast<uint16_t>(1UL << i));
            delay_ms(80);
        }

        // -------------------------------------------------------------
        // Fill from both ends toward the middle, then clear — a pattern
        // that's awkward to express with single-byte transfers because
        // the fill state genuinely spans both chips at once: bit 15 (top
        // of U2) and bit 0 (bottom of U1) light up together, then bit 14
        // and bit 1, and so on.
        // -------------------------------------------------------------
        USART0.writeLine_P(PSTR("Fill from both ends toward the middle"));
        for (uint8_t i = 0; i < 8; ++i) {
            uint16_t pattern = 0;
            for (uint8_t b = 0; b <= i; ++b) {
                pattern |= static_cast<uint16_t>(1u << (15 - b));   // top byte, filling downward
                pattern |= static_cast<uint16_t>(1u << b);          // bottom byte, filling upward
            }
            shiftOut16(pattern);
            delay_ms(120);
        }
        shiftOut16(0xFFFF);
        delay_ms(400);
        shiftOut16(0x0000);
        delay_ms(400);

        USART0.writeLine_P(PSTR(""));
    }
}
