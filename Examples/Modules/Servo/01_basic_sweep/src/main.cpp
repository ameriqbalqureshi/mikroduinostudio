/*
 * Servo Basics — Servo(channel), begin(), write() — MikroDuino Module SDK
 *
 * The simplest possible use of the Servo module: sweep a single hobby
 * servo back and forth between 0 and 180 degrees. This is project 1 of
 * 6 in the examples/Modules/Servo series, which walks the Servo class
 * from a single sweep up to a capstone ultrasonic radar that pans a
 * servo while sampling distance at each angle.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ Signal  │ Pin   │ Wiring                                    │
 *   ├─────────┼───────┼──────────────────────────────────────────┤
 *   │ OC1A    │ PB1   │ Servo signal wire (orange/yellow) — Servo  │
 *   │         │       │ channel 0                                  │
 *   │ —       │ —     │ Servo +5V (red) -> external 5V supply —   │
 *   │         │       │ do NOT power a servo from the ATmega's own │
 *   │         │       │ 5V regulator; share GROUND with the board  │
 *   │         │       │ but give the servo its own supply.         │
 *   │ —       │ —     │ Servo GND (brown/black) -> common GND      │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * Relationship to examples/pwm/05_servo_angle_control: that project
 * drives a hobby servo with raw PWM1Driver calls (rawA(), top(), and
 * hand-rolled microsecond-to-tick math) specifically to show what a
 * servo needs at the register level. The Servo class in this module IS
 * that same Timer1 Fast-PWM-with-ICR1-as-TOP setup and that same
 * microsecond math, packaged so write(angleDeg) is the only thing an
 * application has to call.
 *
 * CAUTION: Servo owns Timer1 exclusively once begin() runs. Do not use
 * MikroDuino::PWM1 or raw Timer1 registers in the same project.
 *
 * Servo concepts introduced:
 *   - Servo(channel, minUs=1000, maxUs=2000) — channel 0 is OC1A (PB1),
 *     channel 1 is OC1B (PB2, see project 4). minUs/maxUs default to
 *     the standard 1-2 ms hobby-servo range; project 2 shows overriding
 *     them for a servo with a different datasheet range.
 *   - begin() — configures Timer1 for 50 Hz Fast PWM and enables this
 *     channel's compare output. Safe to call once per servo object.
 *   - write(angleDeg) — maps 0-180 degrees linearly onto [minUs, maxUs]
 *     and applies it immediately. Values above 180 are clamped.
 */

#include <util/delay.h>
#include <Servo.hpp>

using namespace MikroDuino;

Servo arm(0);   // channel 0 -> OC1A -> PB1, standard 1000-2000 us range

int main() {
    arm.begin();

    while (true) {
        for (uint8_t angle = 0; angle <= 180; angle += 2) {
            arm.write(angle);
            _delay_ms(20);   // one PWM period per step — smooth, unhurried sweep
        }
        for (int16_t angle = 180; angle >= 0; angle -= 2) {
            arm.write(static_cast<uint8_t>(angle));
            _delay_ms(20);
        }
    }
}
