/*
 * Button Events & State Queries — MikroDuino Module SDK
 *
 * Project 2 of 6 in the examples/Modules/Button series. Same wiring as
 * project 1, but now narrates every event Button offers except the two
 * saved for later projects (doubleClicked() — project 4; longPressed() —
 * project 3), and introduces the two no-side-effect STATE queries
 * alongside the one-shot EVENT methods.
 *
 * Hardware: identical to project 1 — button on PD2 (internal pull-up via
 * Button::begin()), LED on PB5, plus a USB-serial adapter on PD1 (TXD)
 * at 9600 8N1 for the commentary.
 *
 * The event/state distinction, made concrete:
 *   - pressed() / released() / clicked() are EVENTS: each returns true
 *     exactly once per occurrence, then clears itself. Call one twice in
 *     a row without an intervening real button action and the second
 *     call returns false. This project's main loop calls each of them
 *     once per update() cycle specifically so no event is ever missed
 *     between polls.
 *   - isDown() / heldMs() are STATE: no side effect, safe to call as
 *     many times as you like, always reflecting Button's current
 *     understanding of the world. This project polls isDown() every
 *     loop pass and heldMs() periodically while held, neither of which
 *     "consumes" anything.
 *
 * Button concepts introduced:
 *   - pressed() — fires the instant a debounced press is recognised
 *     (i.e. immediately, not waiting to see if it becomes a click, a
 *     double-click, or a long-press — those are all decided later).
 *   - released() — the debounced-release counterpart. Note it fires on
 *     EVERY release, including ones that turn out to be part of a
 *     double-click or that followed a long press — clicked() is the one
 *     that's conditional on what kind of press/release pair it was, not
 *     released().
 *   - isDown() — true for the entire time the button reads as pressed,
 *     from the pressed() event until the released() event.
 *   - heldMs() — milliseconds held so far. Still meaningful to read right
 *     as released() fires (see this file's on-release printout) — it
 *     reflects the final held duration at that instant, not a
 *     mid-press-only value.
 *
 * Button concept reused from project 1:
 *   - Button(pin), begin(), update(), clicked().
 */

#include <avr/pgmspace.h>
#include <util/delay.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/usart.hpp>
#include <Button.hpp>

using namespace MikroDuino;

static constexpr uint8_t LED = PB5;

Button button(PD2);

int main() {
    GPIO::output(LED);
    GPIO::clear(LED);

    button.begin();

    USART0.begin(9600);
    USART0.writeLine_P(PSTR("Button events & state queries"));
    USART0.writeLine_P(PSTR("================================"));

    uint16_t msSinceLastHeldPrint = 0;

    while (true) {
        button.update();

        // --- Events: each checked once per cycle, each self-clearing ---
        if (button.pressed()) {
            USART0.writeLine_P(PSTR("pressed()  -> DOWN"));
            GPIO::set(LED);
            msSinceLastHeldPrint = 0;
        }

        if (button.clicked()) {
            USART0.writeLine_P(PSTR("clicked()  -> short press+release recognised"));
        }

        if (button.released()) {
            USART0.write_P(PSTR("released() -> UP (was held "));
            USART0.writeInt(static_cast<int32_t>(button.heldMs()));
            USART0.writeLine_P(PSTR(" ms)"));
            GPIO::clear(LED);
        }

        // --- State: safe to poll every cycle, no side effect ---
        if (button.isDown()) {
            // Print a live held-time readout roughly every 100 ms while
            // down, instead of flooding the terminal at the full 1 kHz
            // update() rate.
            if (++msSinceLastHeldPrint >= 100) {
                msSinceLastHeldPrint = 0;
                USART0.write_P(PSTR("  ...still down, heldMs()="));
                USART0.writeInt(static_cast<int32_t>(button.heldMs()));
                USART0.writeLine_P(PSTR(""));
            }
        }

        _delay_ms(1);
    }
}
