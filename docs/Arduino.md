# MikroDuino Arduino Compatibility Reference

## Overview

The Arduino compatibility layer (`sdk/compat/`) lets you write code using familiar
Arduino-style function names — `pinMode`, `digitalWrite`, `analogRead`, `Serial.print`,
`millis`, `map`, `random` — while compiling with the MikroDuino toolchain on bare AVR.

It is entirely **opt-in**: including `Arduino.h` and adding `Arduino.cpp` to your build
is all that is needed. Projects that do not include it are unaffected.

```cpp
#include <Arduino.h>
using namespace MikroDuino;   // optional — Arduino.h already includes mikroduino.hpp
```

The layer is implemented on top of the native MikroDuino API. Every Arduino function
maps to one or more calls in `MikroDuino::GPIO`, `MikroDuino::ADC_Driver`,
`MikroDuino::PWM1`, `MikroDuino::USART0`, etc. You can call both APIs in the same
file and the same project freely.

---

## Project Setup

### Step 1 — `.mdp` project file

Add the compat include path and source file alongside your own:

```json
{
  "build": {
    "includeDirs": [
      "../../sdk/compat/include"
    ]
  },
  "sourceFiles": [
    "src/main.cpp",
    "../../sdk/compat/src/Arduino.cpp"
  ]
}
```

The paths assume your project lives at `Examples/MyProject/`. Adjust `../../` if
your project is at a different depth from the repository root.

The builder automatically adds `sdk/core/avr/include` — you do **not** need to list
the core SDK include path explicitly.

### Step 2 — include the header

```cpp
#include <Arduino.h>
```

This single include provides every Arduino function, constant, the `Serial` object,
and pulls in `<mikroduino/mikroduino.hpp>` so all native MikroDuino classes are
available in the same translation unit.

---

## `ARDUINO_MAIN()`

The Arduino IDE secretly provides a `main()` that calls `setup()` and `loop()`.
MikroDuino exposes this explicitly via a macro:

```cpp
#define ARDUINO_MAIN()      \
    extern void setup();    \
    extern void loop();     \
    int main() {            \
        setup();            \
        for (;;) loop();    \
        return 0;           \
    }
```

**Usage rules:**

- Place it once per project, outside any function, at file scope.
- You still write `setup()` and `loop()` as plain `void` functions.
- It is optional — you can write your own `main()` instead and call `setup()` /
  `loop()` manually.

```cpp
#include <Arduino.h>

ARDUINO_MAIN()   // ← place here, before setup()

void setup() { ... }
void loop()  { ... }
```

Unlike the Arduino IDE, `ARDUINO_MAIN()` does **not** call `sei()` or perform any
pre-`setup()` hardware initialisation. You are responsible for enabling timekeeping
and global interrupts in `setup()` when needed.

---

## Timekeeping

### `Arduino::beginTimekeeping()`

```cpp
namespace Arduino {
    void beginTimekeeping();
}
```

**Must be called once in `setup()` before using `millis()` or `micros()`.**

Configures Timer0 in **Fast PWM** mode with a DIV64 prescaler and enables the Timer0
overflow interrupt:

| Register | Value | Effect |
|----------|-------|--------|
| `TCCR0A` | `(1<<WGM01)\|(1<<WGM00)` | Fast PWM mode |
| `TCCR0B` | `(1<<CS01)\|(1<<CS00)` | Clock / 64 |
| `TIMSK0` | `TOIE0 = 1` | Overflow interrupt enabled |

Timer0 then overflows every **1.024 ms** (256 × 64 / 16 MHz). Each overflow
increments a `volatile uint32_t overflow_count` inside `ISR(TIMER0_OVF_vect)`.
`millis()` and `micros()` derive time from this counter.

`beginTimekeeping()` also calls `sei()` to enable global interrupts. After this
call, `millis()` and `micros()` are immediately valid.

> **Timer0 conflict.** `beginTimekeeping()` takes full ownership of Timer0. Do not
> use `MikroDuino::Timer0` after calling it — the two will overwrite each other's
> `TCCR0x` settings.

---

### `millis()`

```cpp
uint32_t millis(void);
```

Returns milliseconds elapsed since `beginTimekeeping()` was called.

**Implementation:**

```cpp
// Each overflow = 256 ticks × (64 / 16 000 000 s) = 1.024 ms
millis = (overflow_count × 64 × 256) / (F_CPU / 1000)
       = (overflow_count × 16 384) / 16 000
```

