#pragma once
/*
 * Pulse — on-demand elapsed time and GPIO pulse measurement.
 *
 * Two classes, one header.  Both use Timer1 (prescaler 8).
 * Do NOT use simultaneously with Servo (also owns Timer1).
 * Do NOT use both classes at the same time.
 *
 * ┌──────────────────────────────────────────────────────────────────────────┐
 * │ Stopwatch — on-demand elapsed time                                      │
 * │                                                                         │
 * │  Timer1 is enabled only while the stopwatch is running and disabled    │
 * │  when stopped.  No background interrupt overhead.                       │
 * │                                                                         │
 * │  Maximum range without ISR (one Timer1 overflow period):               │
 * │    16 MHz → 32.768 ms    8 MHz → 65.536 ms                            │
 * │                                                                         │
 * │  For longer measurements, route Timer1 OVF to _isrOvf():              │
 * │    ISR(TIMER1_OVF_vect) { sw._isrOvf(); }                             │
 * │  With ISR: max ~35 min at 16 MHz (uint16_t OVF counter).              │
 * │                                                                         │
 * │  Stopwatch sw;                                                          │
 * │  sw.start();                                                            │
 * │  doWork();                                                              │
 * │  uint32_t us = sw.elapsedUs();   // stop() not required to read       │
 * │  sw.stop();                      // disables Timer1                   │
 * └──────────────────────────────────────────────────────────────────────────┘
 *
 * ┌──────────────────────────────────────────────────────────────────────────┐
 * │ PulseMeter — blocking GPIO pulse measurement                            │
 * │                                                                         │
 * │  No ISR needed.  Timer1 overflow is polled in the measurement loop,   │
 * │  so every overflow is caught regardless of interrupt state.            │
 * │  Works on any GPIO pin; no hardware capture pin required.             │
 * │                                                                         │
 * │  PulseMeter pm(PC3);   pm.begin();                                     │
 * │  uint32_t us = pm.pulseWidth(true);        // HIGH pulse in µs        │
 * │  uint32_t T  = pm.period();                // full period in µs       │
 * │  uint32_t hz = pm.frequency(200000UL);     // over a 200 ms window    │
 * └──────────────────────────────────────────────────────────────────────────┘
 *
 * Timer1 prescaler 8 → 0.5 µs/tick at 16 MHz, 1 µs/tick at 8 MHz.
 * Resolution is therefore F_CPU-dependent but better than 1 µs at ≥ 8 MHz.
 *
 * Conflict table:
 *   Servo          — owns Timer1; cannot run simultaneously.
 *   IRRemote       — Timer2 only; no conflict.
 *   SevenSegMux    — Timer2 only; no conflict.
 *   DCMotor OC0A/B — Timer0 only; no conflict.
 *   DCMotor OC2A/B — Timer2 only; no conflict.
 */

#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <mikroduino/gpio.hpp>

namespace MikroDuino {

// ============================================================================
//  Internal: tick ↔ microsecond conversion for Timer1 prescaler 8
// ============================================================================
namespace detail {

// Convert Timer1 ticks (prescaler 8) to microseconds.
// Uses 64-bit intermediate to handle the full uint32_t tick range.
static inline uint32_t ticksToUs(uint32_t ticks) {
    return static_cast<uint32_t>(
        static_cast<uint64_t>(ticks) * 8000000ULL / static_cast<uint64_t>(F_CPU));
}

// Convert microseconds to Timer1 ticks (prescaler 8).
static inline uint32_t usToTicks(uint32_t us) {
    return static_cast<uint32_t>(
        static_cast<uint64_t>(us) * static_cast<uint64_t>(F_CPU) / 8000000ULL);
}

} // namespace detail

// ============================================================================
//  Stopwatch
// ============================================================================

class Stopwatch {
public:
    Stopwatch() : _running(false), _frozenTicks(0u) {}

    // ── Lifecycle ────────────────────────────────────────────────────────────

