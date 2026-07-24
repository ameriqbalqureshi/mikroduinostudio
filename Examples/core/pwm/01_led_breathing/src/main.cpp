/*
 * PWM Basics — Breathing LED — MikroDuino SDK
 *
 * The simplest possible PWM program: fade an LED smoothly up and down,
 * the classic "breathing" effect. This is project 1 of 6 in the
 * examples/pwm series, which walks the PWM1Driver API from a single
 * fading LED up to a capstone that drives two hobby servos from
 * potentiometer input.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ Signal  │ Pin   │ Wiring                                    │
 *   ├─────────┼───────┼──────────────────────────────────────────┤
 *   │ OC1A    │ PB1   │ LED + 220 Ω resistor -> GND                │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * What PWM is, in one paragraph: a PWM pin doesn't actually output a
 * variable voltage — it switches fully on and fully off very fast (here,
 * 500 times a second) and varies the FRACTION of each cycle spent on
 * (the "duty cycle"). An LED switching that fast looks dim or bright to
 * the eye rather than flickering, because the eye and the LED's own
 * thermal/optical response both average the on/off pulses out. 500 Hz is
 * comfortably above the ~50-60 Hz flicker-fusion threshold most people
 * perceive, which is why that frequency (not something much slower) is
 * used here.
 *
 * The MikroDuino SDK's hardware PWM pins are fixed by which timer drives
 * them: PWM1Driver uses Timer1, whose two compare-output pins are PB1
 * (OC1A) and PB2 (OC1B) on the ATmega328P — not routable elsewhere, the
 * same way SPI's MOSI/SCK/MISO/SS are fixed to specific pins.
 *
 * PWM concepts introduced:
 *   - PWM1.begin(frequencyHz, type) — configures Timer1 for hardware PWM
 *     at the requested frequency. Internally this picks the smallest
 *     clock prescaler that lets the required TOP value (the tick count
 *     for one full PWM period) fit in Timer1's 16-bit counter, then
 *     programs ICR1 with that TOP and configures the Waveform Generation
 *     Mode bits for the requested PWMType. FastPWM (used here) is the
 *     simpler of the two: the counter counts 0 -> TOP -> 0 -> TOP...
 *   - PWM1.dutyA(percent) — sets channel A's (OC1A / PB1) duty cycle as
 *     a percentage, 0 (always off) to 100 (always on). Internally this
 *     computes the OCR1A compare value as top()*percent/100 and also
 *     enables the hardware compare output on PB1 the first time it's
 *     called (see enableChannelA() in pwm.hpp) — no separate "start"
 *     call is needed once begin() has configured the timer.
 */

#include <avr/io.h>
#include <util/delay.h>
#include <mikroduino/pwm.hpp>

using namespace MikroDuino;

int main() {
    // 500 Hz Fast PWM — fast enough that the LED never visibly flickers,
    // slow enough that Timer1's prescaler search (see pwm.hpp) lands on
    // a TOP value with plenty of headroom for smooth percent steps.
    PWM1.begin(500, PWMType::FastPWM);

    while (true) {
        // Fade up: 0% (off) to 100% (fully on), one percentage point at
        // a time. dutyA() takes a plain 0-100 percentage — no need to
        // know or compute the underlying timer's TOP value for this.
        for (uint8_t percent = 0; percent <= 100; ++percent) {
            PWM1.dutyA(percent);
            _delay_ms(8);
        }

        // Fade down: 100% back to 0%.
        for (uint8_t percent = 100; percent != 0; --percent) {
            PWM1.dutyA(percent);
            _delay_ms(8);
        }

        // A short pause at fully off between breaths, so the effect
        // reads as distinct "breaths" rather than a continuous triangle
        // wave.
        PWM1.dutyA(0);
        _delay_ms(300);
    }
}
