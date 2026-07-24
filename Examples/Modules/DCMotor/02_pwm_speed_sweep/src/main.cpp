/*
 * DCMotor PWM Speed Control — Variable-Speed Sweep — MikroDuino Module SDK
 *
 * Project 2 of 6 in the examples/Modules/DCMotor series. Wires ENA to a
 * hardware PWM pin so speed() finally controls how FAST the motor spins,
 * not just which way — sweeping smoothly from stopped to full speed
 * forward, back down, into full speed reverse, and back, one step at a
 * time.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ IN1     │ PD4   │ H-bridge direction input 1                │
 *   │ IN2     │ PD5   │ H-bridge direction input 2                │
 *   │ ENA     │ PD6   │ OC0A hardware PWM — speed control          │
 *   │ TXD     │ PD1   │ USB-serial adapter, 9600 8N1               │
 *   │ Motor   │ OUT1/2│ DC motor terminals                        │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * Why PD6 specifically: DCMotor.hpp documents four ATmega328P pins wired
 * to a usable PWM timer channel — OC0A/PD6, OC0B/PD5, OC2A/PB3, OC2B/PD3
 * (Timer1 is reserved for the Servo module elsewhere in this SDK). PD6
 * is picked here simply because it doesn't collide with the IN1/IN2 pins
 * this project also needs; any of the other three would work identically
 * with different wiring.
 *
 * DCMotor concepts introduced:
 *   - DCMotor(in1, in2, pwmPin) — the three-argument constructor. Passing
 *     a real pin (instead of the default NO_PWM) switches the object
 *     into PWM mode.
 *   - begin() now does more work than project 1: because pwmPin != NO_PWM,
 *     it internally calls _initPWM(), which configures PD6's owning timer
 *     (Timer0) for Fast PWM at ~977 Hz and finds the matching OCR0A
 *     register — see DCMotor.hpp's _initPWM() switch statement for the
 *     exact register bits, one case per supported pin.
 *   - speed(pct) now actually scales: DCMotor.hpp's _setPWM() writes
 *     abs(pct) * 255 / 100 into OCR0A, so speed(50) drives roughly half
 *     the duty cycle of speed(100) in the same direction — the digital
 *     ENA-tied-high behaviour from project 1 was really just this same
 *     code path with _ocr left null.
 *
 * DCMotor concepts reused from project 1:
 *   - DCMotor(in1, in2, ...), begin(), speed(pct), coast().
 */

#include <util/delay.h>
#include <mikroduino/usart.hpp>
#include <DCMotor.hpp>

using namespace MikroDuino;

DCMotor motor(PD4, PD5, PD6);   // PD6 = OC0A -> ENA, PWM speed control

// Ramps `from` to `to` in steps of `stepSize`, pausing `stepDelayMs`
// between each speed() call, and printing every step over USART.
static void rampTo(int8_t from, int8_t to, int8_t stepSize, uint16_t stepDelayMs) {
    int8_t step = (to >= from) ? stepSize : static_cast<int8_t>(-stepSize);
    int8_t pct  = from;

    while (true) {
        motor.speed(pct);
        USART0.write_P(PSTR("  speed = "));
        USART0.writeInt(pct);
        USART0.writeLine_P(PSTR(" %"));
        _delay_ms(stepDelayMs);

        if (pct == to) break;

        // Clamp the final step so it lands exactly on `to` instead of
        // overshooting past it when (to - from) isn't a multiple of step.
        int16_t next = static_cast<int16_t>(pct) + step;
        if ((step > 0 && next > to) || (step < 0 && next < to)) next = to;
        pct = static_cast<int8_t>(next);
    }
}

int main() {
    USART0.begin(9600);
    USART0.writeLine_P(PSTR("DCMotor PWM speed sweep"));
    USART0.writeLine_P(PSTR("========================"));

    motor.begin();

    while (true) {
        USART0.writeLine_P(PSTR("Ramping 0 -> +100 (accelerate forward)"));
        rampTo(0, 100, 5, 60);
        _delay_ms(500);

        USART0.writeLine_P(PSTR("Ramping +100 -> 0 (decelerate)"));
        rampTo(100, 0, 5, 60);
        _delay_ms(500);

        USART0.writeLine_P(PSTR("Ramping 0 -> -100 (accelerate reverse)"));
        rampTo(0, -100, 5, 60);
        _delay_ms(500);

        USART0.writeLine_P(PSTR("Ramping -100 -> 0 (decelerate)"));
        rampTo(-100, 0, 5, 60);
        _delay_ms(1000);
    }
}