| Property | Value |
|----------|-------|
| Resolution | 1.024 ms (one step per Timer0 overflow) |
| Accuracy | Exact — the formula compensates for the 1.024 ms period |
| Rollover | After ~49.7 days (32-bit wrap of the computed value) |
| Read safety | Contains a `cli()`/`sei()` pair to read the 32-bit counter atomically |

The return value is in real milliseconds — the 1.024 ms overflow period is
compensated in the formula. However, since the counter only updates on each overflow,
calling `millis()` twice within 1.024 ms will return the same value.

---

### `micros()`

```cpp
uint32_t micros(void);
```

Returns microseconds elapsed since `beginTimekeeping()` was called.

**Implementation:**

```cpp
ticks  = (overflow_count × 256 + TCNT0) × 64
micros = ticks / (F_CPU / 1 000 000)
       = (overflow_count × 256 + TCNT0) × 4   // at 16 MHz
```

Both `overflow_count` and `TCNT0` are read inside a `cli()`/`sei()` guard to
prevent a race where the overflow fires between the two reads.

| Property | Value |
|----------|-------|
| Resolution | 4 µs (one TCNT0 tick = 64 / 16 MHz = 4 µs) |
| Read safety | `cli()`/`sei()` guard around the two-register read |
| Rollover | Same 49.7-day window as `millis()` |

---

### `delay(ms)`

```cpp
void delay(uint32_t ms);
```

Blocks for approximately `ms` milliseconds. Implemented as:

```cpp
while (ms--) _delay_ms(1.0);
```

This is a pure busy-loop from `<util/delay.h>` — it does **not** use Timer0 or any
interrupt. `delay()` can be called before `beginTimekeeping()`.

Because the CPU is completely occupied during `delay()`, **no other code runs** — ISRs
still fire (interrupts are not disabled) but the main loop is stalled. For multi-tasking
use the `millis()` non-blocking pattern instead.

---

### `delayMicroseconds(us)`

```cpp
void delayMicroseconds(uint32_t us);
```

Blocks for approximately `us` microseconds using `_delay_us(1.0)` in a loop.
Accurate to within a few CPU cycles for values ≥ 10 µs. Below ~10 µs, function call
and loop overhead dominate — use inline assembly or a calibrated NOP sequence for
sub-microsecond precision.

---

### Non-blocking timing pattern

Never use `delay()` when you need to do multiple things simultaneously. Instead,
track the last event time and compare against `millis()`:

```cpp
uint32_t lastEvent = 0;

void loop() {
    uint32_t now = millis();

    if (now - lastEvent >= 500) {   // every 500 ms
        lastEvent = now;
        digitalWrite(13, !digitalRead(13));
    }

    // other code here runs on every loop() iteration unimpeded
}
```

Always use subtraction (`now - lastEvent`) rather than `now >= lastEvent + 500`.
The subtraction is correct even after the 49.7-day `millis()` rollover; addition
overflows to zero and the condition can fire at the wrong time.

---

## GPIO

### `pinMode(pin, mode)`

```cpp
void pinMode(uint8_t pin, uint8_t mode);
```

Sets the direction and pull-up state of a pin.

| `mode` | Constant | Underlying call |
|--------|----------|-----------------|
| `OUTPUT` | `0` | `GPIO::output(mdPin)` |
| `INPUT` | `1` | `GPIO::input(mdPin)` — pull-up disabled |
| `INPUT_PULLUP` | `2` | `GPIO::inputPullup(mdPin)` — ~50 kΩ to VCC |

On ATmega328P, the Arduino pin number is translated through `ARDUINO_PIN_MAP[]`
to the MikroDuino packed-pin encoding (`_PIN(port, bit)`). On other MCUs the
pin number is passed directly as a port bit index.

---

### `digitalWrite(pin, value)`

```cpp
void digitalWrite(uint8_t pin, uint8_t value);
```

Drives an output pin HIGH (VCC) or LOW (GND).

```cpp
digitalWrite(13, HIGH);   // LED on
digitalWrite(13, LOW);    // LED off
```

If the pin was configured as `INPUT_PULLUP`, calling `digitalWrite(pin, LOW)`
**disables** the pull-up (same as switching to `INPUT`). This matches Arduino
behaviour.

---

### `digitalRead(pin)`

```cpp
int digitalRead(uint8_t pin);
```

Returns `HIGH` (1) or `LOW` (0) reflecting the current logic level.

```cpp
if (digitalRead(2) == LOW) {
    // button pressed — pin pulled to GND through INPUT_PULLUP wiring
}
```

The underlying call reads the PINx register regardless of the pin's direction,
so it works on both input and output pins.

