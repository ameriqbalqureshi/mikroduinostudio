/*
 * Pulse — Button Hold Duration with ISR-Extended Range — MikroDuino
 * Module SDK
 *
 * Project 2 of 6 in the examples/Modules/Pulse series. Project 1 kept
 * every measurement safely under Stopwatch's 32.768 ms no-ISR range by
 * construction. A real button hold has no such guarantee — even a quick
 * tap is often 100-300 ms, and a deliberate hold can run for seconds —
 * so this project measures exactly that (press-to-release duration) and
 * routes Timer1's overflow interrupt to Stopwatch, exactly as its own
 * header comment prescribes, to extend the accurate range to roughly
 * 35 minutes.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ Button  │ PD2   │ Other leg to GND — internal pull-up enabled│
 *   │ LED     │ PB5   │ Lit for as long as the button is held      │
 *   │ TXD     │ PD1   │ USB-serial adapter, 9600 8N1, for readings │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * This project reads the button with plain GPIO + a short settle delay
 * rather than the SDK's Button module — this series stays focused on
 * Pulse concepts, and a hold-duration measurement only needs to know
 * "pressed" vs "not pressed", not Button's click/double-click/long-press
 * vocabulary.
 *
 * Stopwatch concepts introduced:
 *   - ISR(TIMER1_OVF_vect) { sw._isrOvf(); } — routes Timer1's overflow
 *     interrupt to the Stopwatch instance being timed. Without this, a
 *     hold longer than one overflow period (32.768 ms) would silently
 *     read back short — _rawTicks()'s "late-OVF correction" (see
 *     Pulse.hpp) can only ever detect ONE pending overflow on its own,
 *     not count how many happened while nobody was looking.
 *   - _isrOvf() / the static _ovf counter — internal bookkeeping this
 *     project never touches directly; the ISR one-liner above is the
 *     entire integration surface.
 *   - start() / stop() bracketing a real external event (button press to
 *     release) instead of project 1's fixed busy-waits — the same two
 *     calls, now driven by GPIO state changes instead of code position.
 */

#include <avr/interrupt.h>
#include <util/delay.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/usart.hpp>
#include <Pulse.hpp>

using namespace MikroDuino;

static constexpr uint8_t BUTTON_PIN = PD2;
static constexpr uint8_t LED        = PB5;

Stopwatch sw;

// Required for holds beyond one Timer1 overflow period (32.768 ms @ 16 MHz).
ISR(TIMER1_OVF_vect) {
    sw._isrOvf();
}

int main() {
    USART0.begin(9600);
    USART0.writeLine_P(PSTR("Pulse: button hold duration (ISR-extended range)"));
    USART0.writeLine_P(PSTR("================================================="));
    USART0.writeLine_P(PSTR("Hold the button; release to see the measured duration"));
    USART0.writeLine_P(PSTR(""));

    GPIO::inputPullup(BUTTON_PIN);
    GPIO::output(LED);
    GPIO::clear(LED);
    sei();   // enable global interrupts so TIMER1_OVF_vect can fire

    bool wasDown = false;

    while (true) {
        bool isDown = !GPIO::read(BUTTON_PIN);   // active-low

        if (isDown && !wasDown) {
            _delay_ms(20);                        // debounce settle
            if (!GPIO::read(BUTTON_PIN)) {
                sw.start();
                GPIO::set(LED);
                USART0.writeLine_P(PSTR("pressed..."));
            } else {
                isDown = false;                    // was just contact bounce
            }
        } else if (!isDown && wasDown) {
            _delay_ms(20);                        // debounce settle
            if (GPIO::read(BUTTON_PIN)) {
                sw.stop();
                GPIO::clear(LED);
                USART0.write_P(PSTR("released - held for "));
                USART0.writeInt(static_cast<int32_t>(sw.elapsedMs()));
                USART0.writeLine_P(PSTR(" ms"));
                sw.reset();
            } else {
                isDown = true;                     // was just contact bounce, still held
            }
        }

        wasDown = isDown;
        _delay_ms(1);
    }
}
