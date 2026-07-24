/*
 * Timer Basics — CTC Interrupt-Driven Blink — MikroDuino SDK
 *
 * The simplest possible timer program: blink an LED at an exact 1 Hz
 * rate using a hardware-timed interrupt instead of _delay_ms(). This is
 * project 1 of 6 in the examples/timer series, which walks the
 * Timer0Driver/Timer1Driver API from a single precise blink up to a
 * capstone that runs a small cooperative scheduler off one hardware
 * timer while a second timer profiles it.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ Signal  │ Pin   │ Wiring                                    │
 *   ├─────────┼───────┼──────────────────────────────────────────┤
 *   │ LED     │ PB5   │ Built-in LED — toggled from inside an ISR │
 *   │ TXD     │ PD1   │ USB-serial adapter                        │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * Why a hardware timer instead of examples/gpio/01_led_blink's
 * _delay_ms(500)? _delay_ms() is a busy-wait: the CPU does nothing else
 * for exactly that half-second, every half-second. A timer interrupt
 * frees the CPU to do other work (this project proves it by counting
 * idle loop iterations in main() and reporting them over USART) while
 * the blink itself stays exactly on schedule, driven entirely by
 * hardware counting clock cycles in the background.
 *
 * CTC (Clear Timer on Compare) mode: the counter counts 0, 1, 2, ...
 * up to whatever value is in the compare register, then resets to 0 and
 * raises a "compare match" interrupt — a hardware-timed periodic tick
 * with a period you choose exactly, unlike Timer0's natural 0-255
 * overflow period (see project 2, which deliberately uses that
 * less-convenient overflow instead, for a different reason).
 *
 * Timer concepts introduced:
 *   - Timer1.mode(TimerMode::CTC) — selects CTC waveform generation
 *     (WGM12 bit), where OCR1A defines the counter's TOP/reset point.
 *   - Timer1.prescaler(TimerPrescaler::DIV64) — divides F_CPU by 64
 *     before it reaches the counter, so each tick is 64/16 MHz = 4 us,
 *     letting a 16-bit compare register reach useful periods (a
 *     1 Hz-derived count would overflow 16 bits with no prescaling at
 *     all: 16,000,000 > 65,536).
 *   - Timer1.ticksForHz(hz) — computes the OCR1A value for a target
 *     frequency, using whatever prescaler was just set: F_CPU /
 *     (prescaler * hz) - 1. Saves doing that arithmetic by hand.
 *   - Timer1.compareA(value) — writes OCR1A, the compare/TOP register.
 *   - Timer1.start() — actually applies the mode+prescaler to
 *     TCCR1A/TCCR1B and starts the counter running.
 *   - Timer1.enableInterruptA() — sets OCIE1A in TIMSK1, so a compare
 *     match raises the TIMER1_COMPA_vect interrupt instead of just
 *     silently setting a flag (contrast: project 5 uses the flag
 *     without an interrupt, by polling compareAFlag() directly).
 *   - ISR(TIMER1_COMPA_vect) — plain avr-libc interrupt handler. Kept
 *     deliberately tiny (toggle one GPIO pin) — the golden rule for any
 *     ISR is to do the minimum possible and let main() handle everything
 *     that isn't time-critical.
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/timer.hpp>
#include <mikroduino/usart.hpp>

using namespace MikroDuino;

static constexpr uint8_t LED = PB5;

// Fires once per second, exactly on Timer1's CTC schedule. Kept to the
// bare minimum: toggle the pin and return — see the header comment on
// why ISRs should stay this small.
ISR(TIMER1_COMPA_vect) {
    GPIO::toggle(LED);
}

int main() {
    GPIO::output(LED);

    USART0.begin(9600);
    USART0.writeLine_P(PSTR("Timer1 CTC: precise 1 Hz interrupt-driven blink"));
    USART0.writeLine_P(PSTR("================================================="));

    // Configure Timer1 for CTC mode at a 64x prescale, then ask it for
    // the exact compare value that yields a 1 Hz compare-match rate.
    Timer1.mode(TimerMode::CTC);
    Timer1.prescaler(TimerPrescaler::DIV64);
    Timer1.compareA(Timer1.ticksForHz(1));   // 1 Hz -> compare match once per second
    Timer1.start();
    Timer1.enableInterruptA();

    sei();   // global interrupt enable — required for any ISR to fire

    USART0.write_P(PSTR("OCR1A = "));
    USART0.writeInt(static_cast<int32_t>(OCR1A));
    USART0.writeLine_P(PSTR(" ticks per compare match (at 4 us/tick, DIV64)"));
    USART0.writeLine_P(PSTR("Watch the LED blink exactly once per second."));
    USART0.writeLine_P(PSTR(""));

    // Proves the CPU isn't blocked waiting on the blink: this loop spins
    // as fast as it can and reports how many iterations it manages
    // between each USART line, all while the LED keeps blinking on
    // schedule via the ISR above, completely independent of this loop's
    // speed or timing.
    uint32_t idleSpins = 0;
    while (true) {
        ++idleSpins;

        if (idleSpins % 2000000UL == 0) {
            USART0.write_P(PSTR("main() is still free to run: "));
            USART0.writeInt(static_cast<int32_t>(idleSpins));
            USART0.writeLine_P(PSTR(" idle spins so far"));
        }
    }
}
