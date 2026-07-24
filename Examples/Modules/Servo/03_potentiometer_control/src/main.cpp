/*
 * Servo Live Potentiometer Control — ADCDriver + Servo::write() —
 * MikroDuino Module SDK
 *
 * Project 3 of 6 in the examples/Modules/Servo series. Drives the
 * servo's angle live from a potentiometer instead of a fixed sequence —
 * the same scenario as examples/pwm/05_servo_angle_control, but through
 * the Servo class instead of raw PWM1Driver calls, to make the payoff
 * of projects 1-2's wrapper concrete: no top(), no usToTicks() helper,
 * no manual 20 ms/50 Hz period math — just write(angleDeg).
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ OC1A    │ PB1   │ Servo signal wire — Servo channel 0        │
 *   │ —       │ —     │ External 5V supply + common GND (see       │
 *   │         │       │ project 1's wiring note)                    │
 *   │ ADC0    │ PC0   │ Potentiometer wiper — outer legs to        │
 *   │         │       │ 5V and GND                                 │
 *   │ TXD     │ PD1   │ USB-serial adapter, 9600 8N1                │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * Servo concepts reused from projects 1-2:
 *   - Servo(channel), begin(), write(angleDeg) — the default
 *     1000-2000 us range suits a standard hobby servo again here.
 *
 * New (non-Servo) library used for interactivity:
 *   - ADCDriver (mikroduino/adc.hpp): ADC_Driver.begin() and
 *     ADC_Driver.read(channel) — a blocking single conversion returning
 *     a 0-1023 value proportional to the potentiometer's wiper voltage,
 *     mapped linearly onto 0-180 degrees for write().
 */

#include <avr/pgmspace.h>
#include <util/delay.h>
#include <mikroduino/adc.hpp>
#include <mikroduino/usart.hpp>
#include <Servo.hpp>

using namespace MikroDuino;

static constexpr uint8_t POT_CHANNEL = 0;   // ADC0 / PC0

Servo arm(0);

static void delay_ms(uint16_t ms) { while (ms--) _delay_ms(1); }

int main() {
    ADC_Driver.begin();   // defaults: AVCC reference, DIV128 prescaler
    arm.begin();

    USART0.begin(9600);
    USART0.writeLine_P(PSTR("Servo live potentiometer control"));
    USART0.writeLine_P(PSTR("===================================="));

    while (true) {
        // ADC_Driver.read() blocks until the 10-bit conversion finishes
        // (about 13 ADC clock cycles, well under a millisecond at the
        // default DIV128 prescaler) and returns 0-1023.
        uint16_t potValue = ADC_Driver.read(POT_CHANNEL);
        uint8_t  angleDeg = static_cast<uint8_t>(
            (static_cast<uint32_t>(potValue) * 180u) / 1023u
        );

        arm.write(angleDeg);

        USART0.write_P(PSTR("pot="));
        USART0.writeInt(potValue);
        USART0.write_P(PSTR("  angle="));
        USART0.writeInt(angleDeg);
        USART0.write_P(PSTR(" deg  pulse="));
        USART0.writeInt(arm.readMicroseconds());
        USART0.writeLine_P(PSTR(" us"));

        // A servo only needs to be told its target every 20-50 ms or so;
        // polling much faster wastes cycles without improving response.
        delay_ms(100);
    }
}
