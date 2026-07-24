/*
 * Stepper + Button — Tap-to-Nudge, Hold-to-Jog Manual Positioner —
 * MikroDuino Module SDK
 *
 * Project 3 of 6 in the examples/Modules/Stepper series. Two buttons
 * become a manual jog controller: a quick tap nudges the motor a small
 * fixed amount (clicked()), while holding a button down keeps stepping
 * continuously for as long as it stays pressed (longPressed()/released())
 * — the same "tap vs. hold does something different" vocabulary
 * examples/Modules/Button/01 and /03 introduced, applied here to
 * physical motion instead of an LED or a latch.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ STEP    │ PD2   │ Driver STEP input                         │
 *   │ DIR     │ PD3   │ Driver DIR input                          │
 *   │ CW btn  │ PD5   │ Other leg to GND — tap = nudge CW,         │
 *   │         │       │ hold = jog CW continuously                │
 *   │ CCW btn │ PD6   │ Other leg to GND — same, counter-clockwise │
 *   │ TXD     │ PD1   │ USB-serial adapter, 9600 8N1               │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * Jog design:
 *   - NUDGE_STEPS (25 steps = 1/8 revolution at 200 steps/rev) moves once
 *     per clicked() event — a quick tap that releases before the button's
 *     longPressMs threshold (default 600 ms), so longPressed() never
 *     fires and clicked() does instead.
 *   - longPressed() flips a "jogging" latch on for that button; step(1)
 *     (or step(-1)) then runs once per main-loop pass for as long as the
 *     latch stays set, and released() clears it — motion starts the
 *     moment the hold threshold is crossed and stops the instant the
 *     button comes back up.
 *
 * Button concepts reused from examples/Modules/Button/01 and /03:
 *   - Button(pin), begin(), update(), clicked(), longPressed(),
 *     released().
 */

#include <util/delay.h>
#include <mikroduino/usart.hpp>
#include <Button.hpp>
#include <Stepper.hpp>

using namespace MikroDuino;

Stepper motor(PD2, PD3);
Button  cwButton(PD5);
Button  ccwButton(PD6);

static constexpr int32_t NUDGE_STEPS = 25;   // 1/8 revolution at 200 steps/rev

int main() {
    USART0.begin(9600);
    USART0.writeLine_P(PSTR("Stepper manual jog controller"));
    USART0.writeLine_P(PSTR("==============================="));
    USART0.writeLine_P(PSTR("Tap: nudge 1/8 turn.  Hold: jog continuously."));
    USART0.writeLine_P(PSTR(""));

    motor.begin();
    motor.setRPM(90, 200);
    cwButton.begin();
    ccwButton.begin();

    bool cwJogging  = false;
    bool ccwJogging = false;

    while (true) {
        cwButton.update();
        ccwButton.update();

        if (cwButton.longPressed())  cwJogging  = true;
        if (cwButton.released())     cwJogging  = false;
        if (ccwButton.longPressed()) ccwJogging = true;
        if (ccwButton.released())    ccwJogging = false;

        if (cwJogging) {
            motor.step(1);
        } else if (ccwJogging) {
            motor.step(-1);
        } else if (cwButton.clicked()) {
            USART0.writeLine_P(PSTR("Nudge CW"));
            motor.step(NUDGE_STEPS);
        } else if (ccwButton.clicked()) {
            USART0.writeLine_P(PSTR("Nudge CCW"));
            motor.step(-NUDGE_STEPS);
        }

        _delay_ms(1);   // Button::update() expects to be called ~every 1 ms
    }
}
