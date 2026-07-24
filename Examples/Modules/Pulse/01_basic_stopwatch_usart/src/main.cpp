/*
 * Pulse Basics — Stopwatch Sanity Check — MikroDuino Module SDK
 *
 * The simplest possible use of the Pulse module's Stopwatch class: time a
 * few known-duration busy-waits and print how closely the measured time
 * matches the expected one. No external wiring is needed for this
 * project — it is entirely self-verifying. This is project 1 of 6 in the
 * examples/Modules/Pulse series, which walks Stopwatch and PulseMeter
 * from a single on-demand measurement up to a capstone station that
 * switches between a Stopwatch-based benchmark harness and a
 * PulseMeter-based signal analyzer.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ TXD     │ PD1   │ USB-serial adapter, 9600 8N1, for readings │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * Pulse module note: both Stopwatch and PulseMeter exclusively own
 * Timer1 while active (see Pulse.hpp's header comment) — this SDK's
 * Servo and PWM1 modules also use Timer1, so none of them may run at the
 * same time as a Pulse class instance. Every project in this series
 * avoids Timer1 entirely outside of Stopwatch/PulseMeter for exactly
 * this reason.
 *
 * Stopwatch concepts introduced:
 *   - Stopwatch() — default-constructed, not running.
 *   - start() — configures and enables Timer1 (prescaler 8), begins
 *     counting from zero. Timer1 draws no power and generates no
 *     interrupts until start() is called.
 *   - elapsedUs() / elapsedMs() — read the elapsed time so far. Safe to
 *     call while still running (no need to stop() first) — this project
 *     always stops first anyway, purely to make "the measurement is
 *     over" explicit in the code.
 *   - stop() — freezes the elapsed value and disables Timer1 again.
 *   - Range without an ISR: ONE Timer1 overflow period — 32.768 ms at
 *     16 MHz (see Pulse.hpp). Every busy-wait this project times is kept
 *     well under that, deliberately, so this first project needs no ISR
 *     at all. Project 2 measures something that routinely exceeds this
 *     range and shows the ISR hookup that lifts the limit.
 */

#include <util/delay.h>
#include <mikroduino/usart.hpp>
#include <Pulse.hpp>

using namespace MikroDuino;

Stopwatch sw;

int main() {
    USART0.begin(9600);
    USART0.writeLine_P(PSTR("Pulse basics: Stopwatch sanity check"));
    USART0.writeLine_P(PSTR("====================================="));
    USART0.writeLine_P(PSTR("Times a few known busy-waits; measured should track expected closely"));
    USART0.writeLine_P(PSTR(""));

    while (true) {
        sw.start();
        _delay_us(500);
        sw.stop();
        USART0.write_P(PSTR("expected=500us    measured="));
        USART0.writeInt(static_cast<int32_t>(sw.elapsedUs()));
        USART0.writeLine_P(PSTR("us"));

        sw.start();
        _delay_ms(5);
        sw.stop();
        USART0.write_P(PSTR("expected=5000us   measured="));
        USART0.writeInt(static_cast<int32_t>(sw.elapsedUs()));
        USART0.writeLine_P(PSTR("us"));

        sw.start();
        _delay_ms(20);
        sw.stop();
        USART0.write_P(PSTR("expected=20000us  measured="));
        USART0.writeInt(static_cast<int32_t>(sw.elapsedUs()));
        USART0.writeLine_P(PSTR("us"));

        USART0.writeLine_P(PSTR(""));
        _delay_ms(1000);
    }
}
