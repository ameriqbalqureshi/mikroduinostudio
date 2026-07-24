/*
 * SevenSegShift Multiplexed Counter — Manual refresh(), Polarity Flags —
 * MikroDuino Module SDK
 *
 * Project 2 of 6 in the examples/Modules/SevenSegShift series. Moves
 * from project 1's static demo patterns to a 4-digit counter ticking
 * up once a second, refreshed the simplest way SevenSegShift supports:
 * calling refresh() by hand, on a fixed schedule, from the main loop.
 * Same 3-pin wiring as project 1 — nothing about the electrical
 * connections changes as the software gets more capable.
 *
 * Hardware: identical to project 1 (PD4=data, PD5=clock, PD6=latch,
 * two cascaded 74HC595s driving a 4-digit common-cathode module).
 *
 * Why multiplexing over shift registers needs only 3 pins at all:
 * SevenSegMux (sdk/modules/SevenSeg) wires 7 segment pins + N digit
 * pins straight to the MCU — 11 pins for a 4-digit display, 15 for an
 * 8-digit one. SevenSegShift instead loads a segment byte and a
 * digit-select byte into two cascaded 74HC595s over DATA/CLOCK/LATCH,
 * so the pin count stays fixed at 3 no matter how many digits the
 * template parameter N asks for (project 6 pushes this to N=8 on the
 * exact same 3 pins).
 *
 * Polarity flags, explained here since this is the first project to
 * pass them explicitly (project 1 relied on their defaults):
 *   - commonAnode (segment byte): false (default, used here) for
 *     common-CATHODE boards — the segment byte is sent to the 74HC595
 *     as-is, bit=1 lights that segment. true inverts the byte before
 *     shifting, for common-ANODE boards where the 74HC595 output must
 *     idle HIGH to keep a segment off.
 *   - digitActiveHigh (digit-select byte): true (default, used here)
 *     for the common wiring where each digit's transistor turns ON
 *     when its 74HC595 output goes HIGH (NPN driver, typical on
 *     common-cathode boards). false inverts the digit-select byte, for
 *     boards with an active-LOW digit driver (PNP, typical on
 *     common-anode boards).
 * If your board's digits light in the wrong pattern (right segments,
 * wrong digit, or vice versa), one of these two flags — not the wiring
 * — is almost always the fix.
 *
 * SevenSegShift<N> concepts introduced:
 *   - print(uint16_t, leadingZero) — the class does the decimal
 *     conversion internally; leadingZero=false (this project's choice)
 *     right-justifies the number with unused leading digits left BLANK
 *     rather than padded with '0', so "7" reads as "   7" not "0007".
 *   - refresh() — advances the strobe by exactly one digit: shifts a
 *     fresh segment byte + digit-select byte through both 74HC595s and
 *     pulses latch once. Call this repeatedly, on a schedule fast
 *     enough that no single digit is ever off long enough for a human
 *     eye to notice — this project calls it once every 2 ms via
 *     _delay_ms(), giving each of the 4 digits an 8 ms (125 Hz) refresh
 *     cycle, comfortably above the ~60 Hz flicker threshold. Manual
 *     refresh() is the simplest of SevenSegShift's three refresh
 *     strategies — projects 3 and 4 replace it with an ISR and a fully
 *     automatic Timer2 interrupt, respectively, so the main loop is
 *     free to do other work between refreshes.
 */

#include <util/delay.h>
#include <SevenSegShift.hpp>

using namespace MikroDuino;

SevenSegShift<4> mx(PD4, PD5, PD6, false, true, SevenSegShift<4>::ChainOrder::SEGMENTS_FIRST);

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
