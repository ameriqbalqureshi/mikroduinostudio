# MikroDuino Module Drivers — Developer Guide

This document covers the **module driver libraries** under `sdk/modules/` —
reusable peripheral drivers (displays, sensors, motors, keypads, timing
helpers) that sit on top of the SDK's core hardware layer (GPIO, USART, SPI,
I2C, ADC, Timer, PWM, Interrupt, EEPROM).

> This release ships the core hardware layer and these module drivers only.
> There is no opt-in `MD_INCLUDE_*` utility-library layer (ring buffers,
> software timers, PID, CRC, crypto, protocol framing, etc.) in this build —
> `#include <mikroduino/mikroduino.hpp>` pulls in the core hardware headers
> and nothing else. Non-blocking timing patterns below use `millis()` from
> the Arduino compatibility shim (`sdk/compat/`) instead.

---

## Table of Contents

**Robotics Module Layer** (`sdk/modules/` — separate headers, no guard needed)
1. [Servo](#1-servo)
2. [Stepper Motor — `Stepper`](#2-stepper-motor)
3. [DC Motor — `DCMotor`](#3-dc-motor)
4. [IR Proximity Sensor — `IRSensor`](#4-ir-proximity-sensor)
5. [IR Line Sensor Array — `IRArray<N>`](#5-ir-line-sensor-array)
6. [Robotics Combined Examples](#6-robotics-combined-examples)

**Input & UI Module Layer**
7. [IR Remote Control — `IRRemote` / `IRReceiver`](#7-ir-remote-control)
8. [Matrix Keypad — `MatrixKeypad<ROWS, COLS>`](#8-matrix-keypad)
9. [Push Button — `Button`](#9-push-button)
10. [Rotary Encoder — `RotaryEncoder`](#10-rotary-encoder)
11. [Input & UI Combined Examples](#11-input--ui-combined-examples)

**Display Module Layer**
12. [Seven-Segment Display — `SevenSeg` / `SevenSegMux<N>`](#12-seven-segment-display)
14. [SSD1306 OLED Display — `SSD1306`](#14-ssd1306-oled-display)

**Timing & Measurement Module Layer**
13. [Stopwatch & Pulse Measurement — `Stopwatch` / `PulseMeter`](#13-stopwatch--pulse-measurement)

---

## Including the module drivers

The module drivers in `sdk/modules/` are **not pulled in by `mikroduino.hpp`**.
Add the module's `include/` directory to your compiler's `-I` path and include
the header directly:

```cpp
// Add to build: -I sdk/modules/Servo/include
//               -I sdk/modules/Stepper/include
//               (etc.)
// Link with  :  sdk/modules/Servo/src/Servo.cpp    (Servo only)

#include <mikroduino/mikroduino.hpp>   // core peripherals
#include <Servo.hpp>
#include <Stepper.hpp>
#include <DCMotor.hpp>
#include <IRSensor.hpp>
#include <IRArray.hpp>
using namespace MikroDuino;
```

Several modules (`DHT22`, `IRRemote`, `LCD`, `MAX72xx`, `RotaryEncoder`,
`SevenSeg`, `SevenSegShift`, `SSD1306`, `ST7735`, `Pulse`) also ship a `.cpp`
file under `src/` that must be compiled and linked alongside the project —
noted per module below.

---
## 1. Servo

**Header:** `Servo.hpp` (from `sdk/modules/Servo/include/`)  
**Source:** `sdk/modules/Servo/src/Servo.cpp` — **must be compiled with project**  
**Hardware:** Timer1 (exclusive) · OC1A = PB1 · OC1B = PB2  
**Conflicts with:** `PWM1` core driver — do not use both simultaneously

### What it is

Hardware PWM servo driver. Timer1 runs in Fast PWM mode with ICR1 as TOP,
generating a 50 Hz (20 ms) signal at 0.5 µs resolution (at 16 MHz). Up to two
independent servo channels are available via the two Timer1 output-compare
units.

### API

| Method | Description |
|---|---|
| `Servo(channel, minUs=1000, maxUs=2000)` | `channel` 0 = OC1A, 1 = OC1B |
| `begin()` | Set pin direction, start Timer1, apply centre pulse |
| `write(angleDeg)` | Move to 0–180°; clamped to [minUs, maxUs] |
| `writeMicroseconds(us)` | Set pulse width directly in µs |
| `readMicroseconds()` | Return last commanded pulse width |
| `detach()` | Disconnect OC pin from timer (pin becomes general GPIO) |

### Quick start — pan/tilt mount

```cpp
#include <mikroduino/mikroduino.hpp>
#include <Servo.hpp>
using namespace MikroDuino;

Servo pan(0);   // OC1A → PB1
Servo tilt(1);  // OC1B → PB2

int main() {
    pan.begin();
    tilt.begin();

    pan.write(90);          // centre pan
    tilt.writeMicroseconds(1400); // slightly forward tilt

    _delay_ms(1000);
    // Sweep pan left to right
    for (uint8_t a = 0; a <= 180; a++) {
        pan.write(a);
        _delay_ms(15);
    }
    while (true) {}
}
```

### Pulse-width mapping

```
angle (°)  →  pulse (µs)  =  minUs + angle × (maxUs − minUs) / 180
   0°      →  1000 µs  (fully CCW / left)
  90°      →  1500 µs  (centre)
 180°      →  2000 µs  (fully CW / right)
```

Adjust `minUs` / `maxUs` in the constructor to match your servo's mechanical limits.

---

## 2. Stepper Motor

**Header:** `Stepper.hpp` (from `sdk/modules/Stepper/include/`)  
**Header-only** — no `.cpp` required  
**Blocking** — `step()` busy-waits; use `step(1)` from a Scheduler task for non-blocking motion

### What it is

Drives any step/direction stepper motor controller (A4988, DRV8825, TMC2208,
TMC2209, etc.) by toggling a STEP pin and setting a DIR pin. Speed is controlled
by the inter-step delay, computed from RPM or set directly in microseconds.

### API

| Method | Description |
|---|---|
| `Stepper(stepPin, dirPin, enablePin=NO_PIN)` | Constructor |
| `begin()` | Configure GPIO directions; assert /EN if pin provided |
| `setRPM(rpm, stepsPerRev=200)` | Compute step delay from speed |
| `setStepDelay(us)` | Set period directly (µs) |
| `step(n)` | Move n steps; positive=CW, negative=CCW. Blocking |
| `enable(on=true)` | Toggle /EN pin (active LOW) |

### Quick start — exact-position move

```cpp
#include <mikroduino/mikroduino.hpp>
#include <Stepper.hpp>
using namespace MikroDuino;

// A4988: STEP=PD2, DIR=PD3, /EN=PD4, 1/16 microstepping → 3200 steps/rev
Stepper motor(PD2, PD3, PD4);

int main() {
    motor.begin();
    motor.setRPM(120, 3200);   // 120 RPM in 1/16 microstep mode

    motor.step(3200);          // +1 full revolution CW
    _delay_ms(500);
    motor.step(-1600);         // −0.5 revolution CCW
    motor.enable(false);       // power down driver
    while (true) {}
}
```

### RPM → step delay formula

```
step delay (µs) = 60,000,000 / (rpm × stepsPerRev)

120 RPM, 200 steps/rev  →  60,000,000 / 24,000  =  2500 µs / step
 60 RPM, 3200 steps/rev →  60,000,000 / 192,000 =   312 µs / step
```

### Non-blocking motion with millis()

```cpp
#include <mikroduino/mikroduino.hpp>
#include <Stepper.hpp>
#include <Arduino.h>   // Arduino::beginTimekeeping() / millis()
using namespace MikroDuino;

Stepper motor(PD2, PD3);

int main() {
    Arduino::beginTimekeeping();
    motor.begin();
    motor.setRPM(60, 200);

    uint32_t lastStep = 0;
    const uint32_t periodMs = motor._stepDelay_us / 1000;
    sei();

    while (true) {
        uint32_t now = Arduino::millis();
        if (now - lastStep >= periodMs) {
            lastStep = now;
            motor.step(1);   // one step per period, no blocking delay
        }
    }
}
```

---

## 3. DC Motor

**Header:** `DCMotor.hpp` (from `sdk/modules/DCMotor/include/`)  
**Header-only** — no `.cpp` required  
**Supports:** L298N, L293D, TB6612FNG, and any IN1/IN2/ENA H-bridge

### What it is

Controls a DC motor via two direction pins and an optional hardware PWM pin
for speed control. In no-PWM mode the motor runs at full speed or stops.
With a PWM pin, `speed()` scales duty cycle linearly between 0 and 255.

### Supported PWM pins (ATmega328P)

| Pin | Timer | Encoding |
|---|---|---|
| PD6 / OC0A | Timer0 | `(3<<3)\|6 = 0x1E` or constant `PD6` |
| PD5 / OC0B | Timer0 | `PD5` |
| PB3 / OC2A | Timer2 | `PB3` |
| PD3 / OC2B | Timer2 | `PD3` |

Timer1 (OC1A/OC1B) is reserved for the Servo module.

### API

| Method | Description |
|---|---|
| `DCMotor(in1, in2, pwmPin=NO_PWM)` | Constructor |
| `begin()` | Configure GPIO + initialise PWM timer if pin set |
| `speed(pct)` | −100 (full CCW) … 0 (coast) … +100 (full CW) |
| `brake()` | Both IN pins HIGH → short-circuit braking |
| `coast()` | Both IN pins LOW → motor free-spins |

### Quick start — two-wheel robot

```cpp
#include <mikroduino/mikroduino.hpp>
#include <DCMotor.hpp>
using namespace MikroDuino;

// L298N: left motor IN1=PD4, IN2=PD5, ENA=PD6(OC0A)
//        right motor IN1=PB0, IN2=PB1, ENB=PB3(OC2A)
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

### Wiring note (L298N)

```
MCU IN1 → L298N IN1    MCU IN2 → L298N IN2
MCU PD6 → L298N ENA    (PWM speed signal)
Motor A terminals → L298N OUT1 / OUT2
```

---

## 4. IR Proximity Sensor

**Header:** `IRSensor.hpp` (from `sdk/modules/IRSensor/include/`)  
**Header-only** — no `.cpp` required  
**Modes:** DIGITAL (comparator output) · ANALOG (raw phototransistor via ADC)

### What it is

Wraps a single IR reflectance / proximity sensor. In DIGITAL mode it reads the
comparator output pin directly. In ANALOG mode it performs a blocking ADC
conversion and compares the result against a user-set threshold.

### API

| Method | Description |
|---|---|
| `IRSensor(pin, mode=DIGITAL, activeLow=true, threshold=512, adc=nullptr)` | Constructor |
| `begin()` | Set pin direction (DIGITAL: INPUT\_PULLUP; ANALOG: no-op) |
| `detected()` | true when object / line is detected |
| `readRaw()` | 0 or 1 (DIGITAL); ADC value 0–1023 (ANALOG) |
| `setThreshold(t)` | Update ADC threshold at runtime |

### DIGITAL mode (FC-51, TCRT5000 with comparator)

```cpp
#include <mikroduino/mikroduino.hpp>
#include <IRSensor.hpp>
using namespace MikroDuino;

IRSensor obstacleFront(PD2);   // active-LOW digital output
IRSensor obstacleRight(PD3);

int main() {
    obstacleFront.begin();
    obstacleRight.begin();

    while (true) {
        if (obstacleFront.detected()) {
            // reverse or turn
        }
    }
}
```

### ANALOG mode (bare TCRT5000 / QRE1113)

```cpp
#include <mikroduino/mikroduino.hpp>
#include <IRSensor.hpp>
using namespace MikroDuino;

ADCDriver adc;
// PC0 = ADC channel 0; activeLow=false because ADC rises when object is near;
// threshold 600 → detected when ADC > 600
IRSensor proximity(PC0, IRSensor::ANALOG, /*activeLow=*/false, 600, &adc);

int main() {
    adc.begin();
    proximity.begin();

    while (true) {
        uint16_t raw = proximity.readRaw();
        if (proximity.detected()) {
            // object closer than ~3 cm
        }
    }
}
```

---

## 5. IR Line Sensor Array

**Header:** `IRArray.hpp` (from `sdk/modules/IRArray/include/`)  
**Header-only** — template; no `.cpp` required  
**Template:** `IRArray<N>` where N = 2–16

### What it is

Reads N IR reflectance sensors arranged in a row and computes a continuous line
position using a weighted centre-of-mass formula. Suited for line-following
robots with either digital comparator boards or raw analog sensors.

### linePosition() formula

```
position = Σ(i × norm[i]) / Σ(norm[i])    i = 0 … N−1

Result range: 0 (line under sensor 0) … (N−1)×1000 (line under sensor N−1)
Returns -1 when no sensor fires (line lost)
```

### API

| Method | Description |
|---|---|
| `IRArray(pins, mode=DIGITAL, activeLow=true, adc=nullptr)` | Constructor |
| `begin()` | Configure pin modes |
| `read(out[N])` | Fill array with normalised values 0–1000 per sensor |
| `linePosition()` | Weighted centre-of-mass position; −1 = no line |
| `onLine()` | true if any sensor fires |
| `allOnLine()` | true if every sensor fires |
| `calibrateBlack()` | ANALOG: sample dark (line) surface; call repeatedly |
| `calibrateWhite()` | ANALOG: sample bright (background) surface; call repeatedly |
| `rawADC(i)` | ANALOG: raw ADC value for sensor i |

### Quick start — 5-sensor digital line follower

```cpp
#include <mikroduino/mikroduino.hpp>
#include <IRArray.hpp>
#include <DCMotor.hpp>
using namespace MikroDuino;

const uint8_t IR_PINS[5] = { PD2, PD3, PD4, PD5, PD6 };
IRArray<5>  ir(IR_PINS);   // digital, active-LOW
DCMotor     left (PB0, PB1, PB3);
DCMotor     right(PB4, PB5, PD3);

int main() {
    ir.begin();
    left.begin();
    right.begin();

    while (true) {
        int16_t pos = ir.linePosition(); // centre = 2000
        if (pos < 0) {
            left.brake(); right.brake(); continue; // line lost
        }
        int16_t err   = pos - 2000;          // negative=line left, positive=line right
        int8_t  lSpd  = static_cast<int8_t>(70 + err / 50);
        int8_t  rSpd  = static_cast<int8_t>(70 - err / 50);
        // clamp to ±100
        if (lSpd >  100) lSpd =  100;
        if (lSpd < -100) lSpd = -100;
        if (rSpd >  100) rSpd =  100;
        if (rSpd < -100) rSpd = -100;
        left.speed(lSpd);
        right.speed(rSpd);
    }
}
```

### Analog mode with calibration (6 sensors)

```cpp
#include <mikroduino/mikroduino.hpp>
#include <IRArray.hpp>
using namespace MikroDuino;

ADCDriver adc;
const uint8_t IR_PINS[6] = { PC0, PC1, PC2, PC3, PC4, PC5 };
IRArray<6> ir(IR_PINS, IRArray<6>::ANALOG, /*activeLow=*/false, &adc);

int main() {
    adc.begin();
    ir.begin();

    // Calibration: sweep robot back and forth over the line surface
    for (uint16_t i = 0; i < 200; i++) {
        ir.calibrateBlack();   // record dark (line) ADC maxima
        _delay_ms(10);
    }
    for (uint16_t i = 0; i < 200; i++) {
        ir.calibrateWhite();   // record bright (background) ADC minima
        _delay_ms(10);
    }

    while (true) {
        int16_t pos = ir.linePosition(); // 0..5000, centre=2500
        // ... use pos to drive a steering correction
    }
}
```

---

## 6. Robotics Combined Examples

### Example A — 3-wheel omni-robot with servo gripper

```cpp
#include <mikroduino/mikroduino.hpp>
#include <Servo.hpp>
#include <DCMotor.hpp>
using namespace MikroDuino;

// Two drive wheels + gripper servo
DCMotor driveL(PD4, PD5, PD6); // left
DCMotor driveR(PB0, PB1, PB3); // right
Servo   gripper(0);              // OC1A

int main() {
    driveL.begin();
    driveR.begin();
    gripper.begin();

    gripper.write(30);   // open gripper
    _delay_ms(500);

    // Drive forward 2 s
    driveL.speed(70); driveR.speed(70);
    _delay_ms(2000);
    driveL.brake();   driveR.brake();

    gripper.write(150);  // close gripper
    _delay_ms(500);

    // Back up
    driveL.speed(-60); driveR.speed(-60);
    _delay_ms(1000);
    driveL.coast();   driveR.coast();
    while (true) {}
}
```

### Example B — Line follower with obstacle avoidance

```cpp
#include <mikroduino/mikroduino.hpp>
#include <IRArray.hpp>
#include <IRSensor.hpp>
#include <DCMotor.hpp>
using namespace MikroDuino;

const uint8_t IR_PINS[5] = { PC0, PC1, PC2, PC3, PC4 };
ADCDriver     adc;
IRArray<5>    line(IR_PINS, IRArray<5>::ANALOG, false, &adc);
IRSensor      obstacle(PD2);   // digital front sensor

DCMotor left (PD3, PD4, PD5);
DCMotor right(PD6, PB0, PB1);

int main() {
    adc.begin();
    line.begin();
    obstacle.begin();
    left.begin();
    right.begin();

    // Quick calibration
    for (uint16_t i = 0; i < 100; i++) { line.calibrateBlack(); _delay_ms(10); }
    for (uint16_t i = 0; i < 100; i++) { line.calibrateWhite(); _delay_ms(10); }

    while (true) {
        if (obstacle.detected()) {
            left.brake(); right.brake();
            _delay_ms(300);
            left.speed(-80); right.speed(80);  // spin in place
            _delay_ms(400);
            continue;
        }
        int16_t pos = line.linePosition();  // centre = 2000
        if (pos < 0) { left.coast(); right.coast(); continue; }
        int16_t err = pos - 2000;
        left.speed(static_cast<int8_t>(65 + err / 40));
        right.speed(static_cast<int8_t>(65 - err / 40));
    }
}
```

### Example C — Stepper-driven XY plotter (two axes)

```cpp
#include <mikroduino/mikroduino.hpp>
#include <Stepper.hpp>
using namespace MikroDuino;

// Two A4988 drivers, 1/16 microstepping (200 × 16 = 3200 steps/rev)
Stepper axisX(PD2, PD3, PD4);
Stepper axisY(PD5, PD6, PB0);

void moveTo(Stepper& ax, int32_t steps, uint16_t rpm) {
    ax.setRPM(rpm, 3200);
    ax.step(steps);
}

int main() {
    axisX.begin();
    axisY.begin();

    // Draw a 2×2 cm square (lead screw: 8 mm/rev → 3200 steps/rev → 2.5 µm/step)
    // 2 cm = 20 mm / 0.0025 mm = 8000 steps per side
    moveTo(axisX,  8000, 120);   // right
    moveTo(axisY,  8000, 120);   // forward
    moveTo(axisX, -8000, 120);   // left
    moveTo(axisY, -8000, 120);   // back to origin

    axisX.enable(false);
    axisY.enable(false);
    while (true) {}
}

---

## 7. IR Remote Control

**Header:** `IRRemote.hpp` (from `sdk/modules/IRRemote/include/`)  
**Source:** `sdk/modules/IRRemote/src/IRRemote.cpp` — **must be compiled with project**  
**Protocol:** NEC standard + NEC extended + auto-repeat  
**Hardware:** TSOP38238, VS1838B, TSOP4838 (any 38 kHz demodulating IR receiver)  
**Pin:** PD2 (INT0) by default · PD3 (INT1) with `#define MD_IR_USE_INT1`  
**Timer:** Timer2 (prescaler 8, 0.5 µs/tick) — conflicts with DCMotor OC2A/OC2B only

### NEC code layout

```
bits  7:0   address        (LSB first)
bits 15:8   ~address       (inverted — used for checksum)
bits 23:16  command        (LSB first)
bits 31:24  ~command       (inverted)
```

`NECCode::valid` is true only when both byte-inversions pass. Common TV remotes
with the same brand address can differ only in `command`.

### API

| Method | Description |
|---|---|
| `IRRemote::begin()` | Init Timer2 + INT0/INT1; call `sei()` after |
| `IRRemote::available()` | true when a frame is ready |
| `IRRemote::read()` | Returns `NECCode`; clears the ready flag |
| `NECCode::address` | 8-bit device address |
| `NECCode::command` | 8-bit button code |
| `NECCode::valid` | false if checksum failed (noise / partial frame) |
| `NECCode::repeat` | true for auto-repeat (button held on remote) |

### Quick start

```cpp
#define MD_IR_USE_INT1    // use PD3 instead of PD2 (optional)
#include <mikroduino/mikroduino.hpp>
#include <IRRemote.hpp>
using namespace MikroDuino;

int main() {
    IRRemote::begin();   // PD2 (INT0) with Timer2
    sei();

    while (true) {
        if (IRRemote::available()) {
            IRRemote::NECCode c = IRRemote::read();
            if (!c.valid) continue;
            switch (c.command) {
                case 0x45: /* VOL+ */ break;
                case 0x46: /* VOL- */ break;
                case 0x47: /* CH+  */ break;
            }
        }
    }
}
```

### Identifying unknown remote codes

```cpp
// Dump raw codes to USART to identify button assignments
USART0.begin(115200);
while (true) {
    if (IRRemote::available()) {
        IRRemote::NECCode c = IRRemote::read();
        USART0.print("addr=0x");
        USART0.printHex(c.address);
        USART0.print(" cmd=0x");
        USART0.printHex(c.command);
        USART0.print(c.valid ? " OK" : " ERR");
        USART0.print(c.repeat ? " RPT" : "");
        USART0.write('\n');
    }
}
```

---

### IRReceiver — any GPIO pin (software bit-bang)

**No interrupt. No timer. Works on every pin on every AVR.**

`IRReceiver` is a companion class in the same `IRRemote.hpp` header. It decodes
NEC frames by busy-waiting and counting 10 µs ticks with `_delay_us(10.0)`.
Because 10.0 is a compile-time constant, avr-libc scales the loop count with
`F_CPU`, so the timing is F_CPU-independent.

The tradeoff is that during a 67-pulse NEC frame (~70 ms) the CPU is fully
occupied. For human-speed remotes this is acceptable. Use `IRRemote` (interrupt
mode) when you cannot afford blocking, or when you already need INT0/INT1 for
another purpose.

```cpp
// Both classes live in the same header
#include <IRRemote.hpp>
using namespace MikroDuino;

IRReceiver ir(PC5);   // attach to any GPIO — no constraint on pin bank

int main() {
    ir.begin();   // INPUT_PULLUP; no ISR, no timer configured

    while (true) {
        // ...
    }
}
```

#### API

| Method | Description |
|---|---|
| `IRReceiver(gpioPin)` | Constructor — any MikroDuino pin encoding |
| `begin()` | Configures pin as `INPUT_PULLUP` |
| `poll()` | Non-blocking. Returns `{valid=false}` if pin idle (HIGH). If LOW (frame in progress), blocks ~70 ms to decode. Call at ≤ 1 ms intervals. |
| `receive(timeoutMs=150)` | Blocking. Syncs to idle, then waits up to `timeoutMs` ms for the next complete frame. `timeoutMs=0` → same as `poll()`. |

Both methods return a `NECCode` (the same shared struct used by `IRRemote`):

| Field | Description |
|---|---|
| `address` | 8-bit device address |
| `command` | 8-bit button code |
| `valid` | false on checksum failure or noise |
| `repeat` | true when remote button is held |

#### Style A — polling (non-blocking, like a button scan)

Use when you have a main loop that runs every 1 ms or faster.

```cpp
IRReceiver ir(PC5);

int main() {
    ir.begin();

    while (true) {
        NECCode c = ir.poll();
        if (c.valid && !c.repeat) {
            handleButton(c.command);
        }
        _delay_ms(1);   // ≤ 1 ms poll interval for reliable header capture
    }
}
```

#### Style B — blocking receive (like `waitKey` on a keypad)

Use when the program is ready to wait for a button before proceeding.

```cpp
// Wait up to 5 seconds for any IR button
NECCode c = ir.receive(5000);
if (c.valid) {
    USART0.printHex(c.command);
}

// Wait indefinitely (practical "block forever")
NECCode c2 = ir.receive(60000);
```

#### Using IRReceiver as a serial keypad

Because `receive()` is synchronous and PIN-independent, you can attach multiple
`IRReceiver` objects to different GPIO pins and use them independently — or wire
one IR sensor to a spare analog pin to free up INT0/INT1 for other uses.

```cpp
// IR remote as a menu input device — no interrupts needed
#include <IRRemote.hpp>
#include <MatrixKeypad.hpp>
using namespace MikroDuino;

IRReceiver remote(PC4);

// Map NEC command byte to an ASCII character
static char remoteToChar(uint8_t cmd) {
    switch (cmd) {
        case 0x16: return '0';
        case 0x0C: return '1';  case 0x18: return '2';  case 0x5E: return '3';
        case 0x08: return '4';  case 0x1C: return '5';  case 0x5A: return '6';
        case 0x42: return '7';  case 0x52: return '8';  case 0x4A: return '9';
        case 0x45: return 'U';  // VOL+ → up
        case 0x46: return 'D';  // VOL- → down
        case 0x44: return 'O'; // OK → confirm
        default:   return '\0';
    }
}

int main() {
    remote.begin();
    USART0.begin(115200);

    while (true) {
        NECCode c = remote.receive(30000);   // wait up to 30 s
        if (!c.valid || c.repeat) continue;
        char key = remoteToChar(c.command);
        if (key) USART0.write(key);
    }
}
```

#### Timing accuracy note

The 10 µs tick loop has a systematic error of ≈ 0.5–1 µs per count (from
`GPIO::read()` instruction overhead). At 16 MHz this is ~6% of 10 µs; at 8 MHz
~12%. The ±33% acceptance windows in the thresholds absorb this error at
both clock speeds.

Pin mapping constraint: `IRReceiver` does not require PCINT or external interrupt
capable pins. Any digital I/O pin works, including PA0–PA7, PB0–PB7, PC0–PC6, PD0–PD7.

---

## 8. Matrix Keypad

**Header:** `MatrixKeypad.hpp` (from `sdk/modules/MatrixKeypad/include/`)  
**Header-only** — no `.cpp` required  
**Template:** `MatrixKeypad<ROWS, COLS>` — ROWS and COLS 1–8

### How scanning works

Each row pin is driven LOW in turn; all column pins (with internal pull-ups) are
read. A pressed key creates a LOW-LOW path from the driven row to the column.
Multi-key presses return `'\0'` to prevent ghost key ambiguity.

### Built-in key maps

```cpp
Keypad::MAP_4x4[4][4]  // '1'-'9', 'A'-'D', '*', '0', '#'
Keypad::MAP_4x3[4][3]  // '1'-'9', '*', '0', '#'
```

### API

| Method | Description |
|---|---|
| `MatrixKeypad(rows, cols, keyMap)` | Constructor; pass flat row-major char array |
| `begin()` | Set row pins OUTPUT HIGH, col pins INPUT_PULLUP |
| `scan()` | One scan pass; returns key char or `'\0'` |
| `anyPressed()` | true if any key is down |
| `waitKey(debounceMs)` | Block until a key press-and-release cycle |

### Quick start — 4×4 keypad

```cpp
#include <mikroduino/mikroduino.hpp>
#include <MatrixKeypad.hpp>
using namespace MikroDuino;

const uint8_t ROWS[4] = { PB0, PB1, PB2, PB3 };
const uint8_t COLS[4] = { PB4, PB5, PB6, PB7 };

MatrixKeypad<4,4> kp(ROWS, COLS, (const char*)Keypad::MAP_4x4);

int main() {
    kp.begin();
    char prev = '\0';

    while (true) {
        char k = kp.scan();
        if (k && k != prev) {
            // New keypress — handle it
            if (k == '#') handleEnter();
            else          handleDigit(k);
        }
        prev = k;
        _delay_ms(20);   // scan at 50 Hz; also acts as debounce
    }
}
```

### PIN entry example

```cpp
char entry[5] = {};
uint8_t pos = 0;

while (pos < 4) {
    char k = kp.waitKey(25);   // blocking; waits for full press+release cycle
    if (k >= '0' && k <= '9') { entry[pos++] = k; }
    else if (k == '*')          { pos = 0; } // clear
}
entry[4] = '\0';
// compare entry to stored PIN
```

---

## 9. Push Button

**Header:** `Button.hpp` (from `sdk/modules/Button/include/`)  
**Header-only** — no `.cpp` required  
**Self-contained:** implements its own hold-time debouncing with long-press and double-click detection

### State machine (called from `update()` every 1 ms)

```
  pin inactive ←── IDLE ───→ debouncing press
                      ↓ (after debounceMs)
                   PRESSED ──→ longPressed() after longPressMs
                      ↓ (pin released + debounced)
              check clickCount after doubleClickMs window:
                  clickCount=1 → clicked()
                  clickCount≥2 → doubleClicked()
```

### API

| Method | Description |
|---|---|
| `Button(pin, activeLow, debounceMs, longPressMs, doubleClickMs)` | Constructor |
| `begin()` | Configure pin direction |
| `update()` | Call every 1 ms |
| `pressed()` | One-shot: button just debounced to pressed |
| `released()` | One-shot: button just debounced to released |
| `clicked()` | One-shot: short press+release complete |
| `doubleClicked()` | One-shot: two clicks within doubleClickMs |
| `longPressed()` | One-shot: held for ≥ longPressMs |
| `isDown()` | true while button is held (no event) |
| `heldMs()` | ms since button was pressed |

### Quick start

```cpp
#include <mikroduino/mikroduino.hpp>
#include <Button.hpp>
using namespace MikroDuino;

Button btn(PD4);          // active-LOW, defaults: 20ms debounce, 600ms long, 350ms double
Button modeBtn(PD5, true, 20, 1000); // 1 s long press

int main() {
    btn.begin();
    modeBtn.begin();

    while (true) {
        btn.update();
        modeBtn.update();

        if (btn.clicked())       Serial0.print("click\n");
        if (btn.doubleClicked()) Serial0.print("double-click\n");
        if (btn.longPressed())   Serial0.print("long press\n");
        if (modeBtn.longPressed()) enterConfigMode();

        _delay_ms(1);
    }
}
```

### Multiple buttons, polled on a fixed period

```cpp
#include <mikroduino/mikroduino.hpp>
#include <Button.hpp>
using namespace MikroDuino;

Button up(PD2), down(PD3), sel(PD4);

void pollButtons() {
    up.update(); down.update(); sel.update();
    if (up.clicked())   menuUp();
    if (down.clicked()) menuDown();
    if (sel.clicked())  menuSelect();
    if (up.isDown() && up.heldMs() > 500) fastScrollUp();
}

int main() {
    up.begin(); down.begin(); sel.begin();
    sei();
    while (true) {
        pollButtons();
        _delay_ms(1); // 1 ms period
    }
}
```

---

## 10. Rotary Encoder

**Header:** `RotaryEncoder.hpp` (from `sdk/modules/RotaryEncoder/include/`)  
**Source:** `sdk/modules/RotaryEncoder/src/RotaryEncoder.cpp` — **must be compiled with project**  
**Interrupts:** PCINT (any pin); user routes the ISR vector  
**Relationship to Phase-4A `Encoder`:** This module is ISR-driven and self-counting;
the Phase-4A `Encoder` is a polling helper for use inside user-written ISRs.

### Quadrature decode table

```
prevAB \ currAB   00    01    11    10
   00              —    +1 CW  err  -1 CCW
   01            -1 CCW  —    +1 CW  err
   10            +1 CW  err    —   -1 CCW
   11             err  -1 CCW +1 CW   —
```

### PCINT vector routing

| Pins | ISR vector |
|---|---|
| PB0–PB7 | `ISR(PCINT0_vect)` |
| PC0–PC6 | `ISR(PCINT1_vect)` |
| PD0–PD7 | `ISR(PCINT2_vect)` |

If A and B are on different banks, define both vectors pointing to the same handler.

### API

| Method | Description |
|---|---|
| `RotaryEncoder(pinA, pinB, pinBtn=NO_PIN, debounceMs=20)` | Constructor |
| `begin()` | Configure GPIO INPUT_PULLUP + enable PCINT for A and B |
| `count()` | Atomic `int32_t` read of accumulated count |
| `resetCount(v=0)` | Atomically set count to v |
| `direction()` | Last step direction (+1/−1/0); clears on read |
| `updateButton()` | Call every 1 ms for button debounce |
| `buttonPressed()` | One-shot: button just pressed |
| `buttonReleased()` | One-shot: button just released |
| `buttonIsDown()` | Currently held |
| `buttonHeldMs()` | ms since last press |
| `_isrHandler()` | Call from user's PCINT ISR |

### Quick start — menu encoder

```cpp
#include <mikroduino/mikroduino.hpp>
#include <RotaryEncoder.hpp>
using namespace MikroDuino;

RotaryEncoder enc(PD2, PD3, PD4);   // A=PD2, B=PD3, button=PD4

ISR(PCINT2_vect) { enc._isrHandler(); }  // PD pins → PCINT2

int main() {
    enc.begin();
    sei();

    int32_t prev = 0;
    int8_t  menuItem = 0;
    const int8_t ITEMS = 5;

    while (true) {
        int32_t pos = enc.count();
        if (pos != prev) {
            menuItem = static_cast<int8_t>(
                ((pos % ITEMS) + ITEMS) % ITEMS  // wrap 0..ITEMS-1
            );
            drawMenu(menuItem);
            prev = pos;
        }

        enc.updateButton();
        if (enc.buttonPressed()) selectItem(menuItem);

        _delay_ms(1);
    }
}
```

### Volume knob with acceleration

```cpp
// Detect fast spinning and apply larger steps
int32_t lastCount = 0;
uint32_t lastMs   = 0;

while (true) {
    int32_t c = enc.count();
    if (c != lastCount) {
        int32_t delta = c - lastCount;
        // Accelerate: if moved > 3 steps in under 100 ms, scale up
        int32_t volume_delta = (delta > 3 || delta < -3) ? delta * 3 : delta;
        adjustVolume(volume_delta);
        lastCount = c;
    }
    enc.updateButton();
    if (enc.buttonPressed()) toggleMute();
    _delay_ms(1);
}
```

### Mixed-bank pins (A and B on different ports)

```cpp
RotaryEncoder enc(PB0, PD3);   // A on port B, B on port D

ISR(PCINT0_vect) { enc._isrHandler(); }  // catches PB0 changes
ISR(PCINT2_vect) { enc._isrHandler(); }  // catches PD3 changes
```

---

## 11. Input & UI Combined Examples

### Example A — IR-controlled robot

```cpp
#include <mikroduino/mikroduino.hpp>
#include <IRRemote.hpp>
#include <DCMotor.hpp>
using namespace MikroDuino;

// Remote button codes (discover yours with the dump example in §7)
constexpr uint8_t CMD_FWD   = 0x18;
constexpr uint8_t CMD_BACK  = 0x52;
constexpr uint8_t CMD_LEFT  = 0x08;
constexpr uint8_t CMD_RIGHT = 0x5A;
constexpr uint8_t CMD_STOP  = 0x1C;

DCMotor left (PD4, PD5, PD6);
DCMotor right(PB0, PB1, PB3);

int main() {
    left.begin(); right.begin();
    IRRemote::begin();
    sei();

    while (true) {
        if (!IRRemote::available()) continue;
        IRRemote::NECCode c = IRRemote::read();
        if (!c.valid && !c.repeat) continue;

        switch (c.command) {
            case CMD_FWD:   left.speed( 80); right.speed( 80); break;
            case CMD_BACK:  left.speed(-80); right.speed(-80); break;
            case CMD_LEFT:  left.speed( 40); right.speed( 80); break;
            case CMD_RIGHT: left.speed( 80); right.speed( 40); break;
            case CMD_STOP:  left.brake();    right.brake();     break;
        }
    }
}
```

### Example B — Keypad-based PIN entry with OLED feedback

```cpp
#include <mikroduino/mikroduino.hpp>
#include <MatrixKeypad.hpp>
using namespace MikroDuino;

const uint8_t ROWS[4] = { PB0, PB1, PB2, PB3 };
const uint8_t COLS[4] = { PB4, PB5, PB6, PB7 };
MatrixKeypad<4,4> kp(ROWS, COLS, (const char*)Keypad::MAP_4x4);

constexpr char CORRECT_PIN[] = "1234";
char entry[5];
uint8_t pos;

bool getPin() {
    pos = 0;
    while (pos < 4) {
        char k = kp.waitKey(25);
        if (k == '*')                    { pos = 0; }    // clear
        else if (k == '#' && pos > 0)    { break; }      // submit early
        else if (k >= '0' && k <= '9')   { entry[pos++] = k; }
    }
    entry[pos] = '\0';
    return (pos == 4) && (__builtin_strcmp(entry, CORRECT_PIN) == 0);
}

int main() {
    kp.begin();
    while (true) {
        bool ok = getPin();
        GPIO::write(PC0, ok);   // PC0 = green LED, active-HIGH
        GPIO::write(PC1, !ok);  // PC1 = red LED
        _delay_ms(1000);
        GPIO::clear(PC0); GPIO::clear(PC1);
    }
}
```

### Example C — Encoder + buttons driving a menu system

```cpp
#include <mikroduino/mikroduino.hpp>
#include <RotaryEncoder.hpp>
#include <Button.hpp>
using namespace MikroDuino;

RotaryEncoder enc(PD2, PD3, PD4);  // encoder with built-in button
Button        back(PD5);            // separate back button

ISR(PCINT2_vect) { enc._isrHandler(); }

const char* MENU[] = { "Speed", "Direction", "Calibrate", "Info", "Reset" };
constexpr uint8_t MENU_LEN = 5;

int8_t  menuSel  = 0;
int32_t prevCount = 0;

void redraw() { /* render MENU[menuSel] on LCD/OLED */ }

int main() {
    enc.begin();
    back.begin();
    sei();

    redraw();
    while (true) {
        // Encoder scroll
        int32_t c = enc.count();
        if (c != prevCount) {
            menuSel  = static_cast<int8_t>(((c % MENU_LEN) + MENU_LEN) % MENU_LEN);
            prevCount = c;
            redraw();
        }

        enc.updateButton();
        back.update();

        if (enc.buttonPressed()) enterSubmenu(menuSel);
        if (back.clicked())      goBack();
        if (back.longPressed())  returnToRoot();

        _delay_ms(1);
    }
}
```

### Example D — Multi-button hold combos

```cpp
#include <mikroduino/mikroduino.hpp>
#include <Button.hpp>
using namespace MikroDuino;

Button a(PD2), b(PD3), c(PD4);

int main() {
    a.begin(); b.begin(); c.begin();

    while (true) {
        a.update(); b.update(); c.update();

        if (a.clicked())  handleA();
        if (b.clicked())  handleB();
        if (c.clicked())  handleC();

        // Combo: A + B held together for 1 s → factory reset
        if (a.isDown() && b.isDown() && a.heldMs() > 1000 && b.heldMs() > 1000) {
            factoryReset();
        }

        _delay_ms(1);
    }
}
```

---

## 12. Seven-Segment Display

**Header:** `SevenSeg.hpp` (from `sdk/modules/SevenSeg/include/`)
**Source:** `sdk/modules/SevenSeg/src/SevenSeg.cpp` — **must be compiled with project**

Two classes in one header:

| Class | Purpose |
|---|---|
| `SevenSeg` | Single digit — direct drive, no multiplexing |
| `SevenSegMux<N>` | N digits (2–8), shared segment bus, digit-select pin per digit |

### Segment pin map

```
     a
    ---
  f|   |b
    -g-
  e|   |c
    ---
     d     ·  (dp — optional)
```

Constructor pin order: **a, b, c, d, e, f, g** (array indices 0–6).
Raw bitmask: bit0=a, bit1=b, bit2=c, bit3=d, bit4=e, bit5=f, bit6=g, bit7=dp.

Supported characters in `showChar()` / `setChar()`:
`0-9`, `A-F`, `a-f`, `-`, `_`, `G`, `H`, `h`, `I`, `J`, `L`, `n`, `o`,
`P`, `q`, `r`, `S`, `t`, `U`, `u`, `Y`. Anything else displays blank.
Use `showRaw()` / `setRaw()` for custom patterns.

### Wiring — common cathode vs common anode

| Type | Segment pins | Digit-select pins |
|---|---|---|
| Common cathode (CC) | HIGH = segment on | LOW = digit active |
| Common anode (CA) | LOW = segment on | HIGH = digit active |

Pass `commonAnode=true` to constructors for CA displays. All pin polarities
are inverted automatically.

---

### SevenSeg — single digit

```cpp
#include <SevenSeg.hpp>
using namespace MikroDuino;

const uint8_t segs[7] = {PD0, PD1, PD2, PD3, PD4, PD5, PD6}; // a-g
SevenSeg display(segs, PD7, false);  // DP on PD7, common cathode

int main() {
    display.begin();

    display.show(5);         // digit 5
    display.setDP(true);     // add decimal point (DP state preserved across show calls)
    display.showChar('H');   // character H
    display.showRaw(0x6F);   // digit 9 via raw bitmask
    display.clear();
}
```

#### SevenSeg API

| Method | Description |
|---|---|
| `SevenSeg(segPins[7], dpPin, commonAnode)` | Constructor |
| `begin()` | Set all pins OUTPUT, blank display |
| `show(digit)` | 0–15; DP state preserved across calls |
| `showChar(c)` | Display a supported character |
| `showRaw(seg)` | Raw 8-bit bitmask (bit7 = DP) |
| `setDP(on)` | Toggle decimal point without changing digit |
| `clear()` | Blank all segments |

---

### SevenSegMux<N> — N-digit multiplexed

All digits share one segment bus (7 pins a–g). Each digit has its own
common pin (cathode for CC, anode for CA). Use a transistor or ULN2003A for
digits that draw more than 25 mA total.

#### Refresh modes

| Mode | How | Rate |
|---|---|---|
| **Manual** | Call `refresh()` every ~2 ms in main loop or Scheduler | User-controlled |
| **ISR routing** | Call `_isrTick()` from any timer ISR | Any timer |
| **Auto (Timer2)** | Call `beginTimer2()` once; call `sei()` after | ~1 ms/digit at 16 MHz, prescaler 64 |

At prescaler 64 (16 MHz): N=4 → 250 Hz per digit. N=8 → 125 Hz per digit.
Both are flicker-free. Use prescaler 128 if Timer2 is otherwise configured
at ~977 Hz and you want to halve CPU ISR load.

#### Quick start — auto refresh

```cpp
#include <mikroduino/mikroduino.hpp>
#include <SevenSeg.hpp>
using namespace MikroDuino;

const uint8_t segs[7]   = {PB0,PB1,PB2,PB3,PB4,PB5,PB6};
const uint8_t digits[4] = {PC0,PC1,PC2,PC3};
SevenSegMux<4> disp(segs, digits);   // common cathode, no DP

int main() {
    disp.begin();
    disp.beginTimer2();  // Timer2 OVF, prescaler 64 → ~1 ms per tick
    sei();

    disp.print(int16_t(-273));  // "  -273" right-justified
    while (true) {}
}
```

#### Quick start — manual refresh via millis()

```cpp
#include <mikroduino/mikroduino.hpp>
#include <SevenSeg.hpp>
#include <Arduino.h>   // Arduino::beginTimekeeping() / millis()
using namespace MikroDuino;

const uint8_t segs[7]   = {PB0,PB1,PB2,PB3,PB4,PB5,PB6};
const uint8_t digits[6] = {PC0,PC1,PC2,PC3,PC4,PC5};
SevenSegMux<6> disp(segs, digits);

int main() {
    Arduino::beginTimekeeping();
    disp.begin();
    sei();
    disp.print(uint16_t(65535));

    uint32_t lastRefresh = 0;
    while (true) {
        uint32_t now = Arduino::millis();
        if (now - lastRefresh >= 2) {   // 2 ms → 83 Hz per digit
            lastRefresh = now;
            disp.refresh();
        }
    }
}
```

#### SevenSegMux API

| Method | Description |
|---|---|
| `SevenSegMux(segPins[7], digitPins[N], commonAnode, dpPin)` | Constructor |
| `begin()` | Set all pins OUTPUT, blank, prime first digit |
| `beginTimer2(prescaler=64)` | Auto-refresh via Timer2 OVF; call `sei()` after |
| `setDigit(pos, val)` | Decimal/hex digit (0–15) at pos; DP preserved |
| `setChar(pos, c)` | Character at pos; DP preserved |
| `setRaw(pos, seg)` | Raw 8-bit bitmask at pos |
| `setDP(pos, on)` | Toggle DP at pos without changing digit |
| `clear()` | Blank all positions |
| `print(int16_t, leadingZero=false)` | Right-justify signed integer; sign placed adjacent to number |
| `print(uint16_t, leadingZero=false)` | Right-justify unsigned integer |
| `printHex(uint16_t)` | Up to N hex digits, left-padded with '0' |
| `printFloat(float, decimals=1)` | Fixed-point display; excluded if `MD_NO_FLOAT` |
| `refresh()` | Advance one digit; call every ~2 ms |
| `_isrTick()` | Alias for `refresh()`, route from any timer ISR |

`print()` overflow: if the value needs more digits than N, the rightmost N
digits are shown without sign. Size N for the expected value range.

#### Timer and resource conflict table

| Other module | Timer | Conflict |
|---|---|---|
| `IRRemote::begin()` | Timer2 OVF | **Yes** — same ISR. Use `IRReceiver` + manual refresh instead. |
| `DCMotor` OC2A/OC2B | Timer2 PWM | `beginTimer2()` overwrites TCCR2A/B |
| `Servo` | Timer1 | None |
| `DCMotor` OC0A/OC0B | Timer0 | None |
| `IRReceiver` | None | None |

---

### Number formatting

```cpp
SevenSegMux<4> d(segs, digits);

d.print(int16_t(0));           // "   0"
d.print(int16_t(-42));         // "  -42"
d.print(int16_t(-42), true);   // "-042"
d.print(uint16_t(1234));       // "1234"
d.print(uint16_t(5), true);    // "0005"
d.printHex(0xBEEF);           // "bEEF"

// Float: 3.30 V on a 4-digit display
d.printFloat(3.30f, 2);        // "3.30" with DP on second digit
```

### Combined example — voltmeter

```cpp
#include <mikroduino/mikroduino.hpp>
#include <SevenSeg.hpp>
#include <Arduino.h>   // Arduino::beginTimekeeping() / millis()
using namespace MikroDuino;

const uint8_t segs[7]   = {PB0,PB1,PB2,PB3,PB4,PB5,PB6};
const uint8_t digits[4] = {PC0,PC1,PC2,PC3};
SevenSegMux<4> disp(segs, digits);
ADC            adc;

int main() {
    Arduino::beginTimekeeping();
    disp.begin();
    disp.beginTimer2();   // mux handled in background
    adc.begin();
    sei();

    uint32_t lastSample = 0;
    while (true) {
        uint32_t now = Arduino::millis();
        if (now - lastSample >= 200) {
            lastSample = now;
            uint16_t raw  = adc.read(0);           // ADC channel 0
            float    volt = raw * (5.0f / 1023.0f);
            disp.printFloat(volt, 2);               // e.g. "4.98"
        }
    }
}
```

### Combined example — countdown timer with button

```cpp
#include <mikroduino/mikroduino.hpp>
#include <SevenSeg.hpp>
#include <Button.hpp>
#include <Arduino.h>   // Arduino::beginTimekeeping() / millis()
using namespace MikroDuino;

const uint8_t segs[7]   = {PD0,PD1,PD2,PD3,PD4,PD5,PD6};
const uint8_t digits[4] = {PB0,PB1,PB2,PB3};
SevenSegMux<4> disp(segs, digits);
Button         btn(PC0);

static int16_t remaining = 9999;
static bool    running   = false;

int main() {
    Arduino::beginTimekeeping();
    disp.begin();
    disp.beginTimer2();
    btn.begin();
    sei();

    disp.print(remaining, true);

    uint32_t lastBtnPoll = 0, lastTick = 0;
    while (true) {
        uint32_t now = Arduino::millis();

        if (now - lastBtnPoll >= 1) {
            lastBtnPoll = now;
            btn.update();
            if (btn.clicked()) running = !running;
        }

        if (running && remaining > 0 && now - lastTick >= 1000) {
            lastTick = now;
            disp.print(--remaining, true);
        }
    }
}
```

---

## 13. Stopwatch & Pulse Measurement

**Header:** `Pulse.hpp` (from `sdk/modules/Pulse/include/`)
**Source:** `sdk/modules/Pulse/src/Pulse.cpp` — **must be compiled with project**

Two classes for time measurement. Both use **Timer1, prescaler 8**:
- 16 MHz → 0.5 µs per tick, 32.768 ms per overflow
- 8 MHz → 1.0 µs per tick, 65.536 ms per overflow

**Conflict:** Do not use while `Servo` is running (also owns Timer1).
No conflict with `IRRemote`, `SevenSegMux`, or `DCMotor`.

---

### Stopwatch

Measures elapsed time with the timer enabled only while running — zero CPU overhead when stopped.

```cpp
#include <Pulse.hpp>
using namespace MikroDuino;

Stopwatch sw;

int main() {
    sw.start();

    doWork();                         // code under measurement

    uint32_t us = sw.elapsedUs();    // read without stopping
    sw.stop();                        // disables Timer1

    USART0.begin(115200);
    USART0.print("elapsed us: ");
    USART0.printDec(us);
    USART0.write('\n');
}
```

#### API

| Method | Description |
|---|---|
| `start()` | Configure Timer1 (prescaler 8), reset counter, start timing |
| `stop()` | Freeze elapsed value, disable Timer1 |
| `resume()` | Re-enable Timer1 from where `stop()` left off |
| `reset()` | Stop and clear to zero |
| `elapsedUs()` | Microseconds since `start()` — safe to call while running |
| `elapsedMs()` | Milliseconds since `start()` |
| `isRunning()` | true if Timer1 is active |
| `_isrOvf()` | Route from `ISR(TIMER1_OVF_vect)` for measurements > one OVF period |

#### Short measurements (no ISR needed)

Accurate range without the OVF ISR = one Timer1 overflow period:
- 16 MHz → 32.768 ms
- 8 MHz → 65.536 ms

`elapsedUs()` uses a late-OVF correction (reads `TIFR1` atomically alongside `TCNT1`) so the last pending overflow is counted even if the ISR hasn't fired. This extends the no-ISR range to just under **two** overflow periods.

#### Long measurements (with ISR)

For measurements beyond one overflow period, add to user code:

```cpp
ISR(TIMER1_OVF_vect) { sw._isrOvf(); }
// sw must be accessible here (global or extern)
```

Maximum range with ISR (uint16_t OVF counter):
- 16 MHz → 65535 × 32.768 ms ≈ **35 minutes**
- 8 MHz → 65535 × 65.536 ms ≈ **71 minutes**

#### stop / resume pattern

```cpp
sw.start();
doPartA();
sw.stop();          // Timer1 off while other code runs

doSomethingElse();  // not measured — Timer1 is off

sw.resume();        // Timer1 back on, time accumulates
doPartB();
sw.stop();

// elapsedUs() = time(partA) + time(partB) only
uint32_t total = sw.elapsedUs();
```

---

### PulseMeter

Blocking GPIO measurement. Timer1 overflow is **polled inside the measurement loop** — no ISR required even for measurements spanning many overflow periods.

```cpp
#include <Pulse.hpp>
using namespace MikroDuino;

PulseMeter pm(PC2);

int main() {
    pm.begin();

    // Pulse width — e.g. HC-SR04 echo pin
    uint32_t us = pm.pulseWidth(true, 30000UL); // HIGH pulse, 30 ms timeout
    // distance cm = us / 58

    // Period of a signal
    uint32_t T_us = pm.period(true, 500000UL);  // time between rising edges
    uint32_t freq = 1000000UL / T_us;           // Hz (if T_us > 0)

    // Frequency directly
    uint32_t hz = pm.frequency(200000UL);        // count over 200 ms window
}
```

#### API

| Method | Description |
|---|---|
| `PulseMeter(pin)` | Constructor — any GPIO pin |
| `begin()` | Configure pin as digital input (floating) |
| `pulseWidth(polarity, timeoutUs)` | µs width of one pulse; 0 on timeout |
| `period(risingEdge, timeoutUs)` | µs between two same-polarity edges; 0 on timeout |
| `frequency(windowUs)` | Hz — count rising edges over window; 0 if none |

#### `pulseWidth()` — sequence

```
Signal (HIGH pulse):
  ____           ____
  idle  ↑___↓  next
        sync→↑  ←measure→↓   = pulse width

1. Sync to idle (!polarity) — discards any pulse already in progress.
2. Detect leading edge (pin → polarity).
3. Reset Timer1.
4. Count ticks until trailing edge (pin → !polarity).
```

Both the sync timeout and measurement timeout share the same `timeoutUs` budget.

#### `period()` — sequence

```
Signal (measure rising→rising):
  _________
  ↑ (sync) ↓ _____ ↑ (start clock) ↓ _____ ↑ (read)
                   ←————— one period ————————→
```

#### `frequency()` — accuracy and limits

Edges are detected by comparing consecutive GPIO reads in a tight loop. Loop iteration time is approximately 10–20 cycles (0.6–1.25 µs at 16 MHz), so signals up to ~50 kHz are detectable. Above that, aliasing may under-count.

For a given `windowUs`, frequency resolution = 1 000 000 / windowUs Hz:
- 100 ms window → ±10 Hz resolution
- 1000 ms window → ±1 Hz resolution

---

### Common use cases

#### HC-SR04 ultrasonic distance sensor

```cpp
PulseMeter echo(PC1);
const uint8_t TRIG = PC0;

void setup() {
    GPIO::output(TRIG);
    echo.begin();
}

uint16_t distanceCm() {
    // 10 µs trigger pulse
    GPIO::set(TRIG); _delay_us(10.0); GPIO::clear(TRIG);
    // Echo pulse width / 58 = distance in cm
    uint32_t us = echo.pulseWidth(true, 30000UL);
    return us ? static_cast<uint16_t>(us / 58UL) : 0u;
}
```

#### Servo pulse width verification

```cpp
PulseMeter pm(PD2);
pm.begin();

// Verify a servo signal: should be 1000–2000 µs HIGH pulse at 50 Hz
uint32_t pw = pm.pulseWidth(true, 25000UL);   // HIGH pulse
uint32_t T  = pm.period(true, 100000UL);       // full period
// Expected: pw ≈ 1000-2000 µs, T ≈ 20000 µs (50 Hz)
```

#### Code execution timing

```cpp
Stopwatch sw;
sw.start();

// Section under test
for (uint16_t i = 0; i < 1000; ++i) heavyComputation(i);

sw.stop();
uint32_t us = sw.elapsedUs(); // total µs for 1000 iterations
```

#### Tachometer (motor RPM)

```cpp
PulseMeter pm(PD3);
pm.begin();

// Hall effect sensor on PD3; 1 pulse per revolution
// Measure over 500 ms for slow motors, shorter for fast
uint32_t hz  = pm.frequency(500000UL);  // pulses per second = rev/s
uint32_t rpm = hz * 60UL;
```

#### Signal period with display

```cpp
#include <Pulse.hpp>
#include <SevenSeg.hpp>
using namespace MikroDuino;

PulseMeter       pm(PC4);
const uint8_t    segs[7]   = {PB0,PB1,PB2,PB3,PB4,PB5,PB6};
const uint8_t    digits[4] = {PC0,PC1,PC2,PC3};
SevenSegMux<4>   disp(segs, digits);

int main() {
    pm.begin();
    disp.begin(); disp.beginTimer2(); sei();

    while (true) {
        uint32_t hz = pm.frequency(200000UL);  // 200 ms window
        disp.print(static_cast<uint16_t>(hz > 9999UL ? 9999UL : hz));
        // Display shows frequency in Hz, updated every ~200 ms
    }
}

---

## 14. SSD1306 OLED Display — `SSD1306`

**Header:** `SSD1306.hpp` (`sdk/modules/SSD1306/include/`)  
**Source:** link `sdk/modules/SSD1306/src/SSD1306.cpp` with your project  
**Not included via `mikroduino.hpp`** — add `sdk/modules/SSD1306/include/` to `-I` and compile `SSD1306.cpp` alongside your sources.  
**RAM cost:** 1024 bytes (framebuffer) + ~10 bytes object state  
**Flash cost:** ~475 bytes font table (PROGMEM) + ~1.5 KB driver code

### What it is

A framebuffer-based driver for the SSD1306 128×64 OLED over I2C. All drawing operations work on a 1024-byte RAM buffer; `display()` pushes the entire buffer to the panel in one I2C burst. This decouples drawing from I/O: you can build a frame incrementally over many loop iterations and then push it at once.

**RAM note:** On ATmega328P (2 KB RAM), the 1024-byte framebuffer leaves ~1 KB for stack, variables, and other drivers. Prefer ATmega64/128 for applications that also use large arrays or deep call stacks.

### Wiring

```
SSD1306 SDA  →  PC4  (ATmega328P TWI SDA)
SSD1306 SCL  →  PC5  (ATmega328P TWI SCL)
SSD1306 VCC  →  3.3 V or 5 V (module-dependent; most breakouts include a regulator)
SSD1306 GND  →  GND

I2C address: 0x3C (SA0 pin LOW, default) or 0x3D (SA0 pin HIGH)
```

### Initialization

```cpp
#include <mikroduino/mikroduino.hpp>
#include <SSD1306.hpp>
using namespace MikroDuino;

SSD1306 oled(I2C);          // default address 0x3C
// SSD1306 oled(I2C, 0x3D); // use 0x3D if SA0 is HIGH

int main() {
    I2C.beginMaster(400000UL);  // 400 kHz for fastest framebuffer push
    oled.begin();               // init sequence + clear + display()
    sei();
    // ...
}
```

`begin()` sends the full SSD1306 init sequence, clears the framebuffer, and pushes it. The panel is ready immediately after.

### API — Framebuffer control

```cpp
void oled.clear()                    // fill framebuffer with 0x00 (all pixels off)
void oled.fill(uint8_t pattern=0xFF) // fill with pattern (0xFF = all on)
void oled.display()                  // push 1024-byte framebuffer to OLED over I2C
```

Call `display()` after any drawing operations to make them visible. At 400 kHz I2C, pushing the full buffer takes ~25 ms (~40 fps theoretical maximum).

### API — Display control

```cpp
void oled.on(bool enable)       // display on / off (framebuffer content preserved)
void oled.invert(bool inv)      // swap lit/unlit pixels globally
void oled.contrast(uint8_t lvl) // brightness 0–255 (default ~0xCF)
void oled.flip(bool h, bool v)  // mirror horizontally and/or vertically
```

`flip()` takes effect on the next `display()` call. `invert()` and `contrast()` apply instantly via a command (no framebuffer push needed).

### API — Pixel operations

```cpp
void oled.drawPixel(uint8_t x, uint8_t y, bool on=true)
bool oled.getPixel(uint8_t x, uint8_t y)
```

Origin is top-left (0,0). X runs 0–127 left→right; Y runs 0–63 top→bottom. Out-of-bounds writes are silently clipped.

### API — Drawing primitives

All primitives write to the framebuffer. Call `display()` to push.

```cpp
// Lines
void oled.drawHLine(uint8_t x, uint8_t y, uint8_t w, bool on=true)
void oled.drawVLine(uint8_t x, uint8_t y, uint8_t h, bool on=true)
void oled.drawLine(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, bool on=true)

// Rectangles
void oled.drawRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, bool on=true)
void oled.fillRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, bool on=true)

// Circles (Midpoint algorithm)
void oled.drawCircle(uint8_t cx, uint8_t cy, uint8_t r, bool on=true)
void oled.fillCircle(uint8_t cx, uint8_t cy, uint8_t r, bool on=true)
```

Pass `on=false` to erase (clear pixels) with any primitive.

### API — Bitmap (PROGMEM)

```cpp
void oled.drawBitmap(uint8_t x, uint8_t y, uint8_t w, uint8_t h,
                     const uint8_t* pgmData, bool on=true)
```

Data is in **SSD1306 native page-column format**:
- `(h/8)` pages × `w` columns of bytes
- Each byte represents 8 vertical pixels; bit 0 = topmost pixel of the page row
- `h` must be a multiple of 8

Declare bitmaps in PROGMEM to keep them out of RAM:

```cpp
// 16×8 bitmap (1 page × 16 columns)
static const uint8_t ICON[] PROGMEM = {
    0x3C,0x42,0x85,0xB1,0xB1,0x85,0x42,0x3C,  // column 0–7
    0x3C,0x42,0x85,0xB1,0xB1,0x85,0x42,0x3C   // column 8–15
};
oled.drawBitmap(56, 28, 16, 8, ICON);
```

### API — Text (5×7 font)

Character cell is 6×8 pixels (5 glyph columns + 1 spacing). ASCII 0x20–0x7E supported; out-of-range characters render as `?`.

```cpp
void oled.setCursor(uint8_t x, uint8_t y)  // x: pixel column; y: pixel row (multiples of 8 recommended)
void oled.print(char c)                    // print one character; '\n' moves to next row
void oled.print(const char* str)           // print NUL-terminated string
void oled.print_P(const char* pgmStr)      // print PROGMEM string
void oled.print(int32_t value)             // print signed decimal integer
void oled.printU(uint32_t value, uint8_t base=10)  // print unsigned, base 10 or 16
```

Text wraps automatically at the right edge. Text that goes below row 56 is clipped silently.

| `y` value | Display row |
|---|---|
| 0 | Top (pixels 0–7) |
| 8 | Second text row (pixels 8–15) |
| … | … |
| 56 | Bottom text row (pixels 56–63) |

### API — Hardware scrolling

Hardware scrolling moves GDDRAM content on the panel — it does **not** affect the framebuffer. Call `stopScroll()` before `display()` if you need to update content while scrolling.

```cpp
void oled.scrollRight(uint8_t startPage=0, uint8_t endPage=7)
void oled.scrollLeft(uint8_t startPage=0, uint8_t endPage=7)
void oled.scrollDiagRight(uint8_t startPage=0, uint8_t endPage=7)  // diagonally up-right
void oled.scrollDiagLeft(uint8_t startPage=0, uint8_t endPage=7)   // diagonally up-left
void oled.stopScroll()
```

`startPage` and `endPage` are page indices (0–7, where each page = 8 pixel rows). Diagonal scrolling uses a vertical offset of 1 row per step over the full 64-row area.

### Key constraints

- **1024 bytes RAM** — the framebuffer is always allocated; there is no partial-buffer mode.
- **Blocking I2C** — `display()` blocks for ~25 ms at 400 kHz. Do not call from an ISR.
- **Hardware scrolling vs. framebuffer** — once hardware scrolling is active, the framebuffer-to-panel mapping shifts. Stop scrolling before pushing a new frame.
- **`SSD1306.cpp` must be compiled** — add it to your build alongside project sources.
- **Not compatible with simultaneous use of other large I2C transfers** while `display()` is running.

### Example 1 — Hello World

```cpp
#include <mikroduino/mikroduino.hpp>
#include <SSD1306.hpp>
using namespace MikroDuino;

SSD1306 oled(I2C);

int main() {
    I2C.beginMaster(400000UL);
    oled.begin();

    oled.setCursor(0, 0);
    oled.print("MikroDuino");
    oled.setCursor(0, 16);
    oled.print("SSD1306 OK");
    oled.display();

    while (true) {}
}
```

### Example 2 — Sensor dashboard

Display two live sensor readings, refreshed every 200 ms:

```cpp
#include <mikroduino/mikroduino.hpp>
#include <SSD1306.hpp>
using namespace MikroDuino;

SSD1306 oled(I2C);

void showDashboard(int16_t tempC, uint16_t lightRaw) {
    oled.clear();

    oled.setCursor(0, 0);  oled.print("Temp:");
    oled.setCursor(36, 0); oled.print(static_cast<int32_t>(tempC));
    oled.setCursor(60, 0); oled.print(" C");

    oled.setCursor(0, 16); oled.print("Light:");
    oled.setCursor(42, 16); oled.print(static_cast<int32_t>(lightRaw));

    oled.drawHLine(0, 13, 128);           // separator line
    oled.display();
}

int main() {
    I2C.beginMaster(400000UL);
    ADC::begin(ADC::Ref::VCC, ADC::Prescaler::Div128);
    oled.begin();

    while (true) {
        int16_t temp  = readTempSensor();          // your function
        uint16_t light = ADC::read(ADC::Ch::ADC0);
        showDashboard(temp, light);
        _delay_ms(200);
    }
}
```

### Example 3 — Graphics demo

```cpp
#include <mikroduino/mikroduino.hpp>
#include <SSD1306.hpp>
using namespace MikroDuino;

SSD1306 oled(I2C);

int main() {
    I2C.beginMaster(400000UL);
    oled.begin();

    // Outline rectangle
    oled.drawRect(0, 0, 128, 64);

    // Filled circle
    oled.fillCircle(32, 32, 20);

    // Diagonal line
    oled.drawLine(64, 0, 127, 63);

    // Hollow circle to the right
    oled.drawCircle(100, 32, 20);

    // Text overlay
    oled.setCursor(66, 28);
    oled.print("Hi!");

    oled.display();
    while (true) {}
}
```

### Example 4 — Hardware scroll marquee

```cpp
#include <mikroduino/mikroduino.hpp>
#include <SSD1306.hpp>
using namespace MikroDuino;

SSD1306 oled(I2C);

int main() {
    I2C.beginMaster(400000UL);
    oled.begin();

    oled.setCursor(0, 24);
    oled.print("  -- MikroDuino --  ");
    oled.display();

    // Scroll rows 3–4 (pages 3 and 4, i.e. pixels 24–39) left continuously
    oled.scrollLeft(3, 4);

    while (true) {}
}
```

To stop and update the display content:

```cpp
oled.stopScroll();
oled.clear();
oled.setCursor(0, 0);
oled.print("Done.");
oled.display();
```

### Example 5 — PROGMEM bitmap splash screen

```cpp
#include <mikroduino/mikroduino.hpp>
#include <SSD1306.hpp>
#include <avr/pgmspace.h>
using namespace MikroDuino;

// 32×16 logo (2 pages × 32 columns)
static const uint8_t LOGO[2][32] PROGMEM = {
    // Page 0 (pixels row 0–7)
    { 0x00,0x00,0xFF,0x81,0x81,0xFF,0x00,0x00,
      0xFE,0x01,0x01,0xFE,0x00,0x00,0xFF,0x00,
      0x00,0xFF,0x00,0x00,0x81,0xFF,0x00,0x00,
      0xFF,0x09,0x09,0x06,0x00,0x00,0x00,0x00 },
    // Page 1 (pixels row 8–15)
    { 0x00,0x00,0x7F,0x40,0x40,0x7F,0x00,0x00,
      0x7F,0x00,0x00,0x7F,0x00,0x00,0x7F,0x40,
      0x40,0x7F,0x00,0x00,0x00,0x7F,0x00,0x00,
      0x7F,0x00,0x00,0x00,0x00,0x00,0x00,0x00 }
};

SSD1306 oled(I2C);

int main() {
    I2C.beginMaster(400000UL);
    oled.begin();

    // Center the 32×16 logo at x=48, y=24
    oled.drawBitmap(48, 24, 32, 16, reinterpret_cast<const uint8_t*>(LOGO));
    oled.display();

    _delay_ms(2000);
    oled.clear();
    oled.display();

    while (true) {}
}
```