---

### Pin mapping (ATmega328P)

| Arduino number | Constant | AVR pin | Port | Bit | Notes |
|----------------|----------|---------|------|-----|-------|
| 0 | — | PD0 | D | 0 | USART0 RX |
| 1 | — | PD1 | D | 1 | USART0 TX |
| 2 | — | PD2 | D | 2 | INT0 |
| 3 | — | PD3 | D | 3 | INT1 / OC2B |
| 4 | — | PD4 | D | 4 | |
| 5 | — | PD5 | D | 5 | OC0B |
| 6 | — | PD6 | D | 6 | OC0A |
| 7 | — | PD7 | D | 7 | |
| 8 | — | PB0 | B | 0 | |
| 9 | — | PB1 | B | 1 | OC1A — `analogWrite` supported |
| 10 | — | PB2 | B | 2 | OC1B — `analogWrite` supported |
| 11 | — | PB3 | B | 3 | OC2A / MOSI |
| 12 | — | PB4 | B | 4 | MISO |
| 13 | — | PB5 | B | 5 | SCK / on-board LED |
| A0 | `A0` = 14 | PC0 | C | 0 | ADC0 |
| A1 | `A1` = 15 | PC1 | C | 1 | ADC1 |
| A2 | `A2` = 16 | PC2 | C | 2 | ADC2 |
| A3 | `A3` = 17 | PC3 | C | 3 | ADC3 |
| A4 | `A4` = 18 | PC4 | C | 4 | ADC4 / SDA |
| A5 | `A5` = 19 | PC5 | C | 5 | ADC5 / SCL |

`A0`–`A5` are `constexpr uint8_t` constants (values 14–19). Use them interchangeably
with the numeric form — `analogRead(A0)` and `analogRead(14)` are identical.

---

## Analog I/O

### `analogRead(pin)`

```cpp
int analogRead(uint8_t pin);
```

Performs a blocking 10-bit ADC conversion and returns a value from **0** (0 V) to
**1023** (VCC).

```cpp
int raw   = analogRead(A0);
float v   = raw * (5.0f / 1023.0f);   // voltage at 5 V VCC
int   pct = map(raw, 0, 1023, 0, 100);
```

**Implementation notes:**

- `ADC_Driver.begin()` is called on **every** `analogRead()` invocation, always
  with the defaults: AVCC reference, DIV128 prescaler (125 kHz ADC clock), no
  left-adjust.
- If you have configured the ADC using `MikroDuino::ADC_Driver.begin(ref, psr)`,
  each `analogRead()` call will silently reset it to AVCC + DIV128.
- For custom reference (e.g. internal 1.1 V) or free-running mode, bypass
  `analogRead()` and use `MikroDuino::ADC_Driver` directly.
- You may pass pin numbers as ADC channels (0–5) or Arduino constants (A0–A5):
  both forms are accepted — `analogRead(0)` and `analogRead(A0)` are equivalent.
- One conversion takes approximately 104 µs at 125 kHz ADC clock (13 ADC clock
  cycles × 8 µs per cycle).

---

### `analogWrite(pin, value)`

```cpp
void analogWrite(uint8_t pin, uint8_t value);
```

Generates a PWM signal on `pin` with a duty cycle proportional to `value`
(0 = always LOW, 255 = always HIGH).

```cpp
analogWrite(9, 0);     // off
analogWrite(9, 128);   // ~50 % duty
analogWrite(9, 255);   // full on
```

**Supported pins: 9 (OC1A) and 10 (OC1B) only.**
All other pin numbers are silently ignored — no compile error, no runtime error.

| Property | Value |
|----------|-------|
| PWM frequency | ~490 Hz (matches Arduino Uno Timer1 default) |
| Resolution | 8-bit input (0–255), mapped internally to ICR1 ticks |
| Hardware | Timer1 Fast PWM, ICR1 as TOP, prescaler DIV8 |
| Underlying call | `PWM1.begin(490)` then `PWM1.rawA()` or `PWM1.rawB()` |

**Implementation notes:**

- `PWM1.begin(490)` is called on **every** `analogWrite()` invocation. This
  reconfigures TCCR1A, TCCR1B, and ICR1 on each call. The overhead is negligible
  for infrequent calls but may cause a one-cycle glitch when both channels are
  active and one is updated.
- `analogWrite()` uses Timer1, which conflicts with `MikroDuino::Timer1`. Do not
  use both at the same time.
- `value = 0` does not produce a clean LOW — FastPWM generates one narrow spike
  per period at 0 % duty. Call `digitalWrite(pin, LOW)` for true zero output.
