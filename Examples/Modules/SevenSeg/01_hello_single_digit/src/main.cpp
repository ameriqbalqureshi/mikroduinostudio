/*
 * SevenSeg Basics — Single Digit, show()/showChar()/showRaw()/setDP() —
 * MikroDuino Module SDK
 *
 * The simplest possible use of the SevenSeg module: drive one common
 * cathode 7-segment digit directly, with no multiplexing at all. This
 * is project 1 of 6 in the examples/Modules/SevenSeg series, which
 * walks from a single static digit up to a capstone reaction-timer
 * game combining a 4-digit multiplexed display, Button, and EEPROM.
 *
 * Hardware (ATmega328P @ 16 MHz, one common-cathode 7-segment digit):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ Segment │ Pin   │ Wiring                                     │
 *   ├─────────┼───────┼──────────────────────────────────────────┤
 *   │ a       │ PD0   │ Each segment pin -> its own ~220 ohm        │
 *   │ b       │ PD1   │ current-limiting resistor -> the matching   │
 *   │ c       │ PD2   │ segment lead on the display.                │
 *   │ d       │ PD3   │                                             │
 *   │ e       │ PD4   │                                             │
 *   │ f       │ PD5   │                                             │
 *   │ g       │ PD6   │                                             │
 *   │ dp      │ PD7   │ Decimal point, same resistor treatment       │
 *   │ COM     │ —     │ Common cathode -> GND directly (this is a   │
 *   │         │       │ single digit, so no multiplexing/transistor  │
 *   │         │       │ is needed the way projects 2-6 require)      │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * SevenSeg concepts introduced:
 *   - SevenSeg(segPins[7], dpPin, commonAnode) — segPins[7] must be in
 *     a-b-c-d-e-f-g order (see the wiring table); dpPin is optional
 *     (SEG_NO_PIN to omit it); commonAnode defaults to false, i.e. a
 *     common-CATHODE display like this project's, where a HIGH segment
 *     pin turns that segment ON.
 *   - begin() — configures all wired pins (segments + dp, if present)
 *     as outputs and blanks the digit. Call once before anything else.
 *   - show(digit) — displays a decimal digit 0-9 OR a hex digit up to
 *     15 (10-15 render as A-F), looked up from the same SEG_TABLE the
 *     multiplexed SevenSegMux class uses internally.
 *   - showChar(c) — displays one character from a small supported set
 *     ('0'-'9', 'A'-'F'/'a'-'f', plus a handful of letters that have a
 *     recognisable 7-segment shape, like 'H', 'L', 'P', 'U' — see
 *     SevenSeg.hpp's charToSeg() for the full list). Anything else
 *     shows blank; there is no way to spell most words on one digit
 *     anyway, so this is mostly useful for single status letters.
 *   - showRaw(seg) — bypasses both lookup tables entirely and drives
 *     the exact segment bitmask given (bit0=a ... bit6=g, bit7=dp).
 *     This is how a custom glyph that isn't a digit or a supported
 *     letter gets drawn — this project uses it for a small square in
 *     the top-left of the digit, built from segments a, b, f, and g.
 *   - setDP(on) — toggles just the decimal point without touching
 *     whatever digit/char/raw pattern is currently shown.
 *   - clear() — blanks the digit (all segments + dp off).
 */

#include <util/delay.h>
#include <SevenSeg.hpp>

using namespace MikroDuino;

static const uint8_t SEGS[7] = { PD0, PD1, PD2, PD3, PD4, PD5, PD6 };

SevenSeg digit(SEGS, PD7, false);   // common cathode, dp on PD7

int main() {
    digit.begin();

    while (true) {
        // ---- show(): decimal 0-9, then hex A-F (10-15) on the same call ----
        for (uint8_t n = 0; n <= 15; ++n) {
            digit.show(n);
            _delay_ms(300);
        }

        digit.clear();
        _delay_ms(300);

        // ---- showChar(): a few of the supported single-letter shapes ----
        static const char letters[] = { 'H', 'E', 'L', 'P', 'U', '-', '_' };
        for (char c : letters) {
            digit.showChar(c);
            _delay_ms(400);
        }

        digit.clear();
        _delay_ms(300);

        // ---- showRaw(): a custom glyph no lookup table knows about ----
        // bit0=a, bit1=b, bit5=f, bit6=g -> a small square, top-left.
        digit.showRaw(0b1100011);
        _delay_ms(600);
        digit.clear();
        _delay_ms(300);

        // ---- setDP(): blink the decimal point over a static '8' ----
        digit.show(8);
        for (uint8_t i = 0; i < 6; ++i) {
            digit.setDP(i % 2 == 0);
            _delay_ms(250);
        }

        digit.clear();
        _delay_ms(500);
    }
}
