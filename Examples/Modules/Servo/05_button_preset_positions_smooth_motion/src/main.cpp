/*
 * Servo Button Presets with Non-Blocking Easing and detach() — Button +
 * Servo — MikroDuino Module SDK
 *
 * Project 5 of 6 in the examples/Modules/Servo series. Projects 1-4 all
 * called write()/writeMicroseconds() and let the servo snap straight to
 * the new target. Snapping is fine for a slow potentiometer sweep, but
 * a button-triggered jump between far-apart presets (0 -> 180 degrees
 * on a click) commands the servo's motor to slew as fast as it
 * physically can, which draws a large current spike and puts a hard
 * jolt through anything mounted on the horn. This project instead
 * steps the commanded angle toward the target by one degree at a time
 * on its own schedule — the same non-blocking "ease toward a target"
 * shape as a PID setpoint ramp, just linear instead of proportional.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ OC1A    │ PB1   │ Servo signal wire — Servo channel 0        │
 *   │ —       │ —     │ External 5V supply + common GND (see       │
 *   │         │       │ project 1's wiring note)                    │
 *   │ SW      │ PD4   │ Push button to GND, internal pull-up        │
 *   │ TXD     │ PD1   │ USB-serial adapter, 9600 8N1                │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * Gestures (examples/Modules/Button's clicked()/longPressed()):
 *   clicked()     -> advance to the next of 3 presets (0, 90, 180 deg)
 *                    and ease toward it; OR, if the servo is currently
 *                    detached, re-attach it at its last held angle
 *                    without changing the target.
 *   longPressed() -> detach() the servo — it stops receiving pulses and
 *                    goes limp under its own gearing friction, saving
 *                    current and eliminating idle jitter, exactly the
 *                    scenario examples/pwm/06_pan_tilt_servo_dashboard
 *                    automated with an idle timer instead of a button.
 *
 * Servo concepts reused from projects 1-3:
 *   - Servo(channel), begin(), write(angleDeg).
 *
 * New Servo concept:
 *   - detach() — disconnects Timer1's compare output from the pin,
 *     leaving it as a plain (unpowered-signal) GPIO. There is no
 *     separate "attach" call: begin() unconditionally re-enables this
 *     channel's compare output as one of its first steps, so calling
 *     begin() again is how a detached channel resumes being driven —
 *     used below on the re-attach click.
 *
 * Non-Servo concepts reused:
 *   - Button (examples/Modules/Button) for clicked()/longPressed().
 *   - The Timer0-overflow millis() clock (examples/timer/02), driving
 *     both Button::update()'s 1 ms cadence and the easing step's own
 *     independent schedule — Timer0 is free to use for this because
 *     Servo owns Timer1 exclusively, never Timer0.
 */

#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/usart.hpp>
#include <mikroduino/timer.hpp>
#include <mikroduino/registers.hpp>
#include <Button.hpp>
#include <Servo.hpp>

using namespace MikroDuino;

Servo  arm(0);
Button btn(PD4);

// ── millis() clock (same technique as examples/timer/02_software_millis_clock) ──

static constexpr uint16_t MICROS_PER_OVERFLOW = 1024;
static constexpr uint16_t MILLIS_INC = MICROS_PER_OVERFLOW / 1000;
static constexpr uint16_t FRACT_INC  = MICROS_PER_OVERFLOW % 1000;
static constexpr uint16_t FRACT_MAX  = 1000;

static volatile uint32_t g_millis = 0;
static volatile uint16_t g_fract  = 0;

ISR(TIMER0_OVF_vect) {
    uint32_t m = g_millis;
    uint16_t f = g_fract;
    m += MILLIS_INC;
    f += FRACT_INC;
    if (f >= FRACT_MAX) { f -= FRACT_MAX; ++m; }
    g_fract  = f;
    g_millis = m;
}

static uint32_t millis() {
    uint32_t snapshot;
    ATOMIC_BLOCK_START;
    snapshot = g_millis;
    ATOMIC_BLOCK_END;
    return snapshot;
}

static constexpr uint8_t  PRESET_COUNT   = 3;
static const uint8_t      PRESETS[PRESET_COUNT] = { 0, 90, 180 };
static constexpr uint16_t EASE_STEP_MS   = 15;   // ~1 deg every 15 ms -> full sweep in ~2.7 s

int main() {
    arm.begin();
    btn.begin();

    Timer0.mode(TimerMode::Normal);
    Timer0.prescaler(TimerPrescaler::DIV64);
    Timer0.start();
    Timer0.enableOverflow();
    sei();

    USART0.begin(9600);
    USART0.writeLine_P(PSTR("Servo button presets, eased motion"));
    USART0.writeLine_P(PSTR("======================================"));
    USART0.writeLine_P(PSTR("Click: next preset.  Hold: detach (go limp)."));

    uint8_t presetIndex   = 0;
    uint8_t currentAngle  = PRESETS[0];
    uint8_t targetAngle   = PRESETS[0];
    bool    detached      = false;

    uint32_t lastUpdateMs = millis();
    uint32_t nextEaseAt   = millis();

    while (true) {
        uint32_t now = millis();

        while (now - lastUpdateMs >= 1) {
            btn.update();
            ++lastUpdateMs;
        }

        if (btn.clicked()) {
            if (detached) {
                arm.begin();          // no separate "attach" API — see header comment
                arm.write(currentAngle);
                detached = false;
                USART0.writeLine_P(PSTR("Re-attached."));
            } else {
                presetIndex = static_cast<uint8_t>((presetIndex + 1) % PRESET_COUNT);
                targetAngle = PRESETS[presetIndex];
                USART0.write_P(PSTR("Target -> "));
                USART0.writeInt(targetAngle);
                USART0.writeLine_P(PSTR(" deg"));
            }
        }

        if (btn.longPressed() && !detached) {
            arm.detach();
            detached = true;
            USART0.writeLine_P(PSTR("Detached (limp, current target retained)."));
        }

        // ---- Non-blocking easing toward the target, one degree at a time ----
        if (!detached && currentAngle != targetAngle && now >= nextEaseAt) {
            nextEaseAt = now + EASE_STEP_MS;
            if (currentAngle < targetAngle) ++currentAngle;
            else                            --currentAngle;
            arm.write(currentAngle);
        }
    }
}
