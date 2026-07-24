/*
 * LED Blink — MikroDuino SDK (GPIO basics)
 *
 * The simplest possible MikroDuino program: blink the on-board LED.
 * This is the "Hello World" of embedded development, and project 1 of 7
 * in the examples/gpio series, which walks the GPIO API from a single
 * blinking LED up to a multi-pin traffic light controller.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ Signal  │ Pin   │ Wiring                                    │
 *   ├─────────┼───────┼──────────────────────────────────────────┤
 *   │ LED     │ PB5   │ Built-in LED (Arduino Uno "pin 13") —     │
 *   │         │       │ no external resistor needed.              │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * GPIO concepts introduced:
 *   - Pin constants (PB5, ...) — each encodes a port index and bit number
 *     in a single uint8_t (see the header comment in gpio.hpp).
 *   - GPIO::output(pin) — configure a pin as a digital output.
 *   - GPIO::set(pin) / GPIO::clear(pin) — drive a pin HIGH / LOW.
 */

#include <avr/io.h>
#include <util/delay.h>
#include <mikroduino/gpio.hpp>

using namespace MikroDuino;

static constexpr uint8_t LED = PB5;

int main() {
    // Every pin starts as an input after reset (DDR bit = 0). output()
    // sets the Data Direction Register bit so the pin can drive current
    // instead of just sensing voltage.
    GPIO::output(LED);

    while (true) {
        GPIO::set(LED);      // PORTB bit 5 = 1 -> pin driven HIGH -> LED on
        _delay_ms(500);

        GPIO::clear(LED);    // PORTB bit 5 = 0 -> pin driven LOW  -> LED off
        _delay_ms(500);
    }
}
