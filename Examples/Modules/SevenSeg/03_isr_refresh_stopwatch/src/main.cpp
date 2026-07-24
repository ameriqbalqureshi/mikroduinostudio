/*
 * SevenSeg ISR-Refresh Stopwatch — Timer1 CTC + MM:SS via the Shared DP —
 * MikroDuino Module SDK
 *
 * Project 3 of 6 in the examples/Modules/SevenSeg series. Project 2's
 * manual refresh() blocks the CPU in a _delay_ms(2) every single loop
 * pass, which is fine when the display strobe is the ONLY thing the
 * program does, but leaves no room for anything else — this project's
 * stopwatch needs to keep counting seconds while the display strobes
 * completely independently, so refresh() moves into a periodic Timer1
 * compare-match interrupt instead. This is SevenSegMux's "ISR" refresh
 * mode (see SevenSeg.hpp's header comment) — a fixed hardware timer
 * fires refresh() on its own schedule, no matter what the main loop is
 * doing.
 *
 * Hardware (ATmega328P @ 16 MHz, same 4-digit shared-bus wiring as
 * project 2, plus a shared decimal-point line this time):
 *
 *   ┌──────────────┬───────┬──────────────────────────────────────────┐
 *   │ Segment a-g  │ PD0-6 │ Shared bus (see project 2's transistor      │
 *   │              │       │ note — applies here too)                    │
 *   │ Shared dp    │ PB4   │ One dp line shared across all 4 digits;     │
 *   │              │       │ SevenSegMux's per-digit buffer still makes  │
 *   │              │       │ it light only on whichever digit is         │
 *   │              │       │ actually selected at that instant           │
 *   │ Digit 0-3    │ PB0-3 │ Same as project 2                            │
 *   └──────────────┴───────┴──────────────────────────────────────────┘
 *
 * Why Timer1 and not Timer2: SevenSegMuxBase::beginTimer2() (project
 * 4) is the AUTOMATIC option and owns Timer2 outright — this project
 * deliberately does the wiring by hand instead, to show what
 * beginTimer2() does under the hood, so it reaches for Timer1's CTC
 * (Clear Timer on Compare) mode instead. Any general-purpose timer
 * would do; Timer1 just happens to be free in this project.
 *
 * SevenSegMux<N> concepts reused from project 2:
 *   - SevenSegMux<4>(segPins, digitPins, commonAnode, dpPin) — this
 *     time WITH a dpPin, since the stopwatch uses it as a blinking
 *     colon.
 *   - begin(), print(uint16_t, leadingZero) — leadingZero=true this
 *     time, so "1:03" reads as "0103" with every position filled
 *     rather than a blank leading digit.
 *   - refresh() — identical to project 2's, just called from an ISR
 *     instead of the main loop.
 *
 * New concept: setDP(pos, on) sets ONLY position 1's decimal point
 * (the digit right after the minutes place), toggled once a second to
 * blink like a clock's ":" separator — the display shows MMSS as four
 * plain digits, and the blinking dot after the 2nd digit is what reads
 * to a human eye as "the colon between minutes and seconds."
 *
 * Timing model: a free-running Timer1 in CTC mode, OCR1A loaded via
 * ticksForHz() for a 500 Hz compare-match rate (2 ms per tick, the
 * same cadence project 2 used) fires ISR(TIMER1_COMPA_vect), which
 * does nothing but call mx.refresh() — kept deliberately tiny since it
 * runs at 500 Hz. All stopwatch bookkeeping (counting whole seconds,
 * blinking the colon, handling MM:SS rollover) happens in the main
 * loop instead, driven by a plain tick counter incremented once per
 * ISR call under a short atomic guard.
 */

#include <avr/interrupt.h>
#include <mikroduino/timer.hpp>
#include <mikroduino/registers.hpp>
#include <SevenSeg.hpp>

using namespace MikroDuino;

static const uint8_t SEGS[7]   = { PD0, PD1, PD2, PD3, PD4, PD5, PD6 };
static const uint8_t DIGITS[4] = { PB0, PB1, PB2, PB3 };

SevenSegMux<4> mx(SEGS, DIGITS, false, PB4);

// Incremented by the ISR every 2 ms; consumed (never written) by main().
static volatile uint16_t g_ticks2ms = 0;

ISR(TIMER1_COMPA_vect) {
    mx.refresh();
    ++g_ticks2ms;
}

static uint16_t takeTicks() {
    uint16_t t;
    ATOMIC_BLOCK_START;
    t = g_ticks2ms;
    g_ticks2ms = 0;
    ATOMIC_BLOCK_END;
    return t;
}

int main() {
    mx.begin();

    Timer1.mode(TimerMode::CTC);
    Timer1.prescaler(TimerPrescaler::DIV64);
    Timer1.compareA(Timer1.ticksForHz(500));   // 500 Hz -> refresh() every 2 ms
    Timer1.start();
    Timer1.enableInterruptA();
    sei();

    uint16_t elapsedMs   = 0;   // sub-second accumulator
    uint16_t totalSecs   = 0;   // seconds since start, wraps at 99:59
    bool     colonOn     = true;

    while (true) {
        uint16_t ticks = takeTicks();
        if (ticks == 0) continue;

        elapsedMs += static_cast<uint16_t>(ticks) * 2u;
        if (elapsedMs < 1000) continue;

        elapsedMs -= 1000;
        colonOn = !colonOn;

        ++totalSecs;
        uint16_t minutes = (totalSecs / 60u) % 100u;
        uint16_t seconds = totalSecs % 60u;
        uint16_t mmss    = static_cast<uint16_t>(minutes * 100u + seconds);

        mx.print(mmss, true);
        mx.setDP(1, colonOn);
    }
}