- For PWM on pins 3, 5, 6, or 11 use `MikroDuino::Timer0`/`Timer2` directly;
  those pins are not reachable via `analogWrite()`.

---

## Serial

`Serial` is a globally available `HardwareSerial` object that wraps
`MikroDuino::USART0`. It provides a subset of the Arduino `Serial` API.

### `Serial.begin(baud)`

```cpp
void begin(uint32_t baud);
```

Initialises USART0 at `baud` bits per second, 8N1 (8 data bits, no parity,
1 stop bit), TX+RX enabled. Must be called before any print or read operation.

```cpp
Serial.begin(9600);     // standard Arduino baud rate
Serial.begin(115200);   // faster — recommended for large output
```

### `Serial.end()`

```cpp
void end();
```

Disables TX and RX, clears all interrupt enable bits for USART0.

---

### Printing — `Serial.print()` and `Serial.println()`

Both methods have the same overload set. `println()` appends `\r\n` after the value;
`print()` does not.

#### String

```cpp
void print(const char* s);
void println(const char* s);
```

Transmits a null-terminated C string from SRAM.

```cpp
Serial.println("Hello, world");
```

> **SRAM note.** String literals are stored in SRAM on AVR. Each `"..."` literal in
> your code consumes bytes from the 2 KB (ATmega328P) or 4 KB (ATmega64/128) heap.
> For programs with many string literals, use `MikroDuino::USART0.write_P(PSTR("..."))`
> to store the string in flash and read it byte-by-byte without SRAM overhead.

#### Integer

```cpp
void print(int v);
void print(int32_t v);
void println(int v);
void println(int32_t v);
```

Prints a signed decimal integer. Negative values print a leading `-`.

```cpp
Serial.print(-42);      // "-42"
Serial.println(1024);   // "1024\r\n"
```

#### Integer with base

```cpp
void print(uint32_t v, uint8_t base = DEC);
void println(uint32_t v, uint8_t base = DEC);
```

Prints an **unsigned** 32-bit integer in the specified base. Use the predefined
base constants:

| Constant | Value | Output for 255 |
|----------|-------|----------------|
| `DEC` | 10 | `255` |
| `HEX` | 16 | `FF` |
| `OCT` | 8 | `377` |
| `BIN` | 2 | `11111111` |

```cpp
Serial.print(255, HEX);   // "FF"  (no "0x" prefix — add it manually if needed)
Serial.print(255, BIN);   // "11111111"
Serial.print(255, OCT);   // "377"
```

> HEX output does **not** include a `0x` prefix. Print it explicitly:
> ```cpp
> Serial.print("0x"); Serial.print(value, HEX);
> ```

#### Float / double

```cpp
void print(float v,  uint8_t decimals = 2);
void println(float v, uint8_t decimals = 2);
void print(double v,  uint8_t decimals = 2);
void println(double v, uint8_t decimals = 2);
```

Prints a floating-point value with `decimals` digits after the decimal point.
Internally uses `dtostrf()` from avr-libc. The default is 2 decimal places.

```cpp
Serial.print(3.14159f);        // "3.14"
Serial.print(3.14159f, 4);     // "3.1416"
Serial.println(0.5);           // "0.50\r\n"
```

Note: `double` on AVR is the same precision as `float` (32-bit IEEE 754).

#### Character

```cpp
void print(char c);
void println(char c);
```

Transmits a single ASCII character.

```cpp
Serial.print('A');             // sends byte 0x41
Serial.println('!');           // sends 0x21 + \r\n
```

#### Blank line

```cpp
void println();
```

Transmits `\r\n` with no preceding content.

---

### `Serial.write(byte)`

```cpp
void write(uint8_t b);
```

Transmits a single raw byte. Unlike `print(char)`, this does not interpret the
value as an ASCII character — it sends the byte as-is. Use this for binary
protocols or when echoing received bytes.

```cpp
Serial.write(0xFF);    // sends the byte 0xFF
Serial.write(c);       // echo a received byte unmodified
```

---

### `Serial.available()`

```cpp
int available();
```

Returns **1** if a byte is waiting in the hardware receive register, **0** otherwise.

> The hardware USART has a **one-byte buffer** — there is no interrupt-driven
> software receive queue in the compat layer. If a second byte arrives before
> you read the first, the first byte is overwritten and lost. For reliable
> multi-byte reception at high baud rates, use `MikroDuino::USART0` with the RX
> interrupt enabled and manage your own ring buffer.

