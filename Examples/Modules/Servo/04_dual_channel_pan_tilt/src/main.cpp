/*
 * Servo Dual Channel Pan-Tilt — two Servo objects, one shared Timer1 —
 * MikroDuino Module SDK
 *
 * Project 4 of 6 in the examples/Modules/Servo series. The Servo class
 * exposes exactly two channels because it exposes exactly Timer1's two
 * compare outputs (OC1A, OC1B) — this project uses both at once for a
 * classic pan/tilt camera-mount scheme, one potentiometer per axis.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌─────────────┬───────┬──────────────────────────────────────────┐
 *   │ OC1A        │ PB1   │ Pan servo signal wire — Servo channel 0    │
 *   │ OC1B        │ PB2   │ Tilt servo signal wire — Servo channel 1   │
 *   │ —           │ —     │ Both servos: +5V from an external supply, │
 *   │             │       │ GND common with the ATmega — see project  │
 *   │             │       │ 1's wiring note; two servos draw more     │
 *   │             │       │ current than the board's own regulator    │
 *   │             │       │ can safely supply.                        │
 *   │ ADC0        │ PC0   │ Pan potentiometer wiper                    │
 *   │ ADC1        │ PC1   │ Tilt potentiometer wiper                   │
 *   │ TXD         │ PD1   │ USB-serial adapter                        │
 *   └─────────────┴───────┴──────────────────────────────────────────┘
 *
 * Sharing Timer1 between two Servo objects: begin() unconditionally
 * (re)writes TCCR1A/TCCR1B/ICR1 to the same 50 Hz Fast-PWM configuration
 * every time it runs, then enables just its OWN channel's compare
 * output on top of that. Calling pan.begin() and then tilt.begin() is
 * therefore safe and is the documented usage (see Servo.hpp's own
 * header comment) — the second call re-applies the identical timer
 * config and additionally turns on OC1B, leaving OC1A's setting from
 * the first call untouched. What is NOT safe is mixing a Servo object
 * with MikroDuino::PWM1 or raw Timer1 register writes in the same
 * project — both would fight over the same timer.
 *
 * Servo concepts reused from projects 1-3:
 *   - Servo(channel), begin(), write(angleDeg) — one object per
 *     channel, called independently.
 *
 * Non-Servo concepts reused from project 3:
 *   - ADCDriver — two channels (ADC0 = pan, ADC1 = tilt) read every
 *     loop iteration to drive the two servos independently.
 */

#include <avr/pgmspace.h>
#include <util/delay.h>
#include <mikroduino/adc.hpp>
#include <mikroduino/usart.hpp>
#include <Servo.hpp>

using namespace MikroDuino;

static constexpr uint8_t PAN_CHANNEL  = 0;   // ADC0 / PC0
static constexpr uint8_t TILT_CHANNEL = 1;   // ADC1 / PC1

Servo pan(0);    // OC1A / PB1
Servo tilt(1);   // OC1B / PB2

static void delay_ms(uint16_t ms) { while (ms--) _delay_ms(1); }

static uint8_t potToAngle(uint16_t potValue) {
    return static_cast<uint8_t>((static_cast<uint32_t>(potValue) * 180u) / 1023u);
}

int main() {
    ADC_Driver.begin();
    pan.begin();
    tilt.begin();

    USART0.begin(9600);
    USART0.writeLine_P(PSTR("Servo dual-channel pan-tilt"));
    USART0.writeLine_P(PSTR("=============================="));

    while (true) {
        uint8_t panAngle  = potToAngle(ADC_Driver.read(PAN_CHANNEL));
        uint8_t tiltAngle = potToAngle(ADC_Driver.read(TILT_CHANNEL));

        pan.write(panAngle);
        tilt.write(tiltAngle);

        USART0.write_P(PSTR("pan="));
        USART0.writeInt(panAngle);
        USART0.write_P(PSTR(" deg  tilt="));
        USART0.writeInt(tiltAngle);
        USART0.writeLine_P(PSTR(" deg"));

        delay_ms(100);
    }
}
