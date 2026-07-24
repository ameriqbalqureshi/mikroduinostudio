/*
 * PWM Servo Angle Control (Potentiometer Input) — MikroDuino SDK
 *
 * Project 5 of 6 in the examples/pwm series. Drives a standard hobby
 * servo — the canonical real-world use of precise raw-tick PWM — with
 * its angle set live by a potentiometer read through the ADC library.
 * The potentiometer/ADC part isn't new PWM API, but reading a live input
 * to drive the PWM output is what makes this feel like a real project
 * rather than a fixed demo sequence, and it directly motivates WHY raw
 * ticks (project 3) matter: a servo needs a specific pulse width in
 * MICROSECONDS, not a percentage.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ Signal  │ Pin   │ Wiring                                    │
 *   ├─────────┼───────┼──────────────────────────────────────────┤
 *   │ OC1A    │ PB1   │ Servo signal wire (orange/yellow)          │
 *   │ —       │ —     │ Servo +5V (red) -> external 5V supply —   │
 *   │         │       │ do NOT power a servo from the ATmega's own │
 *   │         │       │ 5V regulator; share GROUND with the board  │
 *   │         │       │ but give the servo its own supply.         │
 *   │ —       │ —     │ Servo GND (brown/black) -> common GND      │
 *   │ ADC0    │ PC0   │ Potentiometer wiper — outer legs to        │
 *   │         │       │ 5V and GND                                 │
 *   │ TXD     │ PD1   │ USB-serial adapter                        │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * How a hobby servo actually reads PWM: unlike the LED/buzzer projects,
 * a servo does NOT care about duty cycle as a percentage — it decodes
 * the ABSOLUTE PULSE WIDTH of the high period, repeated roughly every
 * 20 ms (50 Hz). By convention:
 *   1.0 ms pulse  -> 0°   (one end of travel)
 *   1.5 ms pulse  -> 90°  (center)
 *   2.0 ms pulse  -> 180° (other end of travel)
 * Everything in between maps roughly linearly to angle. This is why
 * project 3's rawA()/top() are the right tool here: dutyA(percent) can't
 * express "exactly 1500 microseconds" in general, because percent is
 * relative to top() and top() depends on frequency/prescaler in a way
 * that doesn't line up neatly with microseconds. Raw ticks, converted
 * from microseconds using top() and the known 20 ms period, hit the
 * exact pulse width a servo needs regardless of which prescaler
 * PWM1.begin() happened to choose internally.
 *
 * PWM concepts reused from projects 1-3:
 *   - PWM1.begin(frequencyHz, PWMType::FastPWM) — 50 Hz this time, the
 *     servo-standard PWM rate, instead of the LED/buzzer projects' much
 *     higher frequencies.
 *   - PWM1.top(), PWM1.rawA(value) — converting a physical pulse width
 *     in microseconds to the correct raw tick count for whatever TOP
 *     begin() happened to compute at 50 Hz.
 *
 * New (non-PWM) library used for interactivity:
 *   - ADCDriver (mikroduino/adc.hpp): ADC_Driver.begin() and
 *     ADC_Driver.read(channel) — a blocking single conversion returning
 *     a 0-1023 value proportional to the potentiometer's wiper voltage.
 */

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <mikroduino/pwm.hpp>
#include <mikroduino/adc.hpp>
#include <mikroduino/usart.hpp>

using namespace MikroDuino;

static constexpr uint8_t POT_CHANNEL = 0;   // ADC0 / PC0

static constexpr uint16_t SERVO_MIN_US = 1000;   // 0 degrees
static constexpr uint16_t SERVO_MAX_US = 2000;   // 180 degrees
static constexpr uint16_t SERVO_PERIOD_US = 20000; // 50 Hz -> 20 ms period

static void delay_ms(uint16_t ms) { while (ms--) _delay_ms(1); }

// Convert a desired pulse width in microseconds to the raw OCR1A tick
// count for whatever TOP the current PWM1.begin() call computed. Works
// regardless of which prescaler begin() chose internally: top() ticks
// span exactly SERVO_PERIOD_US microseconds, so this is a simple
// proportion.
static uint16_t usToTicks(uint16_t microseconds) {
    uint32_t top = PWM1.top();
    return static_cast<uint16_t>((top * microseconds) / SERVO_PERIOD_US);
}

int main() {
    ADC_Driver.begin();                       // defaults: AVCC reference, DIV128 prescaler
    PWM1.begin(50, PWMType::FastPWM);          // 50 Hz servo-standard PWM rate

    USART0.begin(9600);
    USART0.writeLine_P(PSTR("PWM servo angle control (potentiometer on ADC0)"));
    USART0.writeLine_P(PSTR("================================================="));
    USART0.write_P(PSTR("PWM1.top() at 50 Hz = "));
    USART0.writeInt(PWM1.top());
    USART0.writeLine_P(PSTR(" raw ticks per 20 ms period"));

    while (true) {
        // ADC_Driver.read() blocks until the 10-bit conversion finishes
        // (about 13 ADC clock cycles, well under a millisecond at the
        // default DIV128 prescaler) and returns 0-1023.
        uint16_t potValue = ADC_Driver.read(POT_CHANNEL);

        // Map the 0-1023 ADC range linearly onto the servo's 1000-2000 us
        // pulse-width range, and onto a human-readable 0-180 degree scale
        // for the USART readout.
        uint16_t pulseUs = static_cast<uint16_t>(
            SERVO_MIN_US + (static_cast<uint32_t>(potValue) * (SERVO_MAX_US - SERVO_MIN_US)) / 1023u
        );
        uint8_t angleDeg = static_cast<uint8_t>(
            (static_cast<uint32_t>(potValue) * 180u) / 1023u
        );

        PWM1.rawA(usToTicks(pulseUs));

        USART0.write_P(PSTR("pot="));
        USART0.writeInt(potValue);
        USART0.write_P(PSTR("  pulse="));
        USART0.writeInt(pulseUs);
        USART0.write_P(PSTR(" us  angle~="));
        USART0.writeInt(angleDeg);
        USART0.writeLine_P(PSTR(" deg"));

        // A servo only needs to be told its target every 20-50 ms or so;
        // polling much faster wastes cycles without improving response.
        delay_ms(100);
    }
}
