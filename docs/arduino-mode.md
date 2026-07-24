# Arduino Compatibility Mode

MikroDuino includes an optional Arduino compatibility layer that lets you write
code using the exact same function names you know from the Arduino IDE —
`pinMode`, `digitalWrite`, `analogRead`, `Serial.print`, `millis`, and more —
while still compiling with the MikroDuino toolchain on bare AVR hardware.

---

## Table of Contents

1. [Enabling Arduino Mode](#1-enabling-arduino-mode)
2. [The One Difference: ARDUINO\_MAIN()](#2-the-one-difference-arduino_main)
3. [Pin Numbering](#3-pin-numbering)
4. [GPIO Functions](#4-gpio-functions)
5. [Analog Functions](#5-analog-functions)
6. [Timing Functions](#6-timing-functions)
7. [Serial](#7-serial)
8. [Math and Utility Macros](#8-math-and-utility-macros)
9. [Interrupts](#9-interrupts)
10. [pulseIn](#10-pulsein)
11. [random](#11-random)
12. [What is Not Included](#12-what-is-not-included)
13. [Mixing Arduino and MikroDuino APIs](#13-mixing-arduino-and-mikroduino-apis)
14. [Full Example Walkthrough](#14-full-example-walkthrough)

---

## 1. Enabling Arduino Mode

### Step 1 — add include paths and source file to your `.mdp`

```json
"build": {
  "includeDirs": [
    "../../sdk/core/avr/include",
    "../../sdk/compat/include"
  ]
},
"sourceFiles": [
  "src/main.cpp",
  "../../sdk/compat/src/Arduino.cpp"
]
```

Both paths assume your project lives in `Examples/MyProject/`.
Adjust `../../` if your project is at a different depth from the repo root.

### Step 2 — include the header

```cpp
#include <Arduino.h>
```

That one include gives you every Arduino function, constant, and the `Serial`
object. You do **not** need to include `<avr/io.h>` or `<util/delay.h>`
separately — `Arduino.h` pulls in everything it needs.

---

## 2. The One Difference: ARDUINO\_MAIN()

The Arduino IDE secretly provides a `main()` function that calls your `setup()`
and `loop()`. MikroDuino does not hide this from you — you have to provide it
yourself. The `ARDUINO_MAIN()` macro does exactly that in one line:

```cpp
#include <Arduino.h>

ARDUINO_MAIN()          // ← generates: int main() { setup(); for(;;) loop(); }

void setup() {
    Serial.begin(9600);
    pinMode(13, OUTPUT);
}

void loop() {
    digitalWrite(13, HIGH);
    delay(500);
    digitalWrite(13, LOW);
    delay(500);
}
```

Place `ARDUINO_MAIN()` once per project, outside any function, anywhere in
your `.cpp` file. That is the only structural change from what you would write
in the Arduino IDE.

If you prefer to keep everything inside an explicit `main()`, that works too:

```cpp
#include <Arduino.h>

int main() {
    Serial.begin(9600);
    pinMode(13, OUTPUT);

    while (1) {
        digitalWrite(13, HIGH); delay(500);
        digitalWrite(13, LOW);  delay(500);
    }
}
```

---

## 3. Pin Numbering

On ATmega328P the Arduino pin numbers map to AVR port pins as follows:

| Arduino pin | AVR pin | Notes |
|-------------|---------|-------|
| 0 | PD0 | USART RX |
| 1 | PD1 | USART TX |
| 2 | PD2 | INT0 |
| 3 | PD3 | INT1 / OC2B |
| 4 | PD4 | |
| 5 | PD5 | OC0B |
| 6 | PD6 | OC0A |
| 7 | PD7 | |
| 8 | PB0 | |
| 9 | PB1 | OC1A — **analogWrite supported** |
| 10 | PB2 | OC1B — **analogWrite supported** |
| 11 | PB3 | OC2A / MOSI |
| 12 | PB4 | MISO |
| 13 | PB5 | SCK / built-in LED |
| A0 (14) | PC0 | ADC0 |
| A1 (15) | PC1 | ADC1 |
| A2 (16) | PC2 | ADC2 |
| A3 (17) | PC3 | ADC3 |
| A4 (18) | PC4 | ADC4 / SDA |
| A5 (19) | PC5 | ADC5 / SCL |

Use `A0`–`A5` as named constants in your code:

```cpp
int val = analogRead(A0);    // same as analogRead(14)
```

---

## 4. GPIO Functions

### `pinMode(pin, mode)`

Sets a pin's direction. Must be called before using `digitalWrite` or
`digitalRead`.

```cpp
pinMode(13, OUTPUT);        // drive the pin
pinMode(2,  INPUT);         // floating input — add external pull resistor
pinMode(2,  INPUT_PULLUP);  // input with internal ~50 kΩ pull-up to VCC
```

| Mode | Constant | Effect |
|------|----------|--------|
| Output | `OUTPUT` | Pin is driven HIGH or LOW |
| Input | `INPUT` | Pin is high-impedance — needs external pull |
| Input with pull-up | `INPUT_PULLUP` | Internal ~50 kΩ pull-up enabled |

### `digitalWrite(pin, value)`

Sets an output pin HIGH or LOW.

```cpp
digitalWrite(13, HIGH);    // 5 V (or 3.3 V on 3.3 V boards)
digitalWrite(13, LOW);     // 0 V / GND
```

### `digitalRead(pin)`

Returns `HIGH` (1) or `LOW` (0).

```cpp
int state = digitalRead(2);
if (state == LOW) {
    // button is pressed (wired to GND with INPUT_PULLUP)
}
```

---

## 5. Analog Functions

### `analogRead(pin)`

Reads the voltage on an analog input pin and returns a number from 0 (0 V)
to 1023 (VCC, typically 5 V). Uses the AVcc reference by default.

```cpp
int raw = analogRead(A0);                    // 0–1023
float volts = raw * (5.0f / 1023.0f);       // 0.0–5.0 V
```

You can also pass the channel number directly:

```cpp
int raw = analogRead(0);    // same as analogRead(A0)
```

### `analogWrite(pin, value)`

Generates a PWM signal on the pin with a duty cycle proportional to `value`
(0 = always LOW, 255 = always HIGH).

```cpp
analogWrite(9,  0);     // off
analogWrite(9,  128);   // ~50 % duty cycle
analogWrite(9,  255);   // full on
```

**Supported pins on ATmega328P: 9 (OC1A) and 10 (OC1B) only.**
All other pins are silently ignored. For PWM on pins 3, 5, 6, 11 use
`MikroDuino::PWM1` or direct timer registers.

The PWM frequency is fixed at approximately 490 Hz (matching the Arduino
default for Timer1).

---

## 6. Timing Functions

### `delay(ms)`

Stops execution for the given number of milliseconds.

```cpp
delay(1000);    // pause 1 second
```

Simple and readable, but it blocks everything else. For non-blocking timing,
use `millis()` subtraction (see below).

### `delayMicroseconds(us)`

Pauses for the given number of microseconds.

```cpp
delayMicroseconds(100);    // 100 µs pause
```

### `millis()`

Returns the number of milliseconds since `Arduino::beginTimekeeping()` was
called. The counter wraps around to 0 after approximately 49 days.

```cpp
uint32_t now = millis();
```

**You must call `Arduino::beginTimekeeping()` once in `setup()` before using
`millis()` or `micros()`.** This starts Timer0.

### `micros()`

Returns microseconds since timekeeping started.

```cpp
uint32_t t = micros();
```

Resolution is 4 µs at 16 MHz (limited by the Timer0 prescaler).

### Non-blocking timing pattern

```cpp
uint32_t lastAction = 0;

void loop() {
    uint32_t now = millis();
    if (now - lastAction >= 500) {
        lastAction = now;
        // runs every 500 ms without blocking
        digitalWrite(13, !digitalRead(13));
    }
    // other code here runs every loop iteration
}
```

This is the standard Arduino pattern for doing multiple things at once.
`delay()` prevents any other code from running during the pause; `millis()`
subtraction does not.

---

## 7. Serial

`Serial` is a global `HardwareSerial` object that wraps `MikroDuino::USART0`.

### Setup

```cpp
Serial.begin(9600);     // call once in setup()
```

Always call `Serial.begin()` before any `Serial.print()`. The baud rate must
match your terminal (MikroDuino Serial Monitor default is 9600).

### Printing

```cpp
Serial.print("text");           // string, no newline
Serial.println("text");         // string + \r\n

Serial.print(42);               // integer decimal
Serial.println(-7);             // integer + newline

Serial.print(3.14f);            // float, 2 decimal places by default
Serial.print(3.14f, 4);         // float, 4 decimal places
Serial.println(3.14f);          // float + newline

Serial.print('A');              // single character

Serial.println();               // blank line (just \r\n)
```

### Printing in different bases

Pass `HEX`, `OCT`, or `BIN` as the second argument:

```cpp
Serial.print(255, HEX);         // "FF"
Serial.print(255, OCT);         // "377"
Serial.print(255, BIN);         // "11111111"
Serial.print(255, DEC);         // "255"  (same as default)
```

### Receiving

```cpp
if (Serial.available()) {       // true if a byte is waiting
    int c = Serial.read();      // read one byte (returns -1 if none)
    Serial.write((uint8_t)c);   // echo it back
}
```

`Serial.available()` returns the number of bytes ready to read (0 or 1 — the
hardware buffer holds one byte without interrupts).

### flush

```cpp
Serial.flush();     // block until all transmitted bytes are physically sent
```

Useful before `delay()`, sleep, or reconfiguration.

---

## 8. Math and Utility Macros

These behave identically to their Arduino counterparts.

```cpp
map(value, fromLow, fromHigh, toLow, toHigh)
```
Scales `value` from one range to another. Returns a `long`.

```cpp
int percent = map(analogRead(A0), 0, 1023, 0, 100);
```

```cpp
constrain(value, low, high)
```
Clamps `value` between `low` and `high`.

```cpp
int safe = constrain(percent, 0, 100);
```

```cpp
min(a, b)   max(a, b)   abs(x)   sq(x)
```
Standard minimum, maximum, absolute value, square.

```cpp
float radians = 45.0f * DEG_TO_RAD;
float degrees = 1.5707f * RAD_TO_DEG;
```

Math constants: `PI`, `HALF_PI`, `TWO_PI`, `DEG_TO_RAD`, `RAD_TO_DEG`.

---

## 9. Interrupts

```cpp
noInterrupts();     // disable all interrupts (cli)
interrupts();       // re-enable interrupts (sei)
```

These are direct wrappers for the AVR `cli()` and `sei()` instructions.
Use them to protect shared variables accessed from both an ISR and main code:

```cpp
volatile uint32_t counter = 0;

ISR(INT0_vect) {
    counter++;
}

void loop() {
    noInterrupts();
    uint32_t snap = counter;   // atomic read
    interrupts();

    Serial.println(snap);
}
```

---

## 10. pulseIn

```cpp
uint32_t pulseIn(uint8_t pin, uint8_t state, uint32_t timeout)
```

Waits for a pulse on `pin`, measures its duration in microseconds, and returns
it. Returns 0 if the timeout (in µs) expires first.

```cpp
uint32_t duration = pulseIn(7, HIGH, 30000);  // wait up to 30 ms
```

Typical use — HC-SR04 ultrasonic distance sensor:

```cpp
const uint8_t TRIG = 4;
const uint8_t ECHO = 5;

void setup() {
    Serial.begin(9600);
    Arduino::beginTimekeeping();
    pinMode(TRIG, OUTPUT);
    pinMode(ECHO, INPUT);
}

void loop() {
    // Send 10 µs trigger pulse
    digitalWrite(TRIG, LOW);  delayMicroseconds(2);
    digitalWrite(TRIG, HIGH); delayMicroseconds(10);
    digitalWrite(TRIG, LOW);

    // Measure echo
    uint32_t us = pulseIn(ECHO, HIGH, 30000);
    uint32_t cm = us / 58;          // sound speed at 20 °C
    Serial.print(cm); Serial.println(" cm");
    delay(200);
}
```

---

## 11. random

```cpp
randomSeed(seed);               // initialise the RNG
int32_t n = random(lo, hi);    // return a number in [lo, hi)
```

Seed from an unconnected ADC pin for better randomness:

```cpp
randomSeed(analogRead(A3));     // floating pin reads noise
int32_t dice = random(1, 7);    // 1–6 inclusive
```

---

## 12. What is Not Included

The following Arduino features are intentionally absent because they pull in
significant code, depend on unsupported hardware, or are rarely needed on bare
AVR without an OS:

| Feature | Alternative |
|---------|-------------|
| `tone()` / `noTone()` | Use `MikroDuino::Timer0` in CTC mode |
| `shiftIn()` / `shiftOut()` | Use `MikroDuino::SPI` or bit-bang manually |
| `attachInterrupt()` | Use `MikroDuino::InterruptManager::attach()` |
| `Wire` (I2C) | Use `MikroDuino::I2C` |
| `SPI` object | Use `MikroDuino::SPI` |
| `String` class | Use `char` arrays and `<string.h>` / `<stdio.h>` |
| `PROGMEM` / `pgm_read_*` | Use `<avr/pgmspace.h>` directly |
| `Serial1`, `Serial2` | Use `MikroDuino::USART1` directly |

For anything not in the compat layer, drop down to the MikroDuino API —
you can use both in the same file (see §13).

---

## 13. Mixing Arduino and MikroDuino APIs

`Arduino.h` includes `<mikroduino/mikroduino.hpp>` internally, so all
MikroDuino classes are already available when you include `Arduino.h`. You
can freely mix both styles:

```cpp
#include <Arduino.h>

ARDUINO_MAIN()

void setup() {
    Serial.begin(9600);
    Arduino::beginTimekeeping();

    // Arduino style for simple GPIO
    pinMode(13, OUTPUT);

    // MikroDuino style when you need the full API
    MikroDuino::PWM1.begin(1000);   // 1 kHz on OC1A (D9)
    MikroDuino::ADC_Driver.begin(MikroDuino::ADCRef::Internal);  // 1.1 V ref
}

void loop() {
    uint16_t raw = MikroDuino::ADC_Driver.read(0);  // precise ADC read
    int      pct = map(raw, 0, 1023, 0, 100);
    MikroDuino::PWM1.dutyA(pct);                    // apply as PWM duty
    delay(10);
}
```

The rule: use Arduino functions where they are clearest; reach for
MikroDuino classes when you need a feature the compat layer does not expose.

---

## 14. Full Example Walkthrough

The sketch below combines every feature in this guide into one project. Set
it up the same way as Step 1 — a `.mdp` with `includeDirs` pointing at
`sdk/core/avr/include` and `sdk/compat/include`, and `sourceFiles` listing
your `src/main.cpp` plus `sdk/compat/src/Arduino.cpp`.

**What it does:**

| Feature used | Where |
|---|---|
| `ARDUINO_MAIN()` | Top of file — generates `main()` |
| `Serial.begin(9600)` | `setup()` — opens serial at 9600 baud |
| `Arduino::beginTimekeeping()` | `setup()` — enables `millis()` / `micros()` |
| `pinMode()` | Configures D13, D2, D9 |
| `digitalWrite()` | Blinks D13 every 500 ms using `millis()` |
| `digitalRead()` | Reads button on D2 (INPUT_PULLUP) |
| `analogWrite()` | PWM fade on D9 when button is not pressed |
| `analogRead(A0)` | Reads potentiometer |
| `millis()` | Non-blocking blink and timed serial reports |
| `map()` / `constrain()` | Scales ADC value to percent |
| `Serial.print(float, 2)` | Prints voltage with 2 decimal places |
| `Serial.print(val, HEX)` | Prints ADC value in hex |
| `Serial.print(val, BIN)` | Prints ADC value in binary |

**Serial output (at 9600 baud, updated every second):**

```
--- MikroDuino Arduino Mode ---
Blink: D13  PWM fade: D9  ADC: A0  Button: D2

Uptime: 1002 ms   |   A0 raw:  512  =  50%  =  2.50 V   |   LED: ON
       A0 hex: 0x200   bin: 1000000000
Uptime: 2004 ms   |   A0 raw:  768  =  75%  =  3.75 V   |   LED: OFF
       A0 hex: 0x300   bin: 1100000000
```

---

*See also: `sdk/compat/include/Arduino.h` and `sdk/compat/src/Arduino.cpp`
for the full implementation. `docs/getting-started.md` for the raw AVR and
MikroDuino SDK alternatives.*
