/*
 * DCMotor Soft-Start / Soft-Stop Ramping — Non-Blocking millis() —
 * MikroDuino Module SDK
 *
 * Project 4 of 6 in the examples/Modules/DCMotor series. DCMotor itself
 * has no ramping built in — speed() applies whatever percentage you pass
 * it immediately, which is exactly what projects 1-3 wanted. Snapping
 * straight from 0 to 100% is hard on gearboxes, drive belts, and the
 * H-bridge's inrush current, though, so this project builds a soft-
 * start/soft-stop layer entirely out of DCMotor's existing public API:
 * a button steps a TARGET speed through five presets, and a background
 * ramp eases the motor's ACTUAL speed toward that target a few percent
 * at a time — the same "supplement the module with ordinary application
 * logic" idea examples/Modules/Button/05 used for auto-repeat.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ IN1     │ PD4   │ H-bridge direction input 1                │
 *   │ IN2     │ PD5   │ H-bridge direction input 2                │
 *   │ ENA     │ PD6   │ OC0A hardware PWM — speed control          │
 *   │ Button  │ PD2   │ Other leg to GND — click = next speed      │
 *   │         │       │ preset, long-press = immediate brake       │
 *   │         │       │ (bypasses the ramp entirely)                │
 *   │ TXD     │ PD1   │ USB-serial adapter, 9600 8N1               │
 *   │ Motor   │ OUT1/2│ DC motor terminals                        │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * Why this project switches to millis() (identical Timer0-overflow
 * technique to examples/timer/02_software_millis_clock and
 * examples/Modules/Button/03): the ramp must advance on ITS OWN fixed
 * schedule (one step every RAMP_STEP_MS) independent of how often the
 * button happens to be polled, and button clicks must stay responsive
 * without waiting for a ramp step to finish first. A single blocking
 * `_delay_ms(N)` per loop pass can only serve one of those masters — the
 * exact problem Button project 3's header comment already walks through
 * in detail.
 *
 * Ramp design:
 *   - targetSpeed: set instantly by the button, one of five presets
 *     (-100, -50, 0, +50, +100), cycled in order and wrapping around.
 *   - currentSpeed: what's actually passed to motor.speed(), nudged
 *     toward targetSpeed by RAMP_STEP_PCT every RAMP_STEP_MS, in
 *     whichever direction closes the gap. Reaching targetSpeed exactly
 *     stops the ramp until the next click moves the target again.
 *   - A long-press brake() bypasses the ramp completely and forces both
 *     currentSpeed and targetSpeed to 0, so releasing the button (or
 *     clicking again) resumes from a clean stop rather than lurching
 *     back to whatever speed was active before the brake.
 *
 * DCMotor concepts reused from projects 1-3:
 *   - DCMotor(in1, in2, pwmPin), begin(), speed(pct), brake(). Note that
 *     this project is also the first to call speed() from inside a
 *     periodic, non-blocking scheduler rather than a linear sequence of
 *     calls — DCMotor doesn't care who calls it or how often, it just
 *     applies whatever value it's given.
 *
 * Button concepts reused:
 *   - Button(pin), begin(), update(), clicked(), longPressed().
 */

#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/usart.hpp>
#include <mikroduino/timer.hpp>
#include <mikroduino/registers.hpp>
#include <Button.hpp>
#include <DCMotor.hpp>

using namespace MikroDuino;

DCMotor motor(PD4, PD5, PD6);
Button  speedButton(PD2);

// ── millis() clock (identical technique to examples/timer/02_software_millis_clock) ──

static constexpr uint16_t MICROS_PER_OVERFLOW = 1024;   // DIV64, 256 ticks
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

// ── Speed presets ────────────────────────────────────────────────────────

static constexpr int8_t  PRESETS[]   = { -100, -50, 0, 50, 100 };
static constexpr uint8_t PRESET_COUNT = sizeof(PRESETS) / sizeof(PRESETS[0]);
static uint8_t presetIndex = 2;   // starts at 0 %

static constexpr uint16_t RAMP_STEP_MS  = 15;   // one ramp step every 15 ms
static constexpr int8_t   RAMP_STEP_PCT = 2;    // ...of up to 2 % per step

int main() {
    USART0.begin(9600);
    USART0.writeLine_P(PSTR("DCMotor soft-start/soft-stop ramp"));
    USART0.writeLine_P(PSTR("==================================="));
    USART0.writeLine_P(PSTR("Click: next speed preset.  Hold: immediate brake."));
    USART0.writeLine_P(PSTR(""));

    motor.begin();
    speedButton.begin();

    Timer0.mode(TimerMode::Normal);
    Timer0.prescaler(TimerPrescaler::DIV64);
    Timer0.start();
    Timer0.enableOverflow();
    sei();

    int8_t targetSpeed  = PRESETS[presetIndex];
    int8_t currentSpeed = 0;

    uint32_t lastUpdateMs = millis();
    uint32_t nextRampAt   = millis();
    bool     braking      = false;

    while (true) {
        uint32_t now = millis();

        while (now - lastUpdateMs >= 1) {
            speedButton.update();
            ++lastUpdateMs;
        }

        if (speedButton.clicked() && !braking) {
            presetIndex = static_cast<uint8_t>((presetIndex + 1) % PRESET_COUNT);
            targetSpeed = PRESETS[presetIndex];
            USART0.write_P(PSTR("New target: "));
            USART0.writeInt(targetSpeed);
            USART0.writeLine_P(PSTR(" %"));
        }

        if (speedButton.longPressed()) {
            braking = true;
            motor.brake();
            currentSpeed = 0;
            targetSpeed  = 0;
            presetIndex  = 2;   // realign the preset cycle on 0 %
            USART0.writeLine_P(PSTR(">>> BRAKE (ramp bypassed) <<<"));
        }

        if (braking) {
            // Stay braked until the button is released, then resume
            // normal ramped control from a clean stop.
            if (speedButton.released()) {
                braking = false;
                USART0.writeLine_P(PSTR("Brake released — ramp resumed from 0 %"));
            }
        } else if (now >= nextRampAt && currentSpeed != targetSpeed) {
            nextRampAt = now + RAMP_STEP_MS;

            int16_t next = currentSpeed;
            if (currentSpeed < targetSpeed) {
                next += RAMP_STEP_PCT;
                if (next > targetSpeed) next = targetSpeed;
            } else {
                next -= RAMP_STEP_PCT;
                if (next < targetSpeed) next = targetSpeed;
            }
            currentSpeed = static_cast<int8_t>(next);
            motor.speed(currentSpeed);
        }
    }
}
