/*
 * Binary Counter — MikroDuino SDK (batch/port-level GPIO)
 *
 * Project 5 of 7 in the examples/gpio series. Counts 0-15 on four LEDs
 * wired to the same port, displayed as raw binary. Unlike 04_led_chaser
 * (which drives pins one at a time), this project uses GPIO's batch API
 * to configure and update all four pins with a single register write.
 *
 * A direction button lets you flip between counting up and down without
 * resetting, which doubles as a second demonstration of debounced input
 * reading alongside the port-level output work.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ Signal  │ Pin   │ Wiring                                    │
 *   ├─────────┼───────┼──────────────────────────────────────────┤
 *   │ BIT0    │ PC0   │ LED + 220 Ω to GND (least significant)    │
 *   │ BIT1    │ PC1   │ LED + 220 Ω to GND                        │
 *   │ BIT2    │ PC2   │ LED + 220 Ω to GND                        │
 *   │ BIT3    │ PC3   │ LED + 220 Ω to GND (most significant)     │
 *   │ DIR_BTN │ PD2   │ Momentary button to GND (active-low),     │
 *   │         │       │ internal pull-up enabled.                 │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * GPIO concepts introduced:
 *   - GPIO::portOutput(portIdx, mask) — configures every pin covered by
 *     `mask` as an output in one DDR write, instead of calling output()
 *     four times.
 *   - GPIO::portWrite(portIdx, value) — writes the entire PORT register
 *     in one instruction. Bits outside `mask` were never configured as
 *     outputs, so they are simply masked off before writing.
 *   - Port index vs. pin constant: portOutput/portWrite operate on a
 *     *port* (PORT_C = 2, matching the port field packed into PC0..PC7),
 *     not on individual pin constants like PC0.
 */

#include <avr/io.h>
#include <util/delay.h>
#include <mikroduino/gpio.hpp>

using namespace MikroDuino;

// Port index for PC0-PC7 (see the port/bit encoding comment at the top
// of gpio.hpp: bits[5:3] of a pin constant select this same index).
static constexpr uint8_t PORT_C = 2;
static constexpr uint8_t COUNTER_MASK = 0x0F;   // PC0-PC3 = 4-bit display

static constexpr uint8_t DIR_BTN = PD2;         // held = count down instead of up
static constexpr uint16_t DEBOUNCE_MS = 30;
static constexpr uint16_t STEP_MS = 200;

int main() {
    // One call configures all four DDR bits covered by COUNTER_MASK.
    GPIO::portOutput(PORT_C, COUNTER_MASK);
    GPIO::inputPullup(DIR_BTN);

    uint8_t count = 0;
    bool countDown = false;
    bool lastPressed = false;

    while (true) {
        bool pressed = !GPIO::read(DIR_BTN);   // active-low

        if (pressed && !lastPressed) {
            countDown = !countDown;            // each press flips direction
            _delay_ms(DEBOUNCE_MS);
        }
        lastPressed = pressed;

        // portWrite() writes the whole PORTC byte in a single instruction.
        // COUNTER_MASK keeps this to the low 4 bits; PC4-PC7 were never
        // configured as outputs by portOutput(), so their bits are unused.
        GPIO::portWrite(PORT_C, count & COUNTER_MASK);
        _delay_ms(STEP_MS);

        count = countDown ? static_cast<uint8_t>((count - 1u) & COUNTER_MASK)
                           : static_cast<uint8_t>((count + 1u) & COUNTER_MASK);
    }
}