```cpp
if (Serial.available()) {
    int c = Serial.read();
    Serial.write((uint8_t)c);  // echo
}
```

---

### `Serial.read()`

```cpp
int read();
```

Waits (blocking) until a byte arrives, then returns it as an `int` (0–255).
Returns -1 if called when no data is available — but since `read()` blocks until
data arrives, it will only return -1 immediately if the USART hardware is not enabled.

Use `available()` before `read()` for non-blocking behaviour:

```cpp
if (Serial.available()) {
    int c = Serial.read();   // safe — byte is definitely there
}
```

---

### `Serial.peek()`

```cpp
int peek();
```

Returns the next byte if one is available, or -1 if not.

> **Limitation.** The compat layer has no software receive buffer. `peek()` is
> implemented by calling `read()` if a byte is available — it **consumes** the byte.
> Unlike the real Arduino `peek()`, a subsequent `read()` will block waiting for
> the next byte. Do not rely on `peek()` being non-destructive.

---

### `Serial.flush()`

```cpp
void flush();
```

Blocks until all bytes queued for transmission have been physically sent (waits for
the TX-Complete flag `TXC0`). Use before power-down, sleep, or a long delay after
printing, to ensure output is not truncated.

---

## Math and Utility Macros

These macros behave identically to their Arduino counterparts and expand to
inline C expressions.

### `map(value, fromLow, fromHigh, toLow, toHigh)`

```cpp
#define map(val,fromLow,fromHigh,toLow,toHigh) \
    ((val - fromLow) * (toHigh - toLow) / (fromHigh - fromLow) + toLow)
```

Linearly scales `value` from one range to another. Returns a `long`.

```cpp
long pct  = map(analogRead(A0), 0, 1023, 0, 100);
long duty = map(pct, 0, 100, 0, 255);
```

**Notes:**
- All arithmetic is integer. Values are not clamped — if `value` is outside
  `[fromLow, fromHigh]`, the result will be outside `[toLow, toHigh]`. Wrap with
  `constrain()` if needed.
- Division is integer division — some precision is lost, especially for small
  output ranges.
- The macro evaluates each argument multiple times; do not pass expressions with
  side effects.

---

### `constrain(value, low, high)`

```cpp
#define constrain(v, lo, hi) ((v)<(lo) ? (lo) : ((v)>(hi) ? (hi) : (v)))
```

Clamps `value` to the range `[low, high]`.

```cpp
int safe = constrain(map(raw, 0, 1023, 0, 100), 0, 100);
```

---

### `min(a, b)` / `max(a, b)`

```cpp
#define min(a,b) ((a)<(b) ? (a) : (b))
#define max(a,b) ((a)>(b) ? (a) : (b))
```

Return the smaller or larger of two values. Works with any numeric type; the
compiler promotes mixed types.

---

### `abs(x)`

```cpp
#define abs(x) ((x)>0 ? (x) : -(x))
```

Returns the absolute value of `x`. Do not pass expressions with side effects.
For `float`, the C standard library `fabsf()` from `<math.h>` is more precise
(avoids potential rounding in the macro expansion).

---

### `sq(x)`

```cpp
#define sq(x) ((x)*(x))
```

Returns `x²`. Evaluates `x` twice — do not pass expressions with side effects.

---

## Math Constants

All constants are `#define` macros (type `double`).

| Constant | Value | Meaning |
|----------|-------|---------|
| `PI` | 3.14159265358979323846 | π |
| `HALF_PI` | 1.5707963267948966192 | π / 2 |
| `TWO_PI` | 6.28318530717958647692 | 2π |
| `DEG_TO_RAD` | 0.01745329251994329577 | π / 180 |
| `RAD_TO_DEG` | 57.2957795130823208768 | 180 / π |

```cpp
float rad = 90.0f * DEG_TO_RAD;   // 1.5708 rad
float deg = rad   * RAD_TO_DEG;   // 90.00  deg

float circumference = 2.0f * PI * 5.0f;   // for radius = 5
```

On AVR, `double` and `float` are both 32-bit. The constants are defined as `double`
but are implicitly narrowed when assigned to `float`.

---

## Random Numbers

### `randomSeed(seed)`

```cpp
inline void randomSeed(uint32_t seed);
```

Initialises the pseudo-random number generator. Wraps avr-libc `srandom()`.
Call this once in `setup()`.

For a non-deterministic seed, read an unconnected (floating) analog pin — the
ADC noise makes a different value each power-on:

```cpp
randomSeed(analogRead(A3));   // leave A3 unconnected
```

