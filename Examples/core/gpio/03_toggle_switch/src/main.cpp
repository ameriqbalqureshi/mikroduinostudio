/*
 * Toggle Switch — MikroDuino SDK (edge detection + software debounce)
 *
 * Project 3 of 7 in the examples/gpio series. Unlike 02_button_led (LED
 * follows the button's held state), here each *press* flips the LED's
 * state, like a light switch. That requires two new ideas on top of the
 * raw GPIO API: edge detection and debouncing.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ Signal  │ Pin   │ Wiring                                    │
 *   ├─────────┼───────┼──────────────────────────────────────────┤
 *   │ LED     │ PB5   │ Built-in LED                              │
 *   │ BTN     │ PD2   │ Momentary button to GND (active-low),     │
 *   │         │       │ internal pull-up enabled.                 │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * GPIO concepts introduced:
 *   - Edge detection by polling: GPIO::read() only reports the pin's
 *     *current* level. To notice a press (not just "is it down right
 *     now") the code must remember the previous sample and compare.
 *   - Software debounce: a mechanical switch's contacts physically
 *     bounce for a few milliseconds after being pressed, which a fast
 *     enough poll loop would see as several rapid presses. A short delay
 *     after each accepted edge rides out the bounce.
 *   - GPIO::toggle(pin) — flips a pin's output level using the AVR's
 *     PINx-register write trick, a single atomic operation rather than a
 *     read-modify-write on PORTx.
 */

#include <avr/io.h>
#include <util/delay.h>
#include <mikroduino/gpio.hpp>

using namespace MikroDuino;

static constexpr uint8_t LED = PB5;
static constexpr uint8_t BTN = PD2;

// How long to ignore further transitions after accepting a press edge.
// 20-50 ms comfortably covers typical tactile-switch bounce.
static constexpr uint16_t DEBOUNCE_MS = 30;

int main() {
    GPIO::output(LED);
    GPIO::inputPullup(BTN);
    GPIO::clear(LED);

    bool lastPressed = false;   // button state observed on the previous poll

    while (true) {
        bool pressed = !GPIO::read(BTN);   // active-low -> invert

        if (pressed && !lastPressed) {
            // Falling edge: button just went from released to pressed.
            GPIO::toggle(LED);
            _delay_ms(DEBOUNCE_MS);        // ride out contact bounce
        }

        lastPressed = pressed;
        _delay_ms(5);   // poll rate; fast enough not to miss a human press
    }
}
