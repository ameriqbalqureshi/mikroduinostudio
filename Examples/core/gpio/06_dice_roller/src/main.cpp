/*
 * Dice Roller — MikroDuino SDK (state machine + pseudo-randomness)
 *
 * Project 6 of 7 in the examples/gpio series. Hold the button to "shake"
 * the dice — the three LEDs flicker through fast-changing binary values —
 * then release to lock in a result from 1-6, held for two seconds.
 *
 * This is the first project in the series with more than a flat loop: it
 * is a small state machine (WAIT -> SHAKE -> LOCK-IN -> WAIT ...), and it
 * generates its "randomness" purely from GPIO polling timing, with no
 * ADC, timer peripheral, or C library rand() involved.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ Signal  │ Pin   │ Wiring                                    │
 *   ├─────────┼───────┼──────────────────────────────────────────┤
 *   │ DICE[0] │ PC0   │ LED + 220 Ω to GND (bit 0 of result)      │
 *   │ DICE[1] │ PC1   │ LED + 220 Ω to GND (bit 1)                │
 *   │ DICE[2] │ PC2   │ LED + 220 Ω to GND (bit 2; values 1-6 fit │
 *   │         │       │ in 3 bits)                                │
 *   │ ROLL_BTN│ PD2   │ Momentary button to GND (active-low),     │
 *   │         │       │ internal pull-up enabled.                 │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * GPIO concepts introduced:
 *   - Combining portOutput()/portWrite() (batch API, as in
 *     05_binary_counter) with polled digital input to build a multi-phase
 *     interactive program.
 *   - Software pseudo-randomness from GPIO-driven timing: a free-running
 *     counter is stepped every pass through a tight polling loop. Its
 *     value has no relation to wall-clock time, so a human's reaction
 *     time when releasing the button samples it at an effectively
 *     unpredictable point — even though the counter itself follows a
 *     fully deterministic sequence.
 */

#include <avr/io.h>
#include <util/delay.h>
#include <mikroduino/gpio.hpp>

using namespace MikroDuino;

static constexpr uint8_t PORT_C    = 2;
static constexpr uint8_t DICE_MASK = 0x07;   // PC0-PC2
static constexpr uint8_t ROLL_BTN  = PD2;    // active-low, pull-up

static constexpr uint16_t DEBOUNCE_MS = 30;
static constexpr uint16_t SHAKE_STEP_MS = 60;
static constexpr uint16_t RESULT_HOLD_MS = 2000;

// Stepped once per poll. The exact instant a human releases the button
// relative to this fast-changing counter is what supplies the
// "randomness" -- the generator itself is a simple linear congruential
// step (constants from Numerical Recipes), not a hardware RNG.
static uint16_t seed = 1;

static void step_seed() {
    seed = static_cast<uint16_t>(seed * 25173u + 13849u);
}

static void delay_ms(uint16_t ms) {
    while (ms--) _delay_ms(1);
}

int main() {
    GPIO::portOutput(PORT_C, DICE_MASK);
    GPIO::inputPullup(ROLL_BTN);
    GPIO::portWrite(PORT_C, 0x00);

    while (true) {
        // ---- WAIT: idle until pressed, stirring the seed the whole time ----
        while (GPIO::read(ROLL_BTN)) {    // HIGH = not pressed (active-low)
            step_seed();
        }
        delay_ms(DEBOUNCE_MS);            // debounce the press edge

        // ---- SHAKE: flicker while the button stays held ----
        while (!GPIO::read(ROLL_BTN)) {   // LOW = still held
            step_seed();
            GPIO::portWrite(PORT_C, (seed >> 8) & DICE_MASK);
            delay_ms(SHAKE_STEP_MS);
        }
        delay_ms(DEBOUNCE_MS);            // debounce the release edge

        // ---- LOCK IN: map the seed to 1-6 and hold the result ----
        uint8_t result = static_cast<uint8_t>((seed % 6u) + 1u);
        GPIO::portWrite(PORT_C, result & DICE_MASK);
        delay_ms(RESULT_HOLD_MS);

        GPIO::portWrite(PORT_C, 0x00);
    }
}