If you use the same seed every run, `random()` produces the same sequence.

---

### `random(maxVal)`

```cpp
inline int32_t random(int32_t maxVal);
```

Returns a pseudo-random number in the range **[0, maxVal)**. The result is always
`< maxVal` — `maxVal` itself is never returned.

```cpp
int32_t r = random(10);    // 0 – 9
```

---

### `random(minVal, maxVal)`

```cpp
inline int32_t random(int32_t minVal, int32_t maxVal);
```

Returns a pseudo-random number in the range **[minVal, maxVal)**.

```cpp
int32_t dice = random(1, 7);      // 1 – 6  (simulates a die)
int32_t byte_ = random(0, 256);   // 0 – 255
```

---

## Interrupt Control

### `noInterrupts()` / `interrupts()`

```cpp
#define noInterrupts()  cli()
#define interrupts()    sei()
```

These are direct aliases for the AVR `cli()` (clear global interrupt enable) and
`sei()` (set global interrupt enable) instructions.

```cpp
noInterrupts();
uint32_t snapshot = sharedVar;   // atomic read — ISR cannot fire here
interrupts();
```

Use `noInterrupts()` when reading or writing multi-byte values that are also
modified by an ISR. Variables shared with an ISR must be declared `volatile`:

```cpp
volatile uint32_t g_count = 0;

ISR(TIMER1_COMPA_vect) {
    ++g_count;             // ISR writes
}

void loop() {
    noInterrupts();
    uint32_t snap = g_count;   // atomic two-byte read
    interrupts();

    Serial.println(snap);
}
```

`uint8_t` and `bool` are single-byte and inherently atomic on AVR — no guard needed
for those types.

Keep the critical section as short as possible. `Serial.print()` inside
`noInterrupts()` will deadlock because the USART TX-ready interrupt cannot fire.

---

## `pulseIn(pin, state, timeout)`

```cpp
uint32_t pulseIn(uint8_t pin, uint8_t state, uint32_t timeout);
```

Waits for a pulse of the given logic `state` (`HIGH` or `LOW`) on `pin` and
returns its duration in microseconds. Returns 0 if the timeout (in µs) expires
before the pulse completes.

**Sequence:**
1. Wait for pin to be NOT in `state` (pre-pulse idle).
2. Wait for pin to enter `state` (start of pulse).
3. Measure how long pin stays in `state`.
4. Return duration in µs via `micros()` subtraction.

Steps 1 and 2 both count against the timeout.

```cpp
// HC-SR04 ultrasonic sensor
uint32_t us = pulseIn(5, HIGH, 30000);   // up to 30 ms timeout
uint32_t cm = us / 58;
```

**Notes:**
- `pulseIn()` uses `micros()` internally — `beginTimekeeping()` must be called first.
- It is a busy-loop — the CPU is fully occupied during the wait.
- The minimum measurable pulse is limited by function call overhead (~10–20 µs).

---

## Mixing Arduino and MikroDuino APIs

`Arduino.h` includes `<mikroduino/mikroduino.hpp>` internally, so all native
MikroDuino classes are automatically available when you include `Arduino.h`.

```cpp
#include <Arduino.h>

ARDUINO_MAIN()

void setup() {
    Serial.begin(115200);
    Arduino::beginTimekeeping();

    pinMode(13, OUTPUT);                       // Arduino GPIO

    MikroDuino::PWM1.begin(20000);             // Native: 20 kHz PWM
    MikroDuino::ADC_Driver.begin(
        MikroDuino::ADCRef::Internal);         // Native: 1.1 V reference
}

void loop() {
    // Native ADC (keeps internal reference — not overwritten by analogRead())
    uint16_t raw = MikroDuino::ADC_Driver.read(0);

    // Arduino map/constrain
    int duty = constrain(map(raw, 0, 1023, 0, 100), 0, 100);

    // Native PWM
    MikroDuino::PWM1.dutyA(duty);

    delay(10);
}
```

**Mixing rules:**

| Combination | Safe? |
|-------------|-------|
| `Serial` + `MikroDuino::USART0` | No — both drive the same peripheral. Use one or the other. |
| `delay()` + `MikroDuino::Timer0` | Yes — `delay()` is a busy-loop, not timer-based. |
| `millis()` + `MikroDuino::Timer0` | No — `beginTimekeeping()` owns Timer0. |
| `analogWrite()` + `MikroDuino::PWM1` | No — both drive Timer1. |
| `analogRead()` + `MikroDuino::ADC_Driver` | No — `analogRead()` resets ADC config on every call. |
| `pinMode/digitalWrite/digitalRead` + `MikroDuino::GPIO` | Yes — both access the same registers without conflict. |
| `noInterrupts()/interrupts()` + `sei()/cli()` | Yes — identical instructions. |

