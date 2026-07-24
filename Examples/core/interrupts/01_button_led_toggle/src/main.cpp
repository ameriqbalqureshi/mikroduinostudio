/*
 * External Interrupt Basics — Button Toggles LED — MikroDuino SDK
 *
 * The simplest possible external-interrupt program: pressing a button
 * toggles an LED, with main() doing absolutely nothing but idling. This
 * is project 1 of 6 in the examples/interrupts series, which walks the
 * InterruptManager API from a single button press up to a capstone
 * two-sensor alarm system state machine.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ Signal  │ Pin   │ Wiring                                    │
 *   ├─────────┼───────┼──────────────────────────────────────────┤
 *   │ Button  │ PD2   │ Other leg to GND, internal pull-up — this │
 *   │         │       │ is the ATmega328P's dedicated INT0 pin,   │
 *   │         │       │ not a general GPIO pin used for polling.  │
 *   │ LED     │ PB5   │ Toggled entirely from inside the ISR       │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * How this differs from every button in the gpio/i2c/spi/pwm/timer
 * series so far: every one of those polled GPIO::read(BUTTON) somewhere
 * in a loop — main() had to keep visiting the pin to notice a press.
 * INT0 and INT1 are different: they're dedicated hardware lines wired
 * directly into the CPU's interrupt controller. The instant the pin's
 * voltage crosses the configured trigger condition, the hardware itself
 * pauses whatever main() was doing, jumps to the matching ISR, and
 * resumes main() exactly where it left off afterward — main() never has
 * to ask.
 *
 * Interrupt concepts introduced:
 *   - IntSource::INT0 — selects the ATmega328P's INT0 hardware line
 *     (fixed to PD2; INT1 is fixed to PD3 — see project 2). Not every
 *     pin can do this, unlike GPIO's pin-change interrupts on other
 *     ports, which this SDK does not currently wrap.
 *   - IntSense::Falling — the trigger condition: fire only on a
 *     high-to-low transition, i.e. the instant the button is pressed
 *     (with the internal pull-up, idle = HIGH, pressed = LOW). Project 3
 *     tours all four available sense modes.
 *   - Interrupt.attach(source, handler, sense) — registers a plain
 *     function pointer as the handler for a source, sets its trigger
 *     condition, and enables that specific interrupt line (EIMSK), all
 *     in one call.
 *   - Interrupt.enableGlobal() — the global interrupt enable (sei()).
 *     Even with a source attached and its own line enabled, nothing
 *     fires until this is also set — the same two-level gate every
 *     interrupt in the timer series needed (per-source enable AND the
 *     global flag).
 *   - A plain function used as a handler (onButtonPress below) rather
 *     than an ISR(...) macro — InterruptManager owns the actual
 *     INT0_vect/INT1_vect ISR bodies (defined once in interrupt.cpp) and
 *     dispatches to whatever handler was attach()-ed. This is why
 *     interrupt.cpp must be built alongside this file — see this
 *     project's .mdp sourceFiles.
 */

#include <mikroduino/gpio.hpp>
#include <mikroduino/interrupt.hpp>

using namespace MikroDuino;

static constexpr uint8_t LED    = PB5;
static constexpr uint8_t BUTTON = PD2;   // INT0 — fixed hardware pin, not relocatable

// Kept deliberately tiny, the same rule every ISR in the timer series
// followed: do the minimum possible and let main() (or, having none here
// at all, simply nothing) handle the rest.
void onButtonPress() {
    GPIO::toggle(LED);
}

int main() {
    GPIO::output(LED);
    GPIO::clear(LED);
    GPIO::inputPullup(BUTTON);

    Interrupt.attach(IntSource::INT0, onButtonPress, IntSense::Falling);
    Interrupt.enableGlobal();

    // main() has nothing left to do. Every gpio/usart/i2c/spi/pwm/timer
    // example so far needed a while(true) loop with SOMETHING in it —
    // at minimum a poll, usually a delay or a scheduled task. This one
    // doesn't: the LED responds to the button purely through hardware
    // and the ISR above, with main() completely idle.
    while (true) {}
}
