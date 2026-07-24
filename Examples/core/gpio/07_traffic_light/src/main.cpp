/*
 * Traffic Light Controller — MikroDuino SDK (GPIO capstone)
 *
 * Project 7 of 7 in the examples/gpio series. Drives two traffic-light
 * heads (main road + side road/crosswalk) through a safe state machine,
 * with a pedestrian call button that requests a crossing. The state
 * machine guarantees a safe sequence: it is never possible for both
 * roads to show green, and a road always passes through yellow before
 * stopping — the button can only ever *request* a transition, never
 * skip a state.
 *
 * This project deliberately uses only what earlier projects in the
 * series introduced (output/input/read/write, debounced polling) at
 * larger scale: seven pins, a six-way state sequence, and a helper that
 * sets all six lights from one call.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌─────────────┬───────┬──────────────────────────────────────────┐
 *   │ Signal      │ Pin   │ Wiring                                    │
 *   ├─────────────┼───────┼──────────────────────────────────────────┤
 *   │ MAIN_RED    │ PB0   │ LED + 220 Ω to GND                        │
 *   │ MAIN_YELLOW │ PB1   │ LED + 220 Ω to GND                        │
 *   │ MAIN_GREEN  │ PB2   │ LED + 220 Ω to GND                        │
 *   │ SIDE_RED    │ PB3   │ LED + 220 Ω to GND                        │
 *   │ SIDE_YELLOW │ PB4   │ LED + 220 Ω to GND                        │
 *   │ SIDE_GREEN  │ PB5   │ LED + 220 Ω to GND ("walk" signal)        │
 *   │ PED_BTN     │ PD2   │ Pedestrian call button to GND             │
 *   │             │       │ (active-low), internal pull-up enabled.   │
 *   └─────────────┴───────┴──────────────────────────────────────────┘
 *
 * State sequence (always exactly one road green, or both red):
 *
 *   MAIN_GREEN --(pedestrian request)--> MAIN_YELLOW --> ALL_RED
 *       --> SIDE_GREEN --> SIDE_YELLOW --> ALL_RED --> MAIN_GREEN --> ...
 *
 * The button is polled, not interrupt-driven: wait_for_request() simply
 * blocks in T_POLL-sized steps until PED_BTN reads pressed, then the
 * state machine advances through the fixed sequence unconditionally —
 * once a crossing is granted, nothing can cut it short or skip a phase.
 */

#include <avr/io.h>
#include <util/delay.h>
#include <mikroduino/gpio.hpp>

using namespace MikroDuino;

// ── pins ─────────────────────────────────────────────────────────────────
static constexpr uint8_t MAIN_RED    = PB0;
static constexpr uint8_t MAIN_YELLOW = PB1;
static constexpr uint8_t MAIN_GREEN  = PB2;
static constexpr uint8_t SIDE_RED    = PB3;
static constexpr uint8_t SIDE_YELLOW = PB4;
static constexpr uint8_t SIDE_GREEN  = PB5;
static constexpr uint8_t PED_BTN     = PD2;

// ── timing (ms) ──────────────────────────────────────────────────────────
static constexpr uint16_t T_YELLOW    = 2000;
static constexpr uint16_t T_ALL_RED   = 1000;
static constexpr uint16_t T_SIDE_WALK = 5000;
static constexpr uint16_t T_POLL      = 200;   // button-check granularity while idle
static constexpr uint16_t T_DEBOUNCE  = 30;

static void delay_ms(uint16_t ms) { while (ms--) _delay_ms(1); }

// Drives all six lights from one call so every state transition in main()
// is a single, easy-to-audit line instead of six separate set()/clear()s.
static void set_lights(bool mainR, bool mainY, bool mainG,
                        bool sideR, bool sideY, bool sideG) {
    GPIO::write(MAIN_RED, mainR);
    GPIO::write(MAIN_YELLOW, mainY);
    GPIO::write(MAIN_GREEN, mainG);
    GPIO::write(SIDE_RED, sideR);
    GPIO::write(SIDE_YELLOW, sideY);
    GPIO::write(SIDE_GREEN, sideG);
}

// Blocks until the pedestrian button is pressed, sampling it once every
// T_POLL ms. Used during MAIN_GREEN so the road can stay green
// indefinitely without a request, yet respond within one T_POLL window
// of a press.
static void wait_for_request() {
    while (GPIO::read(PED_BTN)) {   // HIGH = not pressed (active-low)
        delay_ms(T_POLL);
    }
}

int main() {
    GPIO::output(MAIN_RED);   GPIO::output(MAIN_YELLOW);   GPIO::output(MAIN_GREEN);
    GPIO::output(SIDE_RED);   GPIO::output(SIDE_YELLOW);   GPIO::output(SIDE_GREEN);
    GPIO::inputPullup(PED_BTN);

    while (true) {
        // ---- MAIN_GREEN: side road held red until a pedestrian asks ----
        set_lights(/*mainR*/false, /*mainY*/false, /*mainG*/true,
                   /*sideR*/true,  /*sideY*/false,  /*sideG*/false);

        wait_for_request();
        delay_ms(T_DEBOUNCE);   // debounce the accepted press

        // ---- MAIN_YELLOW: warn main-road traffic the light is changing ----
        set_lights(false, true, false, true, false, false);
        delay_ms(T_YELLOW);

        // ---- ALL_RED: mandatory clearance interval, both directions stop ----
        set_lights(true, false, false, true, false, false);
        delay_ms(T_ALL_RED);

        // ---- SIDE_GREEN: pedestrian / side-road crossing window ----
        set_lights(true, false, false, false, false, true);
        delay_ms(T_SIDE_WALK);

        // ---- SIDE_YELLOW: warn side road before switching back ----
        set_lights(true, false, false, false, true, false);
        delay_ms(T_YELLOW);

        // ---- ALL_RED: clearance interval before returning to MAIN_GREEN ----
        set_lights(true, false, false, true, false, false);
        delay_ms(T_ALL_RED);
    }
}