---

## What is Not Included

The following Arduino features are intentionally absent. Use the listed native
alternative for each.

| Feature | Alternative |
|---------|-------------|
| `tone()` / `noTone()` | `MikroDuino::Timer0` in CTC mode with ISR |
| `shiftIn()` / `shiftOut()` | `MikroDuino::SPI` or manual bit-bang |
| `attachInterrupt()` | `MikroDuino::Interrupt.attach()` |
| `Wire` (I2C object) | `MikroDuino::I2C` |
| `SPI` object | `MikroDuino::SPI` |
| `String` class | `char[]`, `<string.h>` (`strcmp`, `strcat`, etc.) |
| `PROGMEM` / `pgm_read_byte` | Use `<avr/pgmspace.h>` directly |
| `F("...")` flash string macro | `USART0.write_P(PSTR("..."))` |
| `Serial.readString()` | Manual loop with `available()` / `read()` |
| `Serial.parseInt()` | `strtol()` on a collected char buffer |
| `Serial1`, `Serial2` | `MikroDuino::USART1` |
| `analogWrite()` on pins 3/5/6/11 | `MikroDuino::Timer0` / `Timer2` registers |
| `EEPROM.read()` / `EEPROM.write()` | `<avr/eeprom.h>` directly |
| `millis()` without `beginTimekeeping()` | Always call `beginTimekeeping()` first |

---

## Implementation Details

### Timer0 configuration in `beginTimekeeping()`

The timer is put in **Fast PWM** mode (not Normal mode). This is intentional — it
matches the Arduino Uno's Timer0 configuration so that pins D5 (OC0B) and D6 (OC0A)
behave the same way they do on Arduino when `analogWrite()` is called on them (though
the compat layer does not implement those channels). The overflow rate is the same in
both Normal and Fast PWM at the same prescaler and TOP.

### millis() resolution vs accuracy

The compat `millis()` updates in steps of 1.024 ms (one Timer0 overflow), not 1 ms.
Between overflows, repeated calls to `millis()` return the same value. This matters
for tight timing loops:

```cpp
// This may spin extra iterations while millis() is stuck on the same value:
uint32_t start = millis();
while (millis() - start < 1) { }   // ← may loop for up to 1.024 ms

// For sub-millisecond precision use micros() instead:
uint32_t startUs = micros();
while (micros() - startUs < 500) { }   // 500 µs wait
```

The compat millis() is appropriate for event scheduling (every 500 ms, every 1 s)
but not for measuring durations shorter than ~2 ms.

### analogRead() re-initialisation overhead

Each call to `analogRead()` calls `ADC_Driver.begin()`, which writes three AVR
registers (ADMUX, ADCSRA, ADCSRB). This adds roughly 6 CPU cycles of overhead
before the conversion starts — negligible in normal use but worth knowing if you
are calling `analogRead()` at high frequency. For burst or free-running ADC reads,
use `MikroDuino::ADC_Driver` directly and call `begin()` once.

### analogWrite() Timer1 reconfiguration

Each `analogWrite()` call invokes `PWM1.begin(490)`, which writes TCCR1A, TCCR1B,
and ICR1 even when the timer is already configured. When both channels (D9 and D10)
are used, updating one via `analogWrite(9, v)` momentarily reconfigures the timer and
may cause a brief glitch on D10. For applications that need precise, stable dual-channel
PWM, use `MikroDuino::PWM1` directly and call `begin()` only once.

---

## Common Mistakes

**Calling `millis()` or `micros()` before `beginTimekeeping()`**
Returns 0 or a stale value. Timer0 is not counting until `beginTimekeeping()` is
called. Always call it early in `setup()`.

**Using `delay()` inside an ISR**
`delay()` busy-waits using `_delay_ms()`. It does not use Timer0, but since it runs
inside an ISR, it blocks the CPU entirely while other interrupts cannot be serviced
(the I-bit is cleared on ISR entry). The program hangs. Never call `delay()` or
`Serial.print()` inside an ISR.

**Missing `volatile` on ISR-shared variables**
The compiler does not know that an ISR can change a variable between two reads in
the main loop. Without `volatile`, it caches the value in a register and the main
loop never observes the ISR's writes.

