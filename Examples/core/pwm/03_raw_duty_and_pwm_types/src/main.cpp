/*
 * PWM Raw Duty & Waveform Types — MikroDuino SDK
 *
 * Project 3 of 6 in the examples/pwm series. Introduces the two pieces
 * of PWM1Driver that projects 1-2 deliberately avoided: raw tick-level
 * duty control via top()/rawA(), and the difference between the two
 * PWMType waveforms.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ Signal  │ Pin   │ Wiring                                    │
 *   ├─────────┼───────┼──────────────────────────────────────────┤
 *   │ OC1A    │ PB1   │ LED + 220 Ω resistor -> GND                │
 *   │ TXD     │ PD1   │ USB-serial adapter                        │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * Why raw ticks instead of percent: dutyA(percent) only has 101 possible
 * settings (0-100). At a given frequency, PWM1.top() is usually much
 * larger than 100 — every raw compare value from 0 to top() is a
 * distinct, valid duty cycle. For a smooth analog-feeling fade (this
 * project) or for hitting an exact physical value like a servo pulse
 * width in microseconds (project 5), raw ticks give far finer control
 * than the convenience percent API can.
 *
 * FastPWM vs PhaseCorrect, briefly: both count using the same TOP value
 * concept, but FastPWM counts 0 -> TOP -> 0 -> TOP (a sawtooth), while
 * PhaseCorrect counts 0 -> TOP -> 0 -> TOP -> 0 as a triangle (up AND
 * down every period). That means a PhaseCorrect period takes TWICE as
 * many timer ticks as a FastPWM period at the SAME PWM frequency and
 * prescaler — begin() accounts for this automatically (notice top()
 * differs between the two below even though the requested frequency is
 * identical), so nothing needs to be adjusted by hand. The practical
 * difference: FastPWM's compare match always happens on the same
 * (rising) edge of the counter, giving a fixed output-transition timing
 * relative to the period; PhaseCorrect's compare matches happen
 * symmetrically around the period's center on both the up-count and
 * down-count, which is why it's generally preferred for motor control
 * and audio — less high-frequency switching noise, at the cost of half
 * the effective PWM frequency for the same TOP resolution.
 *
 * PWM concepts introduced:
 *   - PWM1.top() — returns the TOP value begin() computed and programmed
 *     into ICR1: the raw tick count corresponding to 100% duty at the
 *     configured frequency/type. This is the ceiling for rawA()/rawB().
 *   - PWM1.rawA(value) / PWM1.rawB(value) — sets the OCR1A/OCR1B compare
 *     register directly, in raw timer ticks (0 to top()), bypassing the
 *     percent-based rounding dutyA()/dutyB() do internally. Same
 *     auto-enable-on-first-call behavior as dutyA()/dutyB().
 *   - PWMType::FastPWM vs PWMType::PhaseCorrect, passed to begin().
 *
 * PWM concepts reused from projects 1-2:
 *   - PWM1.begin(frequencyHz, type), PWM1.dutyA(percent).
 */

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <mikroduino/pwm.hpp>
#include <mikroduino/usart.hpp>

using namespace MikroDuino;

static void delay_ms(uint16_t ms) { while (ms--) _delay_ms(1); }

// Fade fully 0 -> top() -> 0 one raw tick at a time. At a high enough PWM
// frequency and a short enough per-step delay, this is a visibly smoother
// fade than project 1's 101-step percent version, because there are far
// more than 101 distinct brightness levels available.
static void fadeRaw(uint16_t stepDelayUs) {
    uint16_t top = PWM1.top();
    for (uint16_t v = 0; v <= top; ++v) {
        PWM1.rawA(v);
        _delay_us(stepDelayUs);
    }
    for (uint16_t v = top; v != 0; --v) {
        PWM1.rawA(v);
        _delay_us(stepDelayUs);
    }
    PWM1.rawA(0);
}

int main() {
    USART0.begin(9600);
    USART0.writeLine_P(PSTR("PWM raw duty & waveform types"));
    USART0.writeLine_P(PSTR("==============================="));

    while (true) {
        // -----------------------------------------------------------------
        // FastPWM @ 10 kHz — high frequency gives a large top() (fine
        // resolution) and is fast enough that even a 1-tick-per-step raw
        // fade completes quickly.
        // -----------------------------------------------------------------
        PWM1.begin(10000, PWMType::FastPWM);
        USART0.write_P(PSTR("FastPWM @ 10 kHz: top() = "));
        USART0.writeInt(PWM1.top());
        USART0.writeLine_P(PSTR(" raw ticks per full-scale fade"));
        fadeRaw(200);

        // -----------------------------------------------------------------
        // PhaseCorrect @ 10 kHz — same requested frequency, but top() is
        // roughly HALF of the FastPWM value above, because a
        // PhaseCorrect period counts up AND down across the same tick
        // budget (see header comment).
        // -----------------------------------------------------------------
        PWM1.begin(10000, PWMType::PhaseCorrect);
        USART0.write_P(PSTR("PhaseCorrect @ 10 kHz: top() = "));
        USART0.writeInt(PWM1.top());
        USART0.writeLine_P(PSTR(" raw ticks per full-scale fade"));
        fadeRaw(200);

        // -----------------------------------------------------------------
        // Exact fractional levels via raw ticks: quarter / half / three-
        // quarter brightness computed directly from top(), which is more
        // precise than dutyA(25)/dutyA(50)/dutyA(75) when top() isn't a
        // clean multiple of 4 (dutyA() truncates top()*percent/100 to an
        // integer, same as this code does explicitly here).
        // -----------------------------------------------------------------
        uint16_t top = PWM1.top();
        USART0.writeLine_P(PSTR("Exact raw fractions of top(): 1/4, 1/2, 3/4"));
        PWM1.rawA(static_cast<uint16_t>(top / 4));
        delay_ms(800);
        PWM1.rawA(static_cast<uint16_t>(top / 2));
        delay_ms(800);
        PWM1.rawA(static_cast<uint16_t>((top * 3) / 4));
        delay_ms(800);
        PWM1.rawA(0);
        delay_ms(400);

        USART0.writeLine_P(PSTR(""));
    }
}
