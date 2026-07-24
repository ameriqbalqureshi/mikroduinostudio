/*
 * Button LED — MikroDuino SDK (GPIO digital input)
 *
 * Project 2 of 7 in the examples/gpio series. Builds on 01_led_blink by
 * reading a digital input: the LED lights only while a button is held.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ Signal  │ Pin   │ Wiring                                    │
 *   ├─────────┼───────┼──────────────────────────────────────────┤
 *   │ LED     │ PB5   │ Built-in LED                              │
 *   │ BTN     │ PD2   │ Momentary button to GND (active-low);     │
 *   │         │       │ uses the ATmega's internal pull-up, so no │
 *   │         │       │ external resistor is required.            │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * GPIO concepts introduced:
 *   - GPIO::inputPullup(pin) — configure a pin as input with the internal
 *     pull-up resistor enabled. With nothing pressed, the pin reads HIGH;
 *     pressing the button connects it to GND, so it reads LOW.
 *   - GPIO::read(pin) — sample the current logic level of a pin.
 *   - GPIO::write(pin, bool) — set a pin HIGH/LOW from a computed condition,
 *     instead of calling set()/clear() explicitly.
 */

#include <avr/io.h>
#include <util/delay.h>
#include <mikroduino/gpio.hpp>

using namespace MikroDuino;

static constexpr uint8_t LED = PB5;
static constexpr uint8_t BTN = PD2;

int main() {
    GPIO::output(LED);
    GPIO::inputPullup(BTN);   // pull-up enabled: idle = HIGH, pressed = LOW

    while (true) {
        // BTN is active-low (wired to GND), so invert read() to get a
        // `pressed` flag that reads true exactly when the button is down.
        bool pressed = !GPIO::read(BTN);
        GPIO::write(LED, pressed);

        // A short poll delay is enough here since we only care about the
        // button's steady state (held / not held), not exact edges.
        _delay_ms(10);
    }
}