**Using `Serial` and `MikroDuino::USART0` together**
`Serial` wraps `USART0`. Calling `USART0.begin()` after `Serial.begin()` reconfigures
the baud rate. Calling `USART0.write()` interleaves bytes with `Serial.print()` output.
Choose one API per project.

**Calling `analogRead()` after custom ADC setup**
`analogRead()` calls `ADC_Driver.begin()` with defaults. If you configured the ADC
with a custom reference or prescaler, that configuration is silently overwritten on
the first `analogRead()` call.

**`analogWrite()` on unsupported pins**
`analogWrite(3, value)` compiles without error but does nothing — pin 3 maps to OC2B
which is not wired in the compat layer. Check the supported-pins table before
expecting PWM output.

**Using `peek()` expecting non-destructive behaviour**
The compat `peek()` calls `read()` internally — it consumes the byte. Code that calls
`peek()` to inspect a byte and then `read()` to consume it will block on the second
call waiting for a second byte that may never arrive.

**Forgetting `ARDUINO_MAIN()` — or placing it inside a function**
Without `ARDUINO_MAIN()`, there is no `main()` and the linker fails. Placing it inside
a function (`void setup() { ARDUINO_MAIN(); }`) is a compile error because it expands
to a function definition at that point.

**String literal SRAM exhaustion**
Every `Serial.println("long string")` consumes SRAM. On ATmega328P (2 KB), a dozen
long string literals can overflow the heap and corrupt the stack. Symptoms are
unpredictable resets or corrupted output. Move strings to flash using
`USART0.write_P(PSTR("..."))`.

**Non-blocking pattern with addition instead of subtraction**
```cpp
// WRONG — breaks after millis() rolls over at ~49 days
if (millis() >= lastEvent + 500) { ... }

// CORRECT — subtraction handles rollover transparently
if (millis() - lastEvent >= 500) { ... }
```

---

## Quick Reference

### Setup

| Task | Call |
|------|------|
| Enable millis()/micros() | `Arduino::beginTimekeeping()` in setup() |
| Open serial at 115200 | `Serial.begin(115200)` |
| Seed RNG from noise | `randomSeed(analogRead(A3))` |
| Set pin as output | `pinMode(13, OUTPUT)` |
| Set pin as input w/ pull-up | `pinMode(2, INPUT_PULLUP)` |

### GPIO / Timing

| Task | Call |
|------|------|
| Drive pin HIGH / LOW | `digitalWrite(13, HIGH)` / `digitalWrite(13, LOW)` |
| Read pin | `int s = digitalRead(2)` |
| Blocking delay | `delay(500)` |
| Sub-ms delay | `delayMicroseconds(100)` |
| Milliseconds since start | `uint32_t t = millis()` |
| Microseconds since start | `uint32_t t = micros()` |
| Non-blocking interval | `if (millis() - last >= 500) { last = millis(); ... }` |

### Analog

| Task | Call |
|------|------|
| Read ADC (0–1023) | `int v = analogRead(A0)` |
| Scale to voltage | `float v = raw * (5.0f / 1023.0f)` |
| PWM output (0–255) | `analogWrite(9, 128)` |

### Serial

| Task | Call |
|------|------|
| Print string | `Serial.print("text")` / `Serial.println("text")` |
| Print integer | `Serial.println(42)` |
| Print float (4 dp) | `Serial.print(3.14f, 4)` |
| Print char | `Serial.print('A')` |
| Print hex / bin | `Serial.print(val, HEX)` / `Serial.print(val, BIN)` |
| Blank line | `Serial.println()` |
| Send raw byte | `Serial.write(0xFF)` |
| Check for incoming byte | `if (Serial.available()) { ... }` |
| Read one byte | `int c = Serial.read()` |
| Echo received byte | `Serial.write((uint8_t)c)` |
| Wait for TX to finish | `Serial.flush()` |

### Math / Utility

| Task | Call |
|------|------|
| Scale between ranges | `map(val, 0, 1023, 0, 100)` |
| Clamp to range | `constrain(val, 0, 100)` |
| Smaller of two | `min(a, b)` |
| Larger of two | `max(a, b)` |
| Absolute value | `abs(x)` |
| Square | `sq(x)` |
| Degrees → radians | `deg * DEG_TO_RAD` |
| Radians → degrees | `rad * RAD_TO_DEG` |
| Random in [0, n) | `random(n)` |
| Random in [lo, hi) | `random(lo, hi)` |
| Disable all interrupts | `noInterrupts()` |
| Re-enable interrupts | `interrupts()` |
| Measure pulse length | `uint32_t us = pulseIn(pin, HIGH, 30000)` |
