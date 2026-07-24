/*
 * Pulse Dual-Mode Station — Benchmark Harness + Signal Analyzer —
 * MikroDuino Module SDK (capstone)
 *
 * Project 6 of 6 in the examples/Modules/Pulse series. Everything the
 * previous five projects introduced, combined into one station with a
 * button-selected mode: BENCH runs project 5's bounded-workload Stopwatch
 * harness continuously; SIGNAL runs projects 3-4's PulseMeter period /
 * frequency / duty-cycle analysis on the same DCMotor-generated loopback
 * signal. The button toggles between them, debounced against a Timer0
 * millis() clock — safe to add because Pulse exclusively uses Timer1,
 * never Timer0 (see Pulse.hpp's conflict table).
 *
 * The one rule this whole series has stood on — Stopwatch and PulseMeter
 * must never run "at the same time" — is not just advisory here, it is
 * load-bearing: BOTH classes reconfigure Timer1's control registers and
 * poll/clear TIFR1's TOV1 flag directly. The g_mode switch below is what
 * makes this safe: exactly one of them is ever called per loop()
 * iteration, and switching modes never leaves the other "still running"
 * in the background — enterMode() only ever changes which branch of
 * loop() executes next, it never leaves a Stopwatch mid-measurement or a
 * PulseMeter call half-blocked.
 *
 * A second, less obvious interaction is why this capstone — like project
 * 5, and UNLIKE project 2 — deliberately does NOT route Timer1's overflow
 * interrupt to Stopwatch::_isrOvf(). PulseMeter polls TIFR1's TOV1 flag
 * and clears it BY HAND in its own measurement loops (see Pulse.hpp/.cpp)
 * — it relies on that flag still being set when it checks. A global
 * ISR(TIMER1_OVF_vect) would fire on every Timer1 overflow and clear TOV1
 * itself first, silently starving every PulseMeter call in SIGNAL mode of
 * the overflow flag it needs. BENCH mode's workload is therefore kept
 * bounded under one overflow period (project 5's strategy), the only
 * choice compatible with SIGNAL mode's PulseMeter calls coexisting in the
 * same program.
 *
 * Also worth being honest about: Pulse has no non-blocking API at all —
 * every Stopwatch/PulseMeter call this project makes is still a single,
 * bounded blocking call. "Responsive to the button" here means each of
 * those calls is individually short (one bench iteration: <~15 ms; one
 * signal read: a few PWM cycles, well under its own timeout), and the
 * button is re-checked once per loop() between them — NOT that
 * measurement happens in the background while other code runs, the way
 * this SDK's SoftTimer-based modules achieve true non-blocking behaviour.
 *
 * Hardware (ATmega328P @ 16 MHz) — connect a jumper wire GEN to MEAS:
 *
 *   ┌──────────────┬───────┬──────────────────────────────────────────┐
 *   │ Mode button   │ PD2   │ Other leg to GND - internal pull-up        │
 *   │ STATUS LED    │ PB5   │ Solid = BENCH running, blinking = SIGNAL   │
 *   │ GEN           │ PD6   │ OC0A - DCMotor PWM output (~977 Hz)        │
 *   │ MEAS          │ PD7   │ PulseMeter input - jumper this to GEN       │
 *   │ TXD           │ PD1   │ USB-serial adapter, 9600 8N1, for dashboard│
 *   └──────────────┴───────┴──────────────────────────────────────────┘
 *
 * Pulse + Stopwatch + PulseMeter concepts reused from projects 1-5:
 *   Stopwatch: start(), stop(), elapsedUs(), reset() bracketing a bounded
 *   workload, running min/max/avg statistics (project 5). PulseMeter:
 *   begin(), period(), pulseWidth(), used together to derive frequency
 *   and duty cycle for one combined status line (projects 3-4).
 */

#include <avr/interrupt.h>
#include <util/delay.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/usart.hpp>
#include <mikroduino/timer.hpp>
#include <mikroduino/registers.hpp>
#include <Pulse.hpp>
#include <DCMotor.hpp>

using namespace MikroDuino;

static constexpr uint8_t BUTTON_PIN = PD2;
static constexpr uint8_t STATUS_LED = PB5;
static constexpr uint8_t GEN_PIN    = PD6;   // OC0A
static constexpr uint8_t MEAS_PIN   = PD7;

Stopwatch  sw;
PulseMeter pm(MEAS_PIN);
DCMotor    gen(PB0, PB1, GEN_PIN);   // in1/in2 unused - see project 3's header comment

// ── millis() clock (Timer0 only - never conflicts with Pulse's Timer1) ──

static constexpr uint16_t MICROS_PER_OVERFLOW = 1024;
static constexpr uint16_t MILLIS_INC = MICROS_PER_OVERFLOW / 1000;
static constexpr uint16_t FRACT_INC  = MICROS_PER_OVERFLOW % 1000;
static constexpr uint16_t FRACT_MAX  = 1000;

static volatile uint32_t g_millis = 0;
static volatile uint16_t g_fract  = 0;

ISR(TIMER0_OVF_vect) {
    uint32_t m = g_millis;
    uint16_t f = g_fract;
    m += MILLIS_INC;
    f += FRACT_INC;
    if (f >= FRACT_MAX) { f -= FRACT_MAX; ++m; }
    g_fract  = f;
    g_millis = m;
}

static uint32_t millis() {
    uint32_t snapshot;
    ATOMIC_BLOCK_START;
    snapshot = g_millis;
    ATOMIC_BLOCK_END;
    return snapshot;
}

