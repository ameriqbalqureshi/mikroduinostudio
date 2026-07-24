/*
 * Button Basics — Click Toggles LED — MikroDuino Module SDK
 *
 * The simplest possible use of the Button module: click a button, toggle
 * an LED. This is project 1 of 6 in the examples/Modules/Button series,
 * which walks the Button class from a single click up to a capstone
 * combination-lock UI backed by on-chip EEPROM.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ Button  │ PD2   │ Other leg to GND — Button.begin() enables │
 *   │         │       │ the internal pull-up itself, no manual    │
 *   │         │       │ GPIO::inputPullup() call needed            │
 *   │ LED     │ PB5   │ Toggles once per click                    │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * Button vs. the raw GPIO polling every gpio/i2c/spi/pwm/timer example in
 * this SDK has used for buttons so far: those all hand-rolled a debounce
 * (a settle delay, or a consecutive-low-sample counter). Button packages
 * a complete, configurable debounce PLUS press/release/click/double-click/
 * long-press GESTURE detection behind five one-shot event methods — this
 * project uses only the simplest of them, clicked().
 *
 * Timing model — the one rule this whole series depends on: Button::update()
 * must be called at a known, fixed interval, because its internal timers
 * (debounce, hold time, double-click window) count CALLS, not real time.
 * The module's own header comment recommends the simplest possible
 * approach — call it once per millisecond from a tight busy-wait — and
 * that's exactly what this first project does. Project 3 switches to a
 * non-blocking millis()-scheduled call instead, once the busy-wait
 * approach's limitation (main() can't do anything else) becomes a real
 * problem to solve rather than a hypothetical one.
 *
 * Button concepts introduced:
 *   - Button(pin) — the default constructor: activeLow=true (idle HIGH,
 *     pressed pulls LOW), debounceMs=20, longPressMs=600,
 *     doubleClickMs=350. Every default is tuned for a typical mechanical
 *     tactile button; project 3 and 4 show overriding them.
 *   - begin() — configures the pin (internal pull-up, since this button
 *     is active-low) and takes an initial reading.
 *   - update() — advances Button's internal debounce/timing state by
 *     "one tick". Must be called every ~1 ms for the Ms-suffixed
 *     parameters to mean what their names say.
 *   - clicked() — a ONE-SHOT event: returns true exactly once when a
 *     complete short press-then-release has happened (and did NOT turn
 *     into a long press — see project 3), then automatically clears
 *     itself. Calling it again immediately afterward returns false until
 *     the next real click.
 */

#include <util/delay.h>
#include <mikroduino/gpio.hpp>
#include <Button.hpp>

using namespace MikroDuino;

static constexpr uint8_t LED = PB5;

Button button(PD2);   // defaults: active-low, 20 ms debounce, 600 ms long-press, 350 ms double-click

int main() {
    GPIO::output(LED);
    GPIO::clear(LED);

    button.begin();

    while (true) {
        button.update();

        if (button.clicked()) {
            GPIO::toggle(LED);
        }

        _delay_ms(1);   // Button::update() expects to be called ~every 1 ms
    }
}
