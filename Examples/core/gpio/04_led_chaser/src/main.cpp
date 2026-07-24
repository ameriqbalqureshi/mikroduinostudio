/*
 * LED Chaser — MikroDuino SDK (multi-pin sequencing)
 *
 * Project 4 of 7 in the examples/gpio series. Drives six individually
 * wired LEDs in a back-and-forth "Cylon eye" sweep, the classic first
 * project that uses more than one or two GPIO pins at once.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ Signal  │ Pin   │ Wiring                                    │
 *   ├─────────┼───────┼──────────────────────────────────────────┤
 *   │ LED[0]  │ PB0   │ LED + 220 Ω to GND                        │
 *   │ LED[1]  │ PB1   │ LED + 220 Ω to GND                        │
 *   │ LED[2]  │ PB2   │ LED + 220 Ω to GND                        │
 *   │ LED[3]  │ PB3   │ LED + 220 Ω to GND                        │
 *   │ LED[4]  │ PB4   │ LED + 220 Ω to GND                        │
 *   │ LED[5]  │ PB5   │ LED + 220 Ω to GND                        │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * GPIO concepts introduced:
 *   - Storing pin constants in an array and looping over them, so the
 *     sweep logic is independent of exactly which physical pins are used.
 *   - Driving several individually-wired outputs with per-pin set()/
 *     clear() calls, as a contrast to the batch (whole-port) API used in
 *     05_binary_counter.
 */

#include <avr/io.h>
#include <util/delay.h>
#include <mikroduino/gpio.hpp>

using namespace MikroDuino;

static constexpr uint8_t LED[] = { PB0, PB1, PB2, PB3, PB4, PB5 };
static constexpr uint8_t LED_COUNT = sizeof(LED) / sizeof(LED[0]);

static constexpr uint16_t STEP_MS = 80;

static void all_off() {
    for (uint8_t i = 0; i < LED_COUNT; ++i) GPIO::clear(LED[i]);
}

int main() {
    for (uint8_t i = 0; i < LED_COUNT; ++i) GPIO::output(LED[i]);
    all_off();

    while (true) {
        // Sweep left to right: LED[0] -> LED[5]
        for (uint8_t i = 0; i < LED_COUNT; ++i) {
            all_off();
            GPIO::set(LED[i]);
            _delay_ms(STEP_MS);
        }

        // Sweep right to left. Start/stop one step short of each end so
        // the outermost LEDs are not lit twice in a row, which is what
        // gives the pattern its bouncing "eye" look instead of a stutter.
        for (int8_t i = LED_COUNT - 2; i >= 1; --i) {
            all_off();
            GPIO::set(LED[i]);
            _delay_ms(STEP_MS);
        }
    }
}
