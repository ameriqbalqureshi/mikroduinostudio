/*
 * Pulse — Execution-Time Benchmark Harness — MikroDuino Module SDK
 *
 * Project 5 of 6 in the examples/Modules/Pulse series. Project 2 handled
 * Stopwatch's 32.768 ms no-ISR range limit by EXTENDING it with an ISR.
 * This project handles the exact same limit the opposite, equally valid
 * way: by construction, every workload this harness times is bounded
 * well under one Timer1 overflow period, so no ISR is used at all. Which
 * strategy fits a given project depends entirely on whether what you are
 * timing has a hard ceiling you control (this project's case) or is an
 * open-ended real-world event like a button hold (project 2's case).
 *
 * The harness itself is a real, reusable pattern: bracket a variable-cost
 * workload with start()/stop() on every call, and maintain running
 * min/max/average statistics instead of printing every single reading —
 * exactly how you would benchmark a candidate function change on actual
 * hardware, where cache-free, interrupt-free AVR execution time is far
 * more consistent than run-to-run timing on a general-purpose CPU, but
 * still varies with the input.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ TXD     │ PD1   │ USB-serial adapter, 9600 8N1, for readings │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * Stopwatch concepts introduced:
 *   - Repeated start()/stop()/elapsedUs()/reset() cycles around a loop
 *     body, one bracket per iteration, rather than projects 1-2's single
 *     one-shot measurement — the same four calls, now driven in a tight
 *     cycle instead of once.
 *   - A workload with a KNOWN worst case (WORKLOAD_ITERS' largest preset,
 *     chosen deliberately small enough to stay under ~20 ms even
 *     accounting for compiler/optimization variance) is what makes
 *     skipping the ISR extension a deliberate, verifiable design choice
 *     here rather than an oversight — contrast with project 2, where a
 *     button hold has no such ceiling and the ISR is mandatory.
 *   - Running statistics (min/max/sum-for-average) computed from
 *     repeated elapsedUs() readings, printed periodically rather than
 *     per-iteration, so the report stays readable across many samples.
 */

#include <util/delay.h>
#include <mikroduino/usart.hpp>
#include <Pulse.hpp>

using namespace MikroDuino;

Stopwatch sw;

// A workload with a known, bounded worst case — a busy accumulate loop
// whose iteration count is the only variable. volatile forces the
// compiler to actually perform every iteration rather than optimising
// the loop away.
static void workload(uint16_t iterations) {
    volatile uint32_t acc = 0;
    for (uint16_t i = 0; i < iterations; ++i) {
        acc += i;
    }
}

int main() {
    USART0.begin(9600);
    USART0.writeLine_P(PSTR("Pulse: execution-time benchmark harness"));
    USART0.writeLine_P(PSTR("========================================"));
    USART0.writeLine_P(PSTR(""));

    // Largest preset is chosen to stay comfortably under Stopwatch's
    // 32.768 ms no-ISR range even with optimisation/timing variance.
    static const uint16_t WORKLOAD_PRESETS[3] = { 1000, 4000, 10000 };
    uint8_t presetIndex = 0;

    uint32_t runMin   = 0xFFFFFFFFUL;
    uint32_t runMax    = 0;
    uint32_t runSum    = 0;
    uint16_t runCount  = 0;

    static constexpr uint16_t REPORT_EVERY = 15;   // iterations between printed reports

    while (true) {
        uint16_t iters = WORKLOAD_PRESETS[presetIndex];
        presetIndex = static_cast<uint8_t>((presetIndex + 1) % 3);

        sw.start();
        workload(iters);
        sw.stop();

        uint32_t us = sw.elapsedUs();
        sw.reset();

        if (us < runMin) runMin = us;
        if (us > runMax) runMax = us;
        runSum += us;
        runCount++;

        if (runCount >= REPORT_EVERY) {
            USART0.write_P(PSTR("last="));
            USART0.writeInt(static_cast<int32_t>(us));
            USART0.write_P(PSTR("us  min="));
            USART0.writeInt(static_cast<int32_t>(runMin));
            USART0.write_P(PSTR("us  max="));
            USART0.writeInt(static_cast<int32_t>(runMax));
            USART0.write_P(PSTR("us  avg="));
            USART0.writeInt(static_cast<int32_t>(runSum / runCount));
            USART0.write_P(PSTR("us  (over "));
            USART0.writeInt(runCount);
            USART0.writeLine_P(PSTR(" samples)"));

            runMin   = 0xFFFFFFFFUL;
            runMax   = 0;
            runSum   = 0;
            runCount = 0;
        }

        _delay_ms(20);
    }
}