    // Start (or restart) timing from zero.  Configures Timer1 and enables it.
    void start() {
        TCCR1A   = 0u;
        TCCR1B   = 0u;           // stop before writing TCNT1
        _frozenTicks = 0u;
        _ovf         = 0u;
        TCNT1        = 0u;
        TIFR1        = (1u << TOV1);
        TCCR1B       = (1u << CS11); // prescaler 8, run
        _running     = true;
    }

    // Stop timing and freeze the elapsed value.  Disables Timer1.
    // May be called while running or already stopped (idempotent).
    void stop() {
        if (!_running) return;
        _frozenTicks += _rawTicks();
        TCCR1B   = 0u;           // stop Timer1
        _running = false;
    }

    // Resume from the last stop() without resetting the accumulated time.
    // Has no effect if already running.
    void resume() {
        if (_running) return;
        _ovf    = 0u;
        TCNT1   = 0u;
        TIFR1   = (1u << TOV1);
        TCCR1B  = (1u << CS11);
        _running = true;
    }

    // Stop and clear the accumulated time.
    void reset() {
        TCCR1B       = 0u;
        _running     = false;
        _frozenTicks = 0u;
        _ovf         = 0u;
    }

    // ── Queries ──────────────────────────────────────────────────────────────

    bool isRunning() const { return _running; }

    // Elapsed time since the last start() (accumulated across stop/resume).
    // Safe to call while running.
    uint32_t elapsedUs() const {
        uint32_t t = _running ? (_frozenTicks + _rawTicks()) : _frozenTicks;
        return detail::ticksToUs(t);
    }

    uint32_t elapsedMs() const { return elapsedUs() / 1000u; }

    // ── Optional ISR routing for long measurements ───────────────────────────
    // Without routing this ISR, accurate range is one Timer1 overflow period:
    //   16 MHz / prescaler 8 → 32.768 ms
    //    8 MHz / prescaler 8 → 65.536 ms
    //
    // To extend to ~35 minutes, add to user code:
    //   ISR(TIMER1_OVF_vect) { myStopwatch._isrOvf(); }
    void _isrOvf() { _ovf++; }

    // Internal — not for direct use.
    static volatile uint16_t _ovf;

private:
    bool     _running;
    uint32_t _frozenTicks;   // accumulated ticks from prior start/stop intervals

    // Atomic read of TCNT1 + OVF count.
    // Uses late-OVF correction: if the OVF hardware flag is set but the ISR
    // hasn't run yet (TCNT1 still below the midpoint), counts one extra OVF.
    // Without the ISR, _ovf stays 0 and this only detects ONE pending OVF —
    // accurate for one timer period; longer durations need the ISR.
    uint32_t _rawTicks() const {
        uint8_t  sreg = SREG;
        cli();
        uint16_t cnt = TCNT1;
        uint16_t ovf = _ovf;
        if ((TIFR1 & (1u << TOV1)) && (cnt < 0x8000u)) ovf++;
        SREG = sreg;
        return static_cast<uint32_t>(ovf) * 65536UL + cnt;
    }
};

// ============================================================================
//  PulseMeter
// ============================================================================

class PulseMeter {
public:
    // gpioPin: any MikroDuino GPIO pin encoding.
    explicit PulseMeter(uint8_t gpioPin) : _pin(gpioPin) {}

    // Configure pin as digital input (floating).
    // Add external pull-up/pull-down as needed for your signal source.
    // For INPUT_PULLUP, call GPIO::inputPullup(_pin) after begin().
    void begin() { GPIO::input(_pin); }

