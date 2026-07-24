/*
 * SevenSeg Multiplexed Counter — SevenSegMux<4>, Manual refresh() —
 * MikroDuino Module SDK
 *
 * Project 2 of 6 in the examples/Modules/SevenSeg series. Moves from
 * project 1's single static digit to a 4-digit multiplexed display
 * counting up once a second, refreshed the simplest way SevenSegMux
 * supports: calling refresh() by hand, on a fixed schedule, from the
 * main loop.
 *
 * Hardware (ATmega328P @ 16 MHz, four common-cathode 7-segment digits
 * sharing one segment bus):
 *
 *   ┌──────────────┬───────┬──────────────────────────────────────────┐
 *   │ Segment a-g  │ PD0-6 │ Shared bus — every digit's matching        │
 *   │              │       │ segment lead ties to the SAME MCU pin,     │
 *   │              │       │ each through its own ~220 ohm resistor     │
 *   │ Digit 0 (COM)│ PB0   │ Leftmost digit's common cathode, through    │
 *   │ Digit 1 (COM)│ PB1   │ an NPN transistor (base via a few hundred   │
 *   │ Digit 2 (COM)│ PB2   │ ohms) so the MCU pin only sinks the         │
 *   │ Digit 3 (COM)│ PB3   │ transistor's base current, not all 8        │
 *   │              │       │ segment currents at once — driving a       │
 *   │              │       │ digit pin straight from the MCU works for  │
 *   │              │       │ a quick bench test but is out of spec for  │
 *   │              │       │ real use.                                  │
 *   └──────────────┴───────┴──────────────────────────────────────────┘
 *
 * Why multiplexing needs a shared bus at all: driving 4 fully
 * independent digits like project 1 would need 4 x 8 = 32 pins. Wiring
 * all four digits' segments together and giving each digit its own
 * COMMON pin instead means only one digit is ever actually lit at a
 * time — but by strobing through all 4 fast enough (see refresh()
 * below), persistence of vision makes them all look continuously lit.
 *
 * SevenSegMux<N> concepts introduced:
 *   - SevenSegMux<4> mx(segPins[7], digitPins[4], commonAnode, dpPin) —
 *     the template parameter N is the digit count (2-8), fixed at
 *     compile time so _buf[N] never needs to be heap-allocated. This
 *     project uses no decimal point, so dpPin is omitted (defaults to
 *     SEG_NO_PIN).
 *   - begin() — configures every segment and digit pin as an output
 *     and blanks the whole display.
 *   - print(uint16_t, leadingZero) — the class does the decimal
 *     conversion internally; leadingZero=false (this project's choice)
 *     right-justifies the number with the unused leading digits left
 *     BLANK rather than padded with '0', so "7" reads as "   7" not
 *     "0007".
 *   - refresh() — advances the strobe by exactly one digit: deselects
 *     every digit, drives the segment bus for the NEXT digit's stored
 *     pattern, then selects that one digit. Call this repeatedly, on a
 *     schedule fast enough that no single digit is ever off long
 *     enough for a human eye to notice — this project calls it once
 *     every 2 ms via _delay_ms(), giving each of the 4 digits an 8 ms
 *     (125 Hz) refresh cycle, comfortably above the ~60 Hz flicker
 *     threshold. Manual refresh() is the simplest of SevenSegMux's
 *     three refresh strategies (see SevenSeg.hpp's header comment) —
 *     projects 3 and 4 replace it with an ISR and a fully automatic
 *     Timer2 interrupt, respectively, so the main loop is free to do
 *     other work between refreshes.
 */

#include <util/delay.h>
#include <SevenSeg.hpp>

using namespace MikroDuino;

static const uint8_t SEGS[7]   = { PD0, PD1, PD2, PD3, PD4, PD5, PD6 };
static const uint8_t DIGITS[4] = { PB0, PB1, PB2, PB3 };

SevenSegMux<4> mx(SEGS, DIGITS);

int main() {
    mx.begin();

    uint16_t count   = 0;
    uint16_t elapsed = 0;   // ms since count last advanced

    while (true) {
        mx.refresh();
        _delay_ms(2);

        elapsed += 2;
        if (elapsed >= 1000) {
            elapsed = 0;
            count = static_cast<uint16_t>((count + 1) % 10000u);
            mx.print(count, false);
        }
    }
}
