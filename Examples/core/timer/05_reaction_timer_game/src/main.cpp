/*
 * Timer1 Free-Running Stopwatch — Reaction Timer Game — MikroDuino SDK
 *
 * Project 5 of 6 in the examples/timer series. Uses Timer1 as a plain
 * free-running microsecond-resolution stopwatch (no CTC, no capture, no
 * external clock — just count()/reset() around an event) to build a
 * human reaction-time game: after a random delay, an LED lights up, and
 * the game measures how long it takes to press a button in response.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ LED     │ PB5   │ Lights up at the unpredictable "go" moment │
 *   │ Button  │ PD2   │ Other leg to GND, internal pull-up          │
 *   │ TXD     │ PD1   │ USB-serial adapter                        │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * Why "extend the range" matters here: at Timer1's DIV64 prescale
 * (4 us/tick), the raw 16-bit counter alone rolls over every
 * 65536 * 4 us ≈ 262 ms. A slow reaction (or a deliberately long false-
 * start wait) easily exceeds that. This project's stopwatch therefore
 * counts OVERFLOWS in an ISR and folds them into every reading, giving
 * an effective 32-bit tick range — hours, not a fraction of a second.
 *
 * Timer concepts introduced:
 *   - Timer1.enableOverflow() + ISR(TIMER1_OVF_vect) used purely to
 *     EXTEND range (a software high word on top of the 16-bit hardware
 *     counter), a different motivation from project 2's Timer0 overflow
 *     (which used it as the entire time base). Same mechanism, different
 *     reason to reach for it.
 *   - Reading a hardware counter and a software overflow counter
 *     TOGETHER, safely: readStopwatchTicks() below disables interrupts
 *     just long enough to read TCNT1 and the overflow counter as one
 *     consistent snapshot, AND checks Timer1.overflowFlag() (TOV1)
 *     directly for the edge case where hardware has already wrapped but
 *     the ISR hasn't run yet (interrupts were briefly disabled) —
 *     without that check, a reading taken in that narrow window would
 *     under-count by exactly one overflow's worth of ticks.
 *   - Timer1.clearOverflowFlag() — used in resetStopwatch() to discard
 *     any overflow flag left pending from just before a reset, so the
 *     very next readStopwatchTicks() doesn't misinterpret stale state
 *     as a wrap that happened during the new measurement.
 *
 * Timer concepts reused from earlier projects:
 *   - Timer1.mode(TimerMode::Normal), Timer1.prescaler(),
 *     Timer1.start(), Timer1.reset(), Timer1.prescalerValue() (project 1
 *     introduced prescalerValue() via ticksForHz(); this project uses it
 *     directly to convert ticks back to milliseconds generically, so the
 *     conversion still works correctly if the prescaler above is ever
 *     changed).
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/timer.hpp>
#include <mikroduino/usart.hpp>
#include <mikroduino/registers.hpp>

using namespace MikroDuino;

static constexpr uint8_t LED    = PB5;
static constexpr uint8_t BUTTON = PD2;

static void delay_ms(uint16_t ms) { while (ms--) _delay_ms(1); }

// ── Timer1: extended-range free-running stopwatch ──────────────────────────

static volatile uint32_t g_overflowCount = 0;

ISR(TIMER1_OVF_vect) {
    ++g_overflowCount;
}

// Snapshot TCNT1 and the software overflow count together, correctly
// handling the case where hardware has already wrapped (TOV1 set) but
// the ISR hasn't incremented g_overflowCount yet because interrupts were
// disabled for this very read.
static uint32_t readStopwatchTicks() {
    uint16_t cnt;
    uint32_t ovf;

    ATOMIC_BLOCK_START;
    cnt = TCNT1;
    ovf = g_overflowCount;
    if (Timer1.overflowFlag() && cnt < 0x8000u) {
        ++ovf;   // hardware wrapped; our ISR hasn't caught up in this window
    }
    ATOMIC_BLOCK_END;

    return (ovf << 16) | cnt;
}

static void resetStopwatch() {
    ATOMIC_BLOCK_START;
    Timer1.reset();               // TCNT1 = 0
    g_overflowCount = 0;
    Timer1.clearOverflowFlag();   // clear any stale pending overflow flag
    ATOMIC_BLOCK_END;
}

static uint32_t ticksToMs(uint32_t ticks) {
    uint32_t prescaler = Timer1.prescalerValue();
    uint32_t us = (ticks * prescaler) / (F_CPU / 1000000UL);
    return us / 1000UL;
}

// ── A tiny linear congruential generator for "random enough" delays ───────
// Not cryptographic, just enough variation that the light's timing isn't
// predictable round to round. Reseeded each round from the previous
// reaction time, which contributes genuine human-timing entropy.
static uint32_t g_rngState = 0xACE1u;

static uint16_t nextRandom() {
    g_rngState = g_rngState * 1103515245UL + 12345UL;
    return static_cast<uint16_t>(g_rngState >> 16);
}

int main() {
    GPIO::output(LED);
    GPIO::clear(LED);
    GPIO::inputPullup(BUTTON);

    USART0.begin(9600);
    USART0.writeLine_P(PSTR("Timer1 reaction timer game"));
    USART0.writeLine_P(PSTR("============================"));
    USART0.writeLine_P(PSTR("Wait for the LED, then press the button as fast as you can!"));
    USART0.writeLine_P(PSTR("Pressing early ends the round as a false start."));
    USART0.writeLine_P(PSTR(""));

    Timer1.mode(TimerMode::Normal);
    Timer1.prescaler(TimerPrescaler::DIV64);
    Timer1.start();
    Timer1.enableOverflow();
    sei();

    uint32_t bestMs = 0xFFFFFFFFUL;
    uint32_t sumMs  = 0;
    uint16_t rounds = 0;

    g_rngState ^= readStopwatchTicks();   // fold in whatever jitter has
                                            // accumulated since boot as
                                            // an initial seed

    while (true) {
        USART0.writeLine_P(PSTR("Get ready..."));

        uint16_t waitMs = static_cast<uint16_t>(1500u + (nextRandom() % 2500u));
        bool falseStart = false;

        // Poll for an early press during the random wait. Checking in
        // small slices (rather than one long delay) is what makes this
        // false-start detection possible at all.
        for (uint16_t waited = 0; waited < waitMs; waited += 10) {
            if (GPIO::read(BUTTON) == false) {
                falseStart = true;
                break;
            }
            delay_ms(10);
        }

        if (falseStart) {
            USART0.writeLine_P(PSTR(">> Too soon! That was a false start."));
            USART0.writeLine_P(PSTR(""));
            // Wait for release before starting the next round, so the
            // same press can't immediately trigger the next round too.
            while (GPIO::read(BUTTON) == false) {}
            delay_ms(300);
            continue;
        }

        // "Go": light the LED and start the stopwatch as close together
        // as two back-to-back instructions allow.
        resetStopwatch();
        GPIO::set(LED);

        while (GPIO::read(BUTTON) != false) {}   // wait for the press —
                                                   // no debounce delay here,
                                                   // it would add latency
                                                   // to the very thing
                                                   // being measured
        uint32_t ticks = readStopwatchTicks();
        GPIO::clear(LED);

        uint32_t reactionMs = ticksToMs(ticks);

        ++rounds;
        sumMs += reactionMs;
        if (reactionMs < bestMs) bestMs = reactionMs;

        USART0.write_P(PSTR(">> Reaction time: "));
        USART0.writeInt(static_cast<int32_t>(reactionMs));
        USART0.writeLine_P(PSTR(" ms"));
        USART0.write_P(PSTR("   best="));
        USART0.writeInt(static_cast<int32_t>(bestMs));
        USART0.write_P(PSTR(" ms   average="));
        USART0.writeInt(static_cast<int32_t>(sumMs / rounds));
        USART0.writeLine_P(PSTR(" ms"));
        USART0.writeLine_P(PSTR(""));

        // Debounce the release and give a moment before the next round.
        while (GPIO::read(BUTTON) == false) {}
        delay_ms(800);
    }
}
