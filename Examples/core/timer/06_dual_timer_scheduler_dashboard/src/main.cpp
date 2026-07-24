/*
 * Dual-Timer Cooperative Scheduler + Profiler Dashboard — MikroDuino SDK
 * (capstone)
 *
 * Project 6 of 6 in the examples/timer series. Timer1 drives a small
 * cooperative task scheduler entirely off ONE hardware timer's TWO
 * independent compare channels, while Timer0 runs completely separately
 * as an ad-hoc profiler — no scheduling role at all, just a free-running
 * stopwatch used to measure how long one of the scheduled tasks takes.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ LED     │ PB5   │ Heartbeat — toggles every 500 ms, scheduled │
 *   │ Button  │ PD2   │ Other leg to GND, internal pull-up —      │
 *   │         │       │ presses are debounced entirely inside an   │
 *   │         │       │ interrupt, not in the scheduler            │
 *   │ TXD     │ PD1   │ USB-serial adapter                        │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * Timer1's two roles from ONE CTC configuration:
 *   - compareA (OCR1A) is the CTC TOP, set for an exact 1 ms period via
 *     ticksForHz(1000) — unlike project 2's Timer0-overflow millis(),
 *     CTC can hit 1.000 ms exactly (249 ticks at DIV64), so no
 *     fractional-microsecond accumulator is needed here at all. This is
 *     the real advantage CTC has over overflow-based timekeeping,
 *     concretely demonstrated by the absence of code project 2 needed.
 *   - compareB (OCR1B) is set to roughly HALF of compareA's value. In
 *     CTC-via-OCR1A mode, OCR1B does NOT reset the counter or define
 *     TOP — it just raises its own independent compare-match interrupt
 *     at that point in every 1 ms cycle, giving a second, faster
 *     (~0.5 ms) tick for free from the same counter. This project uses
 *     that fast tick to run a tiny button-debounce state machine
 *     entirely inside ISR(TIMER1_COMPB_vect), fully decoupled from the
 *     1 ms scheduler tick in ISR(TIMER1_COMPA_vect).
 *
 * The scheduler itself: a small array of {intervalMs, nextRunMs,
 * function pointer} entries, checked once per main() loop pass against
 * a snapshot of the 1 ms tick counter. This is the same "blink without
 * delay" idea project 2 used for one LED, generalised to any number of
 * independent periodic tasks sharing one time base — heartbeatTask()
 * (500 ms) and statusTask() (2000 ms) below.
 *
 * Timer0's separate, non-scheduling role: statusTask() calls
 * Timer0.reset() at its start and Timer0.count() at its end, purely to
 * report how long its own USART output took. Timer0 runs free (Normal
 * mode, no interrupt at all) the whole program at a slow DIV1024
 * prescale specifically chosen so one 8-bit rollover (256 ticks *
 * 64 us/tick = 16.384 ms) comfortably covers the handful of USART writes
 * being timed — see the comment in statusTask() for what happens if that
 * assumption stops holding.
 *
 * Timer concepts reused from the whole series:
 *   - Timer1.mode(CTC), prescaler(), compareA(), ticksForHz(), start(),
 *     enableInterruptA() (project 1).
 *   - Timer0.mode(Normal), prescaler(), start(), reset(), count()
 *     (projects 2 and 5, here without any interrupt at all — the
 *     simplest possible use of a timer: just a free-running counter,
 *     read on demand).
 *
 * Timer concepts introduced (bonus aside, run once at startup before the
 * real scheduler takes over Timer1 — see startupAside() below):
 *   - Timer1.inputCapture(value) used for its ACTUAL purpose: setting
 *     ICR1 as the TOP register in an ICR1-based waveform mode. Project 3
 *     read ICR1 (hardware-written, in true input-capture mode); this is
 *     the opposite direction, and the reason the method exists at all.
 *   - Timer1.compareAFlag() / Timer1.clearCompareAFlag() — the polled-
 *     flag alternative to enableInterruptA(): checking OCF1A directly in
 *     a spin-loop, with no interrupt enabled whatsoever.
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/timer.hpp>
#include <mikroduino/usart.hpp>
#include <mikroduino/registers.hpp>

using namespace MikroDuino;

static constexpr uint8_t LED    = PB5;
static constexpr uint8_t BUTTON = PD2;

// ── Timer1: 1 ms scheduler tick (compareA) + 0.5 ms debounce subtick (compareB) ──

static volatile uint32_t g_ticks      = 0;   // milliseconds since boot
static volatile uint16_t g_pressCount = 0;   // debounced button presses

ISR(TIMER1_COMPA_vect) {
    ++g_ticks;
}

ISR(TIMER1_COMPB_vect) {
    // A tiny debounce state machine sampled every ~0.5 ms: requires 6
    // consecutive low samples (~3 ms of stable contact) before counting
    // a press, and won't count another until the button is released.
    static uint8_t lowStreak    = 0;
    static bool    pressLatched = false;

    bool nowLow = (GPIO::read(BUTTON) == false);
    if (nowLow) {
        if (lowStreak < 6) ++lowStreak;
    } else {
        lowStreak = 0;
        pressLatched = false;
    }

    if (lowStreak >= 6 && !pressLatched) {
        pressLatched = true;
        ++g_pressCount;
    }
}

static uint32_t ticksNow() {
    uint32_t snapshot;
    ATOMIC_BLOCK_START;
    snapshot = g_ticks;
    ATOMIC_BLOCK_END;
    return snapshot;
}

// ── Scheduled tasks ──────────────────────────────────────────────────────

static void heartbeatTask() {
    GPIO::toggle(LED);
}

static void statusTask() {
    Timer0.reset();   // lap marker: zero the free-running profiler counter

    uint16_t presses;
    ATOMIC_BLOCK_START;
    presses = g_pressCount;
    ATOMIC_BLOCK_END;

    USART0.write_P(PSTR("uptime="));
    USART0.writeInt(static_cast<int32_t>(ticksNow() / 1000));
    USART0.write_P(PSTR("s  presses="));
    USART0.writeInt(presses);
    USART0.writeLine_P(PSTR(""));

    // Timer0 free-runs at DIV1024 (64 us/tick) for the whole program;
    // count() here reads how many ticks elapsed since reset() above.
    // This naive approach has NO overflow tracking (contrast project 5,
    // which added exactly that): it only works because these few USART
    // writes reliably finish inside one 8-bit rollover window
    // (256 * 64 us = 16.384 ms). Anything longer would silently wrap and
    // report a meaningless short duration instead of the true one — a
    // fine tool for bounding a short, known operation, the wrong tool
    // for anything open-ended.
    uint16_t profileTicks = Timer0.count();
    uint32_t profileUs = static_cast<uint32_t>(profileTicks) * 64UL;

    USART0.write_P(PSTR("  (status print took ~"));
    USART0.writeInt(static_cast<int32_t>(profileUs));
    USART0.writeLine_P(PSTR(" us, profiled by Timer0)"));
}

struct Task {
    uint32_t intervalMs;
    uint32_t nextRunMs;
    void (*fn)();
};

static Task g_tasks[2] = {
    { 500,  0, heartbeatTask },
    { 2000, 0, statusTask    },
};

// ── Bonus aside: inputCapture() as an ICR1-TOP setter, and polled ─────────
//    compareAFlag()/clearCompareAFlag() instead of an interrupt. Runs
//    once, before Timer1 is reconfigured into its real CTC scheduler
//    role, so it doesn't interfere with anything below.
static void startupAside() {
    USART0.writeLine_P(PSTR("Bonus aside: inputCapture() as an ICR1-TOP setter + polled compareAFlag()"));

    Timer1.mode(TimerMode::FastPWM);           // WGM14: counts 0 -> ICR1 -> 0
    Timer1.prescaler(TimerPrescaler::DIV64);
    Timer1.inputCapture(624);                   // ICR1 = 624 (the TOP here, NOT a captured value)
    Timer1.compareA(300);                       // an arbitrary compare match partway through the count
    Timer1.start();

    while (!Timer1.compareAFlag()) {}           // polled — no interrupt enabled at all
    Timer1.clearCompareAFlag();

    USART0.write_P(PSTR("  ICR1 = "));
    USART0.writeInt(static_cast<int32_t>(ICR1));
    USART0.writeLine_P(PSTR(" (confirms inputCapture() just wrote the register)"));
    USART0.writeLine_P(PSTR("  compareAFlag() observed via polling, then cleared"));

    Timer1.stop();   // tear down before the real CTC configuration below
    USART0.writeLine_P(PSTR(""));
}

int main() {
    GPIO::output(LED);
    GPIO::inputPullup(BUTTON);

    USART0.begin(9600);
    USART0.writeLine_P(PSTR("Dual-timer cooperative scheduler + profiler dashboard"));
    USART0.writeLine_P(PSTR("========================================================"));

    startupAside();

    // Timer1: the real configuration for the rest of the program — CTC
    // scheduler tick on compareA, debounce subtick on compareB.
    Timer1.mode(TimerMode::CTC);
    Timer1.prescaler(TimerPrescaler::DIV64);
    uint16_t oneMs = Timer1.ticksForHz(1000);    // 249 ticks -> exact 1.000 ms at DIV64
    Timer1.compareA(oneMs);
    Timer1.compareB(static_cast<uint16_t>(oneMs / 2));
    Timer1.start();
    Timer1.enableInterruptA();
    Timer1.enableInterruptB();

    // Timer0: free-running profiler only, no interrupt, no scheduling
    // role — see statusTask() for how it's used.
    Timer0.mode(TimerMode::Normal);
    Timer0.prescaler(TimerPrescaler::DIV1024);
    Timer0.start();

    sei();

    USART0.writeLine_P(PSTR("Scheduler running: heartbeat every 500 ms, status every 2 s."));
    USART0.writeLine_P(PSTR("Press the button any time; presses are debounced in ISR(TIMER1_COMPB_vect)."));
    USART0.writeLine_P(PSTR(""));

    while (true) {
        uint32_t now = ticksNow();

        for (uint8_t i = 0; i < 2; ++i) {
            if (now >= g_tasks[i].nextRunMs) {
                g_tasks[i].fn();
                g_tasks[i].nextRunMs = now + g_tasks[i].intervalMs;
            }
        }
    }
}
