# DCMotor

H-bridge DC motor driver with optional hardware PWM speed control (L298N, L293D, TB6612FNG)

**Header:** `include/DCMotor.hpp`
**Header-only** — no `.cpp` required

## Overview

`DCMotor` drives a single DC motor through any IN1/IN2 H-bridge (L298N, L293D,
TB6612FNG, or equivalent). Direction is controlled with two plain GPIO pins.
Speed control is optional:

- **No PWM pin** (digital mode) — the motor runs at full speed in either
  direction, or stops. Use this when the H-bridge's enable pin is tied high
  in hardware, or when you only need on/off/reverse control.
- **PWM pin given** — `speed()` scales output linearly from 0–100 % duty
  cycle via hardware PWM (Timer0 or Timer2 on ATmega328P). Timer1 is
  reserved for the [Servo](../Servo/README.md) module, so it is not used here.

`begin()` configures the direction pins and, if a PWM pin was supplied,
initializes the correct timer for that pin. `brake()` short-circuits both
motor terminals for a fast stop; `coast()` floats both terminals so the
motor spins down freely.

## Wiring

```
MCU IN1 ─────────────► H-bridge IN1
MCU IN2 ─────────────► H-bridge IN2
MCU PWM pin ─────────► H-bridge ENA / PWMA   (omit for digital mode)
                        H-bridge OUT1/OUT2 ──► Motor terminals
```

### Supported PWM pins (ATmega328P)

| Pin | Timer | Notes |
|---|---|---|
| `PD6` (OC0A) | Timer0 | ~977 Hz PWM |
| `PD5` (OC0B) | Timer0 | ~977 Hz PWM |
| `PB3` (OC2A) | Timer2 | ~977 Hz PWM |
| `PD3` (OC2B) | Timer2 | ~977 Hz PWM |

Any other pin passed as `pwmPin` falls back to digital (no PWM) mode.
Timer1 (OC1A/OC1B) is reserved for the Servo module — do not use it here.

## API

| Member | Description |
|---|---|
| `DCMotor(uint8_t in1, uint8_t in2, uint8_t pwmPin = NO_PWM)` | Constructor. `in1`/`in2` are direction GPIO pins; `pwmPin` is optional (`NO_PWM` = digital-only mode). |
| `void begin()` | Configures direction pins as outputs and initializes the PWM timer if `pwmPin` was given. Call once before use. |
| `void speed(int8_t pct)` | Sets speed/direction: `-100` (full CCW) … `0` (coast) … `+100` (full CW). Without a PWM pin, any non-zero magnitude runs at full speed. |
| `void brake()` | Drives both IN pins HIGH — actively shorts the motor terminals for a fast stop. |
| `void coast()` | Drives both IN pins LOW — floats the motor terminals so it spins down freely. |

## Example

```cpp
#include <mikroduino/mikroduino.hpp>
#include <DCMotor.hpp>
using namespace MikroDuino;

// L298N: left motor IN1=PD4, IN2=PD5, ENA=PD6 (OC0A)
//        right motor IN1=PB0, IN2=PB1, ENB=PB3 (OC2A)
DCMotor leftMotor (PD4, PD5, PD6);
DCMotor rightMotor(PB0, PB1, PB3);

int main() {
    leftMotor.begin();
    rightMotor.begin();

    // Drive forward at 80 %
    leftMotor.speed(80);
    rightMotor.speed(80);
    _delay_ms(2000);

    // Turn left: right motor faster
    leftMotor.speed(40);
    rightMotor.speed(80);
    _delay_ms(600);

    // Stop
    leftMotor.brake();
    rightMotor.brake();
    while (true) {}
}
```

Digital-only mode (no speed control, just direction/stop):

```cpp
DCMotor motor(PD4, PD5);   // no pwmPin → NO_PWM
motor.begin();
motor.speed(100);   // full speed, CW
motor.speed(-100);  // full speed, CCW
motor.brake();
```

## See also

- Full reference already written: [`sdk/docs/core-libraries.md`](../../../sdk/docs/core-libraries.md), section 3, "DC Motor".
- [Servo](../Servo/README.md) — shares Timer1, so DCMotor deliberately avoids it.
- [Stepper](../Stepper/README.md) — for STEP/DIR stepper motors instead of brushed DC.