    // ── Pulse width ──────────────────────────────────────────────────────────
    //
    // Measure one complete pulse in µs.
    //
    //   polarity   true  = measure a HIGH pulse (LOW → HIGH → LOW)
    //              false = measure a LOW  pulse (HIGH → LOW → HIGH)
    //
    //   timeoutUs  maximum total time to wait for the pulse to appear AND end.
    //              Default 100 ms covers servos, ultrasonics, IR.
    //
    // Sequence:
    //   1. Sync to idle state (wait for !polarity, in case mid-pulse).
    //   2. Detect leading edge (pin transitions to polarity).
    //   3. Measure active duration (count ticks until trailing edge).
    //
    // Returns pulse width in µs.  Returns 0 on timeout.
    uint32_t pulseWidth(bool polarity = true, uint32_t timeoutUs = 100000UL);

    // ── Period ───────────────────────────────────────────────────────────────
    //
    // Measure the time between two consecutive edges of the same type in µs.
    //
    //   risingEdge  true  = time between rising  edges (full cycle period)
    //               false = time between falling edges
    //   timeoutUs   maximum wait time. Default 1 s.
    //
    // Returns 0 on timeout.
    uint32_t period(bool risingEdge = true, uint32_t timeoutUs = 1000000UL);

    // ── Frequency ────────────────────────────────────────────────────────────
    //
    // Count rising edges over a fixed window and return frequency in Hz.
    //
    //   windowUs  measurement window.  Longer windows give better accuracy
    //             for slow signals.  Blocking for the full duration.
    //             Default 100 ms → ±10 Hz accuracy at any frequency.
    //
    // Accurate up to ~50 kHz (GPIO poll rate limited).
    // Returns 0 if no edges detected in the window.
    uint32_t frequency(uint32_t windowUs = 100000UL);

private:
    uint8_t _pin;

    // Configure and zero Timer1 (prescaler 8).
    static void _timerStart() {
        TCCR1A = 0u;
        TCCR1B = 0u;           // stop before TCNT1 write
        TCNT1  = 0u;
        TIFR1  = (1u << TOV1);
        TCCR1B = (1u << CS11); // prescaler 8, start
    }

    static void _timerStop() { TCCR1B = 0u; }

    // Wait for _pin to reach target state.
    //
    // ovfAccum: running OVF count shared across sequential _awaitState calls
    //           on the same timer session (reset externally before first call).
    // maxTicks: total tick budget including OVF periods.
    //
    // TIFR1 is polled every loop iteration — no ISR needed, no OVF is missed.
    // Returns true if state reached within budget; false on timeout.
    //
    // After return, TCNT1 holds the tick count and ovfAccum holds OVF count
    // since the last TCNT1/TIFR1 reset.  The caller decides whether to reset.
    bool _awaitState(bool target, uint32_t& ovfAccum, uint32_t maxTicks) {
        uint32_t maxOvf = maxTicks >> 16u;
        uint16_t maxRem = static_cast<uint16_t>(maxTicks & 0xFFFFu);
        while (GPIO::read(_pin) != target) {
            if (TIFR1 & (1u << TOV1)) { TIFR1 = (1u << TOV1); ovfAccum++; }
            if (ovfAccum > maxOvf)                              return false;
            if (ovfAccum == maxOvf && TCNT1 >= maxRem)         return false;
        }
        return true;
    }

    // Count ticks while _pin holds state (polls TIFR1 for OVF).
    // Returns total ticks (OVF × 65536 + TCNT1) at the moment the state ends.
    // Returns 0xFFFFFFFFUL on timeout.
    uint32_t _measureState(bool state, uint32_t maxTicks) {
        uint32_t ovf    = 0u;
        uint32_t maxOvf = maxTicks >> 16u;
        uint16_t maxRem = static_cast<uint16_t>(maxTicks & 0xFFFFu);
        TIFR1 = (1u << TOV1);                         // start fresh OVF count

        while (GPIO::read(_pin) == state) {
            if (TIFR1 & (1u << TOV1)) { TIFR1 = (1u << TOV1); ovf++; }
            if (ovf > maxOvf)                              return 0xFFFFFFFFUL;
            if (ovf == maxOvf && TCNT1 >= maxRem)         return 0xFFFFFFFFUL;
        }
        return ovf * 65536UL + TCNT1;
    }
};

} // namespace MikroDuino
