/*
 * DCMotor Basics — Forward / Reverse / Coast / Brake — MikroDuino Module SDK
 *
 * The simplest possible use of the DCMotor module: drive an H-bridge in
 * DIGITAL mode — no PWM pin at all — so the motor only ever runs at full
 * speed in one direction, the other direction, or stopped one of two
 * ways. This is project 1 of 6 in the examples/Modules/DCMotor series,
 * which walks the DCMotor class from a single on/off direction switch up
 * to a capstone robot-control dashboard combining DCMotor, LCD, Button,
 * ADC and EEPROM.
 *
 * Hardware (ATmega328P @ 16 MHz, generic 2-channel H-bridge driver —
 * L298N, L293D, TB6612, etc. — only ONE channel used here):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ Signal  │ Pin   │ Wiring                                    │
 *   ├─────────┼───────┼──────────────────────────────────────────┤
 *   │ IN1     │ PD4   │ H-bridge direction input 1                │
 *   │ IN2     │ PD5   │ H-bridge direction input 2                │
 *   │ ENA     │ —     │ Tied directly to 5V (always enabled) —    │
 *   │         │       │ this project deliberately does NOT wire   │
 *   │         │       │ ENA to the MCU, so speed control is purely│
 *   │         │       │ digital: full speed or stopped, nothing   │
 *   │         │       │ in between. Project 2 wires ENA to a PWM  │
 *   │         │       │ pin and unlocks variable speed.            │
 *   │ TXD     │ PD1   │ USB-serial adapter, 9600 8N1, for status  │
 *   │         │       │ messages printed each phase                │
 *   │ Motor   │ OUT1/2│ DC motor terminals                        │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * DCMotor concepts introduced:
 *   - DCMotor(in1, in2) — the constructor with only two arguments. The
 *     third argument (pwmPin) defaults to DCMotor::NO_PWM, which puts
 *     the object in digital-only mode: IN1/IN2 are driven HIGH/LOW for
 *     direction, and there is no OCR register to scale, so any nonzero
 *     speed() request just drives the motor flat-out.
 *   - begin() — configures IN1/IN2 as outputs and drives both LOW
 *     (stopped/coasting) as a safe initial state. Because pwmPin is
 *     NO_PWM here, begin() skips the PWM-timer setup entirely — that
 *     code path only runs in project 2 onward.
 *   - speed(pct) — takes -100..100. In digital mode the MAGNITUDE is
 *     ignored (there's no duty cycle to scale — see DCMotor.hpp's
 *     _setPWM(), which becomes a no-op when _ocr is nullptr); only the
 *     SIGN matters: positive drives IN1 HIGH/IN2 LOW (arbitrarily "CW"),
 *     negative drives the opposite, and speed(0) is equivalent to
 *     coast().
 *   - coast() — both IN pins LOW. Most H-bridge ICs tri-state (float)
 *     the output stage in this state, so the motor is disconnected and
 *     spins down on its own momentum/friction — a "soft", gradual stop.
 *   - brake() — both IN pins HIGH. This shorts the motor's own back-EMF
 *     across itself through the H-bridge, which resists rotation
 *     electromagnetically — a "hard", near-instant stop compared to
 *     coast(). Feel the difference: coast() lets the motor coast down
 *     over roughly a second; brake() stops it almost the instant it's
 *     called.
 */

#include <util/delay.h>
#include <mikroduino/usart.hpp>
#include <DCMotor.hpp>

using namespace MikroDuino;

// pwmPin omitted -> defaults to DCMotor::NO_PWM -> digital-only mode.
DCMotor motor(PD4, PD5);

int main() {
    USART0.begin(9600);
    USART0.writeLine_P(PSTR("DCMotor forward/reverse/coast/brake"));
    USART0.writeLine_P(PSTR("===================================="));
    USART0.writeLine_P(PSTR("No PWM pin wired: speed() only controls direction,"));
    USART0.writeLine_P(PSTR("not how fast the motor spins."));
    USART0.writeLine_P(PSTR(""));

    motor.begin();

    while (true) {
        USART0.writeLine_P(PSTR("FORWARD (speed +100)"));
        motor.speed(100);
        _delay_ms(2000);

        USART0.writeLine_P(PSTR("COAST (float, spins down freely)"));
        motor.coast();
        _delay_ms(1000);

        USART0.writeLine_P(PSTR("REVERSE (speed -100)"));
        motor.speed(-100);
        _delay_ms(2000);

        USART0.writeLine_P(PSTR("BRAKE (shorted, stops almost instantly)"));
        motor.brake();
        _delay_ms(1000);
    }
}
