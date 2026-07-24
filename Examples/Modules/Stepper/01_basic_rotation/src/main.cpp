/*
 * Stepper Basics — step(), setRPM() — MikroDuino Module SDK
 *
 * The simplest possible use of the Stepper module: rotate a stepper
 * motor one full revolution clockwise, pause, then two revolutions
 * counter-clockwise. This is project 1 of 6 in the examples/Modules/
 * Stepper series, which walks the Stepper class from a single blocking
 * move up to a capstone linear-positioner dashboard with saved presets.
 *
 * Hardware (ATmega328P @ 16 MHz, A4988/DRV8825/TMC2208-compatible driver
 * in step/direction mode):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ Signal  │ Pin   │ Wiring                                    │
 *   ├─────────┼───────┼──────────────────────────────────────────┤
 *   │ STEP    │ PD2   │ Driver STEP input                         │
 *   │ DIR     │ PD3   │ Driver DIR input                          │
 *   │ /EN     │ —     │ Tied directly to GND (always enabled) —   │
 *   │         │       │ this project deliberately does not wire   │
 *   │         │       │ /EN to the MCU. Project 2 wires it up.    │
 *   │ TXD     │ PD1   │ USB-serial adapter, 9600 8N1, for status  │
 *   │ VMOT    │ —     │ External motor supply (NOT the ATmega's   │
 *   │         │       │ own 5V) — share GROUND with the board     │
 *   │ Motor   │1A/1B/ │ 4-wire bipolar stepper coils              │
 *   │         │2A/2B  │                                            │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * Stepper concepts introduced:
 *   - Stepper(stepPin, dirPin) — the two-argument constructor. The third
 *     argument (enablePin) defaults to Stepper::NO_PIN, which skips all
 *     /EN handling entirely — begin() never touches it (see project 2).
 *   - begin() — configures STEP/DIR as outputs, both driven LOW.
 *   - setRPM(rpm, stepsPerRev) — computes the per-step delay from a
 *     speed in revolutions per minute and the motor's steps/revolution
 *     (200 for a standard 1.8 deg/step motor with no microstepping).
 *   - step(n) — blocking move of n steps; positive drives DIR HIGH (CW
 *     here — the driver decides which physical direction that maps to),
 *     negative drives DIR LOW (CCW). Busy-waits between pulses, so the
 *     whole move happens before step() returns — project 5 revisits
 *     this with a non-blocking Scheduler task instead.
 */

#include <util/delay.h>
#include <mikroduino/usart.hpp>
#include <Stepper.hpp>

using namespace MikroDuino;

Stepper motor(PD2, PD3);   // step=PD2, dir=PD3, no enable pin

int main() {
    USART0.begin(9600);
    USART0.writeLine_P(PSTR("Stepper basic rotation"));
    USART0.writeLine_P(PSTR("======================="));
    USART0.writeLine_P(PSTR(""));

    motor.begin();
    motor.setRPM(60, 200);   // 60 RPM, 200 full steps/rev (1.8 deg/step)

    while (true) {
        USART0.writeLine_P(PSTR("1 revolution CW"));
        motor.step(200);
        _delay_ms(1000);

        USART0.writeLine_P(PSTR("2 revolutions CCW"));
        motor.step(-400);
        _delay_ms(1000);
    }
}
