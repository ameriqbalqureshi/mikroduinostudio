/*
 * SevenSegShift Basics — 3-Pin 4-Digit Display over 2x 74HC595 —
 * MikroDuino Module SDK
 *
 * The simplest possible use of the SevenSegShift module: drive a common
 * 4-digit multiplexed display using only 3 MCU pins, instead of the
 * 7+N pins SevenSegMux (sdk/modules/SevenSeg) needs when wired directly.
 * This is project 1 of 6 in the examples/Modules/SevenSegShift series,
 * which walks from this static demo up to a capstone 8-digit scoreboard
 * combining Button, EEPROM, and a boot-time scrolling marquee.
 *
 * Hardware (ATmega328P @ 16 MHz, a common 4-digit 7-segment module with
 * two onboard 74HC595s — the typical eBay/AliExpress "4-digit display
 * module" board):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ DATA/SER│ PD4   │ Serial data in, chip A                    │
 *   │ CLK/SRCLK│ PD5  │ Shift clock, shared by both chips          │
 *   │ LATCH/RCLK│PD6  │ Output latch, shared by both chips          │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * That's the entire wiring — no segment or digit-select pins ever touch
 * the MCU directly; both 74HC595s (one driving the shared segment bus,
 * one driving the 4 digit-select lines) are cascaded together and
 * loaded over those same 3 pins on every refresh() call. See
 * SevenSegShift.hpp's header comment for the full chip-to-chip wiring
 * diagram and what to do if your board's two 74HC595s are cascaded in
 * the opposite order (ChainOrder::DIGITS_FIRST).
 *
 * SevenSegShift<N> concepts introduced:
 *   - SevenSegShift<4> mx(dataPin, clockPin, latchPin) — the template
 *     parameter N is the digit count (2-8), fixed at compile time.
 *     This project uses every default: common-cathode segments,
 *     active-HIGH digit-select, ChainOrder::SEGMENTS_FIRST — the
 *     typical wiring for these boards. A common-anode board would pass
 *     commonAnode=true; a board with the digit-select 74HC595 first in
 *     the chain would pass ChainOrder::DIGITS_FIRST.
 *   - begin() — configures the 3 control pins as outputs and blanks
 *     the display.
 *   - setDigit(pos, val) — write a decimal 0-9 or hex 0-15 digit into
 *     the buffer at position pos (0 = leftmost). Buffer only — nothing
 *     appears on the display until refresh() strobes that position.
 *   - setChar(pos, c) — write a character glyph ('0'-'9', 'A'-'F',
 *     plus a handful of other letters with a recognisable 7-segment
 *     shape — see SevenSegShift.hpp's charToSeg() for the full list).
 *   - setRaw(pos, seg) — bypass both lookup tables and drive the exact
 *     segment bitmask (bit0=a … bit6=g, bit7=dp) at pos.
 *   - setDP(pos, on) — toggle just the decimal point at pos.
 *   - print(uint16_t, leadingZero) — decimal-format an integer across
 *     all N positions in one call.
 *   - refresh() — shows the NEXT digit's buffered pattern and shifts
 *     both 74HC595s' bytes out over the 3 pins. Call this repeatedly,
 *     fast enough that no digit is ever off long enough for the eye to
 *     notice — this project calls it once every 2 ms via _delay_ms(),
 *     the simplest of SevenSegShift's three refresh strategies (manual
 *     here; projects 3 and 4 replace it with an ISR and a fully
 *     automatic Timer2 interrupt).
 */

#include <util/delay.h>
#include <SevenSegShift.hpp>

using namespace MikroDuino;

SevenSegShift<4> mx(PD4, PD5, PD6);   // data, clock, latch

int main() {
    mx.begin();

    while (true) {
        // ---- setDigit(): count 0-9 (and hex A-F) on every position at once ----
        for (uint8_t n = 0; n <= 15; ++n) {
            for (uint8_t pos = 0; pos < 4; ++pos) mx.setDigit(pos, n);
            for (uint8_t frame = 0; frame < 150; ++frame) {
                mx.refresh();
                _delay_ms(2);
            }
        }

        mx.clear();

        // ---- setChar(): spell a short word across all 4 digits ----
        static const char word[4] = { 'H', 'E', 'L', 'P' };
        for (uint8_t pos = 0; pos < 4; ++pos) mx.setChar(pos, word[pos]);
        for (uint16_t frame = 0; frame < 300; ++frame) {
            mx.refresh();
            _delay_ms(2);
        }

        mx.clear();

        // ---- setRaw(): a custom glyph no lookup table knows about ----
        // bit0=a, bit1=b, bit5=f, bit6=g -> a small square, top-left,
        // shown on every digit.
        for (uint8_t pos = 0; pos < 4; ++pos) mx.setRaw(pos, 0b1100011);
        for (uint16_t frame = 0; frame < 300; ++frame) {
            mx.refresh();
            _delay_ms(2);
        }

        mx.clear();

        // ---- setDP(): blink every decimal point over a static "8888" ----
        for (uint8_t pos = 0; pos < 4; ++pos) mx.setDigit(pos, 8);
        for (uint8_t blink = 0; blink < 6; ++blink) {
            bool on = (blink % 2 == 0);
            for (uint8_t pos = 0; pos < 4; ++pos) mx.setDP(pos, on);
            for (uint8_t frame = 0; frame < 125; ++frame) {
                mx.refresh();
                _delay_ms(2);
            }
        }

        mx.clear();

        // ---- print(): the class does decimal conversion + layout for you ----
        mx.print(static_cast<uint16_t>(1234), true);
        for (uint16_t frame = 0; frame < 300; ++frame) {
            mx.refresh();
            _delay_ms(2);
        }

        mx.clear();
        for (uint8_t frame = 0; frame < 150; ++frame) {
            mx.refresh();
            _delay_ms(2);
        }
    }
}