// A workload with a known, bounded worst case - identical to project 5.
static void workload(uint16_t iterations) {
    volatile uint32_t acc = 0;
    for (uint16_t i = 0; i < iterations; ++i) {
        acc += i;
    }
}

enum Mode : uint8_t { BENCH, SIGNAL };

static Mode     g_mode         = BENCH;
static bool     g_lastButton   = false;
static uint32_t g_lastChangeMs = 0;

static const uint16_t WORKLOAD_PRESETS[3] = { 1000, 4000, 10000 };
static uint8_t  g_presetIndex = 0;
static uint32_t g_runMin, g_runMax, g_runSum;
static uint16_t g_runCount;
static constexpr uint16_t REPORT_EVERY = 15;

static uint32_t g_lastSignalMs = 0;
static constexpr uint16_t SIGNAL_PERIOD_MS = 300;

static void resetBenchStats() {
    g_runMin   = 0xFFFFFFFFUL;
    g_runMax   = 0;
    g_runSum   = 0;
    g_runCount = 0;
}

static void enterMode(Mode m, uint32_t now) {
    g_mode = m;
    USART0.writeLine_P(PSTR(""));
    USART0.writeLine_P(m == BENCH ? PSTR(">>> MODE: BENCH <<<") : PSTR(">>> MODE: SIGNAL <<<"));
    if (m == BENCH) {
        resetBenchStats();
    } else {
        g_lastSignalMs = now - SIGNAL_PERIOD_MS;   // force an immediate first reading
    }
}

int main() {
    USART0.begin(9600);
    USART0.writeLine_P(PSTR("Pulse dual-mode station: benchmark + signal analyzer"));
    USART0.writeLine_P(PSTR("====================================================="));
    USART0.writeLine_P(PSTR("Jumper GEN (PD6) to MEAS (PD7). Click the button to switch modes."));

    GPIO::inputPullup(BUTTON_PIN);
    GPIO::output(STATUS_LED);
    GPIO::clear(STATUS_LED);

    gen.begin();
    gen.speed(40);   // fixed ~977 Hz carrier, ~40% duty - runs in hardware, no CPU cost
    pm.begin();

    Timer0.mode(TimerMode::Normal);
    Timer0.prescaler(TimerPrescaler::DIV64);
    Timer0.start();
    Timer0.enableOverflow();
    sei();   // Timer1 overflow is NEVER routed to an ISR - see header comment

    resetBenchStats();

    uint32_t nextBlinkAt = millis();
    bool     ledOn       = false;

    while (true) {
        uint32_t now = millis();

        // ---- Button: toggle BENCH <-> SIGNAL, debounced against millis() ----
        bool pressed = (GPIO::read(BUTTON_PIN) == false);
        if (pressed && !g_lastButton && (now - g_lastChangeMs) >= 200) {
            enterMode(g_mode == BENCH ? SIGNAL : BENCH, now);
            g_lastChangeMs = now;
        }
        g_lastButton = pressed;

        // ---- Mode-specific work: exactly one Timer1 user runs per pass ----
        if (g_mode == BENCH) {
            uint16_t iters = WORKLOAD_PRESETS[g_presetIndex];
            g_presetIndex = static_cast<uint8_t>((g_presetIndex + 1) % 3);

            sw.start();
            workload(iters);
            sw.stop();
            uint32_t us = sw.elapsedUs();
            sw.reset();

            if (us < g_runMin) g_runMin = us;
            if (us > g_runMax) g_runMax = us;
            g_runSum += us;
            g_runCount++;

            if (g_runCount >= REPORT_EVERY) {
                USART0.write_P(PSTR("BENCH  min="));
                USART0.writeInt(static_cast<int32_t>(g_runMin));
                USART0.write_P(PSTR("us  max="));
                USART0.writeInt(static_cast<int32_t>(g_runMax));
                USART0.write_P(PSTR("us  avg="));
                USART0.writeInt(static_cast<int32_t>(g_runSum / g_runCount));
                USART0.writeLine_P(PSTR("us"));
                resetBenchStats();
            }

            GPIO::set(STATUS_LED);   // solid while BENCH is active
        } else {   // SIGNAL
            if (now - g_lastSignalMs >= SIGNAL_PERIOD_MS) {
                g_lastSignalMs = now;

                uint32_t periodUs = pm.period(true, 50000UL);
                if (periodUs == 0) {
                    USART0.writeLine_P(PSTR("SIGNAL TIMEOUT (check the GEN-to-MEAS jumper)"));
                } else {
                    uint32_t highUs = pm.pulseWidth(true, 50000UL);
                    uint32_t hz      = 1000000UL / periodUs;
                    uint32_t dutyPct = (highUs * 100UL) / periodUs;

                    USART0.write_P(PSTR("SIGNAL period="));
                    USART0.writeInt(static_cast<int32_t>(periodUs));
                    USART0.write_P(PSTR("us  freq="));
                    USART0.writeInt(static_cast<int32_t>(hz));
                    USART0.write_P(PSTR("Hz  duty="));
                    USART0.writeInt(static_cast<int32_t>(dutyPct));
                    USART0.writeLine_P(PSTR("%"));
                }
            }

            if (now >= nextBlinkAt) {   // blinking while SIGNAL is active
                nextBlinkAt = now + 150;
                ledOn = !ledOn;
                GPIO::write(STATUS_LED, ledOn);
            }
        }
    }
}
