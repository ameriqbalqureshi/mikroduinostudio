/*
 * SevenSegShift ISR-Refresh Stopwatch — Timer1 CTC + MM:SS via setDP() —
 * MikroDuino Module SDK
 *
 * Project 3 of 6 in the examples/Modules/SevenSegShift series. Project
 * 2's manual refresh() blocks the CPU in a _delay_ms(2) every single
 * loop pass, which is fine when strobing the display is the ONLY thing
 * the program does, but leaves no room for anything else — this
 * project's stopwatch needs to keep counting seconds while the display
 * strobes completely independently, so refresh() moves into a periodic
 * Timer1 compare-match interrupt instead. This is SevenSegShift's "ISR"
 * refresh mode — a fixed hardware timer fires refresh() (and shifts the
 * 16 bits through both 74HC595s) on its own schedule, no matter what
 * the main loop is doing.
 *
 * Hardware: identical 3-pin wiring to projects 1-2 (PD4=data, PD5=clock,
 * PD6=latch). No extra pins needed for the colon — unlike SevenSegMux,
 * which needs a dedicated shared-dp pin, SevenSegShift's DP bit already
 * travels through the segment 74HC595 with everything else.
 *
 * Why Timer1 and not Timer2: SevSegShiftBase::beginTimer2() (project 4)
 * is the AUTOMATIC option and owns Timer2 outright — this project
 * deliberately does the wiring by hand instead, to show what
 * beginTimer2() does under the hood, so it reaches for Timer1's CTC
 * (Clear Timer on Compare) mode instead. Any general-purpose timer
 * would do; Timer1 just happens to be free in this project.
 *
 * SevenSegShift<N> concepts reused from projects 1-2:
 *   - SevenSegShift<4>(dataPin, clockPin, latchPin), begin(),
 *     print(uint16_t, leadingZero) — leadingZero=true this time, so
 *     "1:03" reads as "0103" with every position filled rather than a
 *     blank leading digit.
 *   - refresh() — identical to project 2's, just called from an ISR
 *     instead of the main loop.
 *
 * New concept: setDP(pos, on) sets ONLY position 1's decimal point (the
 * digit right after the minutes place), toggled once a second to blink
 * like a clock's ":" separator — the display shows MMSS as four plain
 * digits, and the blinking dot after the 2nd digit is what reads to a
 * human eye as "the colon between minutes and seconds."
 *
 * Timing model: a free-running Timer1 in CTC mode, OCR1A loaded via
 * ticksForHz() for a 500 Hz compare-match rate (2 ms per tick, the same
 * cadence project 2 used) fires ISR(TIMER1_COMPA_vect), which does
 * nothing but call mx.refresh() — kept deliberately tiny since it runs
 * at 500 Hz. All stopwatch bookkeeping (counting whole seconds, blinking
 * the colon, handling MM:SS rollover) happens in the main loop instead,
 * driven by a plain tick counter incremented once per ISR call under a
 * short atomic guard.
 */

#include <avr/interrupt.h>
#include <mikroduino/timer.hpp>
#include <mikroduino/registers.hpp>
#include <SevenSegShift.hpp>

using namespace MikroDuino;

SevenSegShift<4> mx(PD4, PD5, PD6);

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
