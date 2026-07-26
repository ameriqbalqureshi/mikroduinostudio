# MikroDuino Timer Reference

## Overview

The ATmega family includes dedicated **hardware timer/counter peripherals** that count
independently of the CPU. The MikroDuino SDK exposes them through driver classes —
which ones exist depends on the target MCU (`MD_TIMER_COUNT`):

| Driver | Timer | Width | Global instance | Available on |
|--------|-------|-------|-----------------|--------------|
| `Timer0Driver` | Timer0 | 8-bit | `Timer0` | All supported MCUs |
| `Timer1Driver` | Timer1 | 16-bit | `Timer1` | All supported MCUs |
| `Timer2Driver` | Timer2 | 8-bit, async-capable | `Timer2` | All supported MCUs |
| `Timer3Driver` | Timer3 | 16-bit | `Timer3` | ATmega64, ATmega128, ATmega2560 |
| `Timer4Driver` | Timer4 | 16-bit | `Timer4` | ATmega2560 |
| `Timer5Driver` | Timer5 | 16-bit | `Timer5` | ATmega2560 |

`Timer3Driver`/`Timer4Driver`/`Timer5Driver` are the same template as `Timer1Driver`
(`Timer16Driver<N>`, in `timer.hpp`) — identical API, just wired to a different set of
hardware registers. Everything documented below for `Timer1` applies equally to
`Timer3`/`Timer4`/`Timer5` with the instance name swapped.

Include:
```cpp
#include <mikroduino/timer.hpp>
// — or —
#include <mikroduino/mikroduino.hpp>   // entire SDK
using namespace MikroDuino;
```

No `.cpp` file needs to be added for timers — the drivers are header-only. You do need
`#include <avr/interrupt.h>` if you use ISR-based callbacks.

---

## How AVR Hardware Timers Work

### The counter

Each timer is, at its core, a free-running **up-counter** driven by a clock signal.
The counter register increments once per clock tick and wraps back to zero when it
reaches its maximum value (255 for 8-bit, 65535 for 16-bit).

```
Clock ticks:  ┌─┐ ┌─┐ ┌─┐ ┌─┐
              │ │ │ │ │ │ │ │
Counter:  0 ──▶ 1 ──▶ 2 ──▶ 3 ──▶ ...
```

The CPU can read or write the counter at any time (`TCNT0` / `TCNT1` registers).

### The prescaler

The **prescaler** divides the 16 MHz system clock before feeding it to the counter.
A larger divisor means the counter increments more slowly — lower resolution but longer
range.

```
              ┌────────────┐     ┌─────────┐
16 MHz ──────▶│  Prescaler  │────▶│ Counter │──▶ counts slowly
              │  ÷ 1024    │     └─────────┘
              └────────────┘
                  15 625 Hz
```

At 16 MHz with DIV1024, Timer1 takes **4.19 seconds** to count to 65535 — that is the
whole usable range of the 16-bit counter with no CPU involvement.

### Compare registers

Each timer has one or two **Output Compare Registers** (OCRnA, OCRnB). The hardware
continuously compares the counter against these values. When they match:

- An **interrupt flag** is set in TIFR (you can poll this).
- If the matching interrupt is enabled in TIMSK and `sei()` is active, the CPU calls
  the corresponding ISR.
- In CTC mode, the counter resets to 0.
- In PWM mode, the output pin toggles.

### Overflow

When the counter wraps past its maximum (255 or 65535), an **overflow flag** (`TOV`)
is set in TIFR. If `TOIE` in TIMSK is set, the overflow ISR fires. In Normal mode this
gives you a periodic interrupt at a rate determined entirely by the prescaler.

---

## Hardware Reference

### Timer summary (ATmega328P)

| Timer | Width | Count reg | Compare regs | Overflow max | ISR vectors |
|-------|-------|-----------|--------------|--------------|-------------|
| Timer0 | 8-bit | `TCNT0` | `OCR0A`, `OCR0B` | 255 | `TIMER0_COMPA_vect`, `TIMER0_COMPB_vect`, `TIMER0_OVF_vect` |
| Timer1 | 16-bit | `TCNT1` | `OCR1A`, `OCR1B`, `ICR1` | 65535 | `TIMER1_COMPA_vect`, `TIMER1_COMPB_vect`, `TIMER1_OVF_vect`, `TIMER1_CAPT_vect` |
| Timer2 | 8-bit async | `TCNT2` | `OCR2A`, `OCR2B` | 255 | `TIMER2_COMPA_vect`, `TIMER2_COMPB_vect`, `TIMER2_OVF_vect` |

### Timer summary (ATmega64 / ATmega128 / ATmega2560)

Same shape as above, plus Timer3 (ATmega64/128/2560) and Timer4/Timer5 (ATmega2560
only) — each a full 16-bit timer identical to Timer1 with `OCRnA`/`OCRnB`/`ICRn`,
`TIMERn_COMPA_vect`/`TIMERn_COMPB_vect`/`TIMERn_OVF_vect`/`TIMERn_CAPT_vect`.

> `Timer1Driver.compareC`/`OCR1C` (and the Timer3/4/5 equivalents) exist in hardware
> on these MCUs but are **not exposed** by the SDK — only compare A/B are wired up,
> same as Timer1 on ATmega328P.

### Prescaler reference (ATmega328P @ 16 MHz)

| `TimerPrescaler` | Timer clock | Tick period | Timer0 OVF period | Timer1 OVF period |
|------------------|------------|-------------|-------------------|-------------------|
| `DIV1` | 16 MHz | 62.5 ns | 16 µs | 4.096 ms |
| `DIV8` | 2 MHz | 500 ns | 128 µs | 32.77 ms |
| `DIV64` | 250 kHz | 4 µs | 1.024 ms | 262.1 ms |
| `DIV256` | 62.5 kHz | 16 µs | 4.096 ms | 1.049 s |
| `DIV1024` | 15.625 kHz | 64 µs | 16.38 ms | 4.194 s |
| `ExtFall` | external ↓ | — | — | — |
| `ExtRise` | external ↑ | — | — | — |

OVF periods listed are for **Normal mode** (full 8-bit or 16-bit count before wrap).
CTC mode resets at OCRnA, so the effective period is shorter.

`DIV32` and `DIV128` are **Timer2-only** taps (its prescaler has two extra steps
that Timer0/1/3/4/5 don't have in hardware); passing them to any other timer's
`prescaler()` is treated as `Off`.

### ISR vector names

| Event | Timer0 | Timer1 |
|-------|--------|--------|
| Compare A match | `TIMER0_COMPA_vect` | `TIMER1_COMPA_vect` |
| Compare B match | `TIMER0_COMPB_vect` | `TIMER1_COMPB_vect` |
| Overflow | `TIMER0_OVF_vect` | `TIMER1_OVF_vect` |
| Input Capture | — | `TIMER1_CAPT_vect` |

---

## Timer Modes

### Normal

The counter counts freely from 0 to TOP (255 or 65535) and wraps back to 0.
No automatic reset, no output pin behaviour.

```
Count:  0 ──────────────────▶ 255/65535 ──▶ 0 ──────────────────▶ ...
                                               ↑ overflow flag set here
```

Use Normal mode when you need:
- Elapsed time measurement (`reset()` → do work → `count()`)
- Long-interval timing via overflow flag polling
- A free-running counter to timestamp events

---

### CTC — Clear Timer on Compare

The counter counts from 0 to **OCRnA**, then resets to 0 at the next clock tick.
The compare-A flag fires on every match. This gives a **precise, adjustable period**
with no overflow arithmetic.

```
Count:  0 ──────────▶ OCRnA
                          │ counter resets + OCF1A set
                          ▼
        0 ──────────▶ OCRnA ...
```

Period formula:
```
T = (OCRnA + 1) × prescaler / F_CPU
Hz = F_CPU / (prescaler × (OCRnA + 1))
```

The `ticksForHz()` helper on Timer1 inverts this for you:
```
OCRnA = F_CPU / (prescaler × Hz) - 1
```

CTC is the go-to mode for:
- ISR-driven system ticks (1 ms millis, 10 ms schedulers)
- Periodic sensor reads
- Tone generation (toggle output pin in ISR)

In CTC mode, compareB (`OCR1B`) also generates events and interrupts within the period.
Because the counter resets at OCR1A, compareB must be set to a value **less than** OCR1A
to fire — set it to `OCR1A / 2` to fire twice per period.

---

### Fast PWM

The counter counts from 0 to TOP, then immediately resets. An output pin is driven
high at 0 and cleared when the counter reaches OCRnx. Used by `PWM1Driver` —
see the [PWM reference](pwm.md) for duty-cycle control.

```
Count:  0 ─────────────────▶ TOP ──▶ 0 ...
Output: ████████████░░░░░░░░░         ███...
                    ↑ OCRnx
```

You can configure this mode via `Timer0` / `Timer1` directly, but the `PWM1Driver`
provides a higher-level API with percent duty. Do not mix `Timer1` and `PWM1` on the
same timer — they share the hardware registers.

---

### Phase-Correct PWM

The counter counts **up** to TOP then **back down** to 0 (dual-slope). The output pin
is cleared on the way up and set on the way down when the counter matches OCRnx.
This produces a symmetrical waveform at half the frequency of Fast PWM for the same
TOP and prescaler. Also handled by `PWM1Driver`.

```
Count:  0 ─▶ TOP ─▶ 0 ─▶ TOP ...
Output: ██░░░░░░░░██████░░░░░░░░███...
           ↑ OCRnx  ↑ OCRnx
```

---

## SDK Implementation Status

| Feature | Status |
|---------|--------|
| Timer0 Normal, CTC, FastPWM, PhaseCorrectPWM | Implemented (`Timer0`) |
| Timer1/3/4/5 Normal, CTC, FastPWM, PhaseCorrectPWM | Implemented (`Timer1`, and `Timer3`/`Timer4`/`Timer5` where present) |
| Timer2 Normal, CTC, FastPWM, PhaseCorrectPWM, async clocking | Implemented (`Timer2`) |
| Timer1/3/4/5 input capture | Registers accessible; no high-level API |
| Timer1/3/4/5 compare C (`OCRnC`) | Not exposed — only compare A/B |
| Output compare pin output (OC0A, OC1B, etc.) | Available via PWM driver; not wired in timer driver |

---

## Timer0Driver API Reference

`Timer0Driver` controls the 8-bit Timer0. It is pre-instantiated as the global `Timer0`.

---

### `mode(m)`

```cpp
void mode(TimerMode m);
```

Set the operating mode. Must be called before `start()`.

| `TimerMode` value | Behaviour |
|-------------------|-----------|
| `TimerMode::Normal` | Free-running counter, wraps at 255 |
| `TimerMode::CTC` | Resets at OCR0A on compare match |
| `TimerMode::FastPWM` | Single-slope PWM, TOP = 255 |
| `TimerMode::PhaseCorrectPWM` | Dual-slope PWM, TOP = 255 |

---

### `prescaler(p)`

```cpp
void prescaler(TimerPrescaler p);
```

Set the clock source and divisor. Must be called before `start()`.

---

### `compareA(val)` / `compareB(val)`

```cpp
void compareA(uint8_t val);
void compareB(uint8_t val);
```

Write to `OCR0A` / `OCR0B`. In CTC mode, `compareA` sets the period TOP. In PWM mode,
both set the duty cycle. Values above 255 are not accepted (parameter is `uint8_t`).

---

### `start()` / `stop()`

```cpp
void start();
void stop();
```

`start()` writes TCCR0A and TCCR0B from the current mode and prescaler settings.
`stop()` clears the CS (clock select) bits — the counter freezes but its value is
preserved. Call `reset()` before `start()` if you want a clean count from zero.

---

### `reset()`

```cpp
void reset();
```

Writes 0 to `TCNT0`. Safe to call while the timer is running or stopped.

---

### `count()`

```cpp
uint8_t count() const;
```

Read the current value of `TCNT0`.

---

### `enableInterruptA()` / `disableInterruptA()`

```cpp
void enableInterruptA();
void disableInterruptA();
```

Set or clear the `OCIE0A` bit in `TIMSK0`. When set, a compare-A match fires
`ISR(TIMER0_COMPA_vect)`.

---

### `enableOverflow()` / `disableOverflow()`

```cpp
void enableOverflow();
void disableOverflow();
```

Set or clear `TOIE0` in `TIMSK0`. When set, a counter overflow fires
`ISR(TIMER0_OVF_vect)`.

---

## Timer1Driver API Reference

`Timer1Driver` controls the 16-bit Timer1. It is pre-instantiated as the global `Timer1`.

All methods mirror `Timer0Driver` but use 16-bit registers and add several Timer1-specific
features: compareB interrupt, overflow interrupt, input capture, flag read/clear, and
`ticksForHz()`.

---

### `mode(m)` / `prescaler(p)`

Same semantics as Timer0. Applies to TCCR1A and TCCR1B on `start()`.

---

### `compareA(val)` / `compareB(val)`

```cpp
void compareA(uint16_t val);
void compareB(uint16_t val);
```

Write to `OCR1A` / `OCR1B`. In CTC mode, `OCR1A` is the period TOP. `OCR1B` fires an
event partway through the period without resetting the counter — set it to any value
less than `OCR1A`.

```cpp
Timer1.compareA(31249);    // CTC TOP — 2 Hz at DIV256
Timer1.compareB(15624);    // fires at half-period — 4 Hz
```

---

### `inputCapture(val)`

```cpp
void inputCapture(uint16_t val);
```

Write to `ICR1`. In FastPWM and PhaseCorrectPWM modes the hardware uses ICR1 as TOP
instead of OCR1A, giving full 16-bit PWM duty resolution (OCR1A/B become the duty).
Not used in Normal or CTC mode.

---

### `start()` / `stop()` / `reset()` / `count()`

```cpp
void start();
void stop();
void reset();
uint16_t count() const;
```

Same semantics as Timer0 but operate on TCCR1x and TCNT1 (16-bit).

`count()` reads TCNT1 as a 16-bit value. On AVR, 16-bit timer reads are atomic when
interrupts are disabled — the hardware locks the high byte in a temporary register
during the low-byte read. The SDK does not disable interrupts around `count()`, so
if you share the count across an ISR boundary read it inside an `ATOMIC_BLOCK`.

---

### `ticksForHz(hz)`

```cpp
uint16_t ticksForHz(uint32_t hz) const;
```

Compute the `OCR1A` value needed for CTC mode to fire at `hz` Hz, given the **currently
configured prescaler**. Always call `prescaler()` before `ticksForHz()`.

```
OCR1A = F_CPU / (prescaler × hz) - 1
```

```cpp
Timer1.prescaler(TimerPrescaler::DIV256);
Timer1.compareA(Timer1.ticksForHz(2));   // 31249 at 16 MHz
```

Returns `0xFFFF` if `prescaler` is `Off` (division by zero guard). Does not check
whether the result fits in a `uint16_t` — if the frequency is too low for the chosen
prescaler, the value wraps. Choose a larger prescaler for very low frequencies.

**Frequency limits at 16 MHz:**

| Prescaler | Min Hz (fits in 16 bits) | Max Hz |
|-----------|--------------------------|--------|
| DIV1 | 245 Hz | 8 000 000 Hz |
| DIV8 | 31 Hz | 1 000 000 Hz |
| DIV64 | 4 Hz | 125 000 Hz |
| DIV256 | 1 Hz | 31 250 Hz |
| DIV1024 | 1 Hz | 7 813 Hz |

---

### `prescalerValue()`

```cpp
uint32_t prescalerValue() const;
```

Return the integer divisor for the current prescaler (1, 8, 64, 256, or 1024).
Useful for computing tick durations:

```cpp
// 1 tick duration in nanoseconds:
uint32_t ns_per_tick = Timer1.prescalerValue() * 1000UL / (F_CPU / 1000000UL);
```

---

### Interrupt enable/disable

```cpp
void enableInterruptA();    void disableInterruptA();
void enableInterruptB();    void disableInterruptB();
void enableOverflow();      void disableOverflow();
void enableCapture();       void disableCapture();
```

Set or clear the corresponding bit in `TIMSK1`.

| Method | `TIMSK1` bit | ISR vector |
|--------|-------------|------------|
| `enableInterruptA()` | `OCIE1A` | `TIMER1_COMPA_vect` |
| `enableInterruptB()` | `OCIE1B` | `TIMER1_COMPB_vect` |
| `enableOverflow()` | `TOIE1` | `TIMER1_OVF_vect` |
| `enableCapture()` | `ICIE1` | `TIMER1_CAPT_vect` |

> Enabling an interrupt in TIMSK1 alone does not cause a call — the **global I-bit**
> must also be set via `sei()`.

---

### Flag polling

```cpp
bool overflowFlag()  const;
bool compareAFlag()  const;
void clearOverflowFlag();
void clearCompareAFlag();
```

The hardware sets these flags in `TIFR1` regardless of whether the matching interrupt
is enabled. You can poll them in a loop without an ISR:

```cpp
while (!Timer1.compareAFlag());
Timer1.clearCompareAFlag();
// exactly one compare period has elapsed
```

On AVR, writing a **1** to a flag bit in TIFR **clears** it (write-1-to-clear). The
SDK handles this correctly in `clearOverflowFlag()` and `clearCompareAFlag()`.

> Only compareA and overflow flags are exposed for polling. compareB interrupts must
> use an ISR (`TIMER1_COMPB_vect`); there is no `compareBFlag()` method in the current
> SDK.

---

## Using Timer ISRs

### Declaring an ISR

```cpp
#include <avr/interrupt.h>

ISR(TIMER1_COMPA_vect) {
    // runs when Timer1 counter matches OCR1A
}
```

The ISR macro expands to the correct AVR interrupt vector function with the right
attributes. You must not call `sei()` inside an ISR or nest calls — the AVR hardware
clears the I-bit on entry and restores it on exit.

### Global interrupt enable

ISRs do not fire until `sei()` is called. The standard pattern is:

```cpp
// configure everything first
Timer1.mode(TimerMode::CTC);
Timer1.prescaler(TimerPrescaler::DIV256);
Timer1.compareA(Timer1.ticksForHz(100));
Timer1.enableInterruptA();
Timer1.start();

// then open the gate
sei();
```

### `volatile` for shared variables

Any variable written by an ISR and read by the main loop must be `volatile`:

```cpp
volatile uint32_t g_ticks = 0;

ISR(TIMER0_COMPA_vect) {
    ++g_ticks;      // ISR writes
}

int main() {
    // main reads — without volatile the compiler may cache the value
    if (g_ticks > 1000) { ... }
}
```

### Atomic reads of multi-byte values

A 16-bit or 32-bit variable shared with an ISR can be corrupted if the ISR fires
between the two byte reads. Wrap main-loop reads in `ATOMIC_BLOCK`:

```cpp
#include <util/atomic.h>

volatile uint32_t g_ms = 0;

uint32_t millis() {
    uint32_t t;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        t = g_ms;
    }
    return t;
}
```

`uint8_t` and `bool` are single-byte and are inherently atomic on AVR.

---

## Examples

### 1 — System millisecond ticker (Timer0 CTC)

**16 MHz / (64 × 250) = 1000 Hz → OCR0A = 249**

```cpp
#include <avr/io.h>
#include <avr/interrupt.h>
#include <mikroduino/timer.hpp>
using namespace MikroDuino;

volatile uint32_t g_ms = 0;

ISR(TIMER0_COMPA_vect) {
    ++g_ms;
}

uint32_t millis() {
    uint32_t t;
    cli(); t = g_ms; sei();
    return t;
}

void waitMs(uint32_t ms) {
    uint32_t end = millis() + ms;
    while (millis() < end);
}

int main() {
    Timer0.mode(TimerMode::CTC);
    Timer0.prescaler(TimerPrescaler::DIV64);
    Timer0.compareA(249);          // F_CPU / (64 * 1000) - 1
    Timer0.enableInterruptA();
    Timer0.start();
    sei();

    while (true) {
        // millis() is now available throughout the program
        waitMs(500);
        // toggle something every 500 ms
    }
}
```

---

### 2 — Periodic ISR at arbitrary frequency (Timer1 CTC + `ticksForHz`)

```cpp
#include <avr/io.h>
#include <avr/interrupt.h>
#include <mikroduino/timer.hpp>
#include <mikroduino/gpio.hpp>
using namespace MikroDuino;

static constexpr uint8_t LED = PB5;

ISR(TIMER1_COMPA_vect) {
    GPIO::toggle(LED);      // 2 Hz compare → 2 Hz toggle → 1 Hz visible blink
}

int main() {
    GPIO::output(LED);

    Timer1.mode(TimerMode::CTC);
    Timer1.prescaler(TimerPrescaler::DIV256);

    // ticksForHz() computes OCR1A from the current prescaler and F_CPU
    Timer1.compareA(Timer1.ticksForHz(2));   // 31 249 ticks @ DIV256

    Timer1.enableInterruptA();
    Timer1.start();
    sei();

    while (true) {}
}
```

---

### 3 — Dual compare A + B (two rates from one timer)

CompareA fires at 2 Hz and resets the counter. CompareB is set to half the OCR1A value
so it fires twice per period — effectively at 4 Hz — without affecting the counter.

```cpp
#include <avr/io.h>
#include <avr/interrupt.h>
#include <mikroduino/timer.hpp>
#include <mikroduino/gpio.hpp>
using namespace MikroDuino;

static constexpr uint8_t LED_HB  = PB5;   // 2 Hz heartbeat
static constexpr uint8_t LED_FAST = PD6;  // 4 Hz fast blink

ISR(TIMER1_COMPA_vect) { GPIO::toggle(LED_HB); }
ISR(TIMER1_COMPB_vect) { GPIO::toggle(LED_FAST); }

int main() {
    GPIO::output(LED_HB);
    GPIO::output(LED_FAST);

    Timer1.mode(TimerMode::CTC);
    Timer1.prescaler(TimerPrescaler::DIV256);

    uint16_t topA = Timer1.ticksForHz(2);   // 31 249
    Timer1.compareA(topA);
    Timer1.compareB(topA / 2);              // 15 624 — fires at half-period

    Timer1.enableInterruptA();
    Timer1.enableInterruptB();
    Timer1.start();
    sei();

    while (true) {}
}
```

---

### 4 — Elapsed time measurement (Timer1 Normal + `count()`)

With DIV64 at 16 MHz, each tick is 4 µs. Timer1 can measure up to 65535 × 4 µs = 262 ms
before wrapping.

```cpp
#include <avr/io.h>
#include <mikroduino/timer.hpp>
using namespace MikroDuino;

uint32_t measureUs() {
    Timer1.mode(TimerMode::Normal);
    Timer1.prescaler(TimerPrescaler::DIV64);    // 1 tick = 4 us
    Timer1.reset();
    Timer1.start();

    // ... operation to measure ...
    volatile uint32_t dummy = 0;
    for (uint32_t i = 0; i < 10000; ++i) ++dummy;

    Timer1.stop();
    return static_cast<uint32_t>(Timer1.count()) * 4;   // ticks → us
}

int main() {
    uint32_t us = measureUs();
    // us now holds the measured duration
    while (true) {}
}
```

For durations longer than 262 ms, use DIV1024 (64 µs per tick, 4.19 s max) or
combine the overflow counter with `count()`.

---

### 5 — Compare flag polling without an ISR (Timer1 CTC)

Flag polling is useful when you want a precise delay but cannot use an ISR
(e.g., inside a critical section, or when you want the delay to block explicitly).

```cpp
#include <avr/io.h>
#include <mikroduino/timer.hpp>
#include <mikroduino/gpio.hpp>
using namespace MikroDuino;

// Blocking delay of exactly N CTC periods
void waitPeriods(uint8_t n) {
    while (n--) {
        while (!Timer1.compareAFlag());
        Timer1.clearCompareAFlag();
    }
}

int main() {
    Timer1.mode(TimerMode::CTC);
    Timer1.prescaler(TimerPrescaler::DIV1024);
    Timer1.compareA(Timer1.ticksForHz(10));    // 10 Hz → 100 ms per period
    Timer1.start();

    GPIO::output(PB5);
    while (true) {
        waitPeriods(5);            // 500 ms on
        GPIO::toggle(PB5);
        waitPeriods(5);            // 500 ms off
        GPIO::toggle(PB5);
    }
}
```

---

### 6 — Long-interval timing via overflow polling (Timer1 Normal)

With DIV1024, Timer1 overflows approximately every 4.194 seconds. Count overflows
to measure multi-second intervals without an ISR.

```cpp
#include <avr/io.h>
#include <mikroduino/timer.hpp>
using namespace MikroDuino;

// Wait for 'ov' full overflows (~4.19 s each at 16 MHz / DIV1024)
void waitOverflows(uint8_t ov) {
    Timer1.mode(TimerMode::Normal);
    Timer1.prescaler(TimerPrescaler::DIV1024);
    Timer1.reset();
    Timer1.start();
    while (ov--) {
        while (!Timer1.overflowFlag());
        Timer1.clearOverflowFlag();
    }
    Timer1.stop();
}

int main() {
    // Do something every ~8.4 seconds (2 overflows)
    while (true) {
        waitOverflows(2);
        // timed action here
    }
}
```

For sub-overflow precision, add `Timer1.count()` after the last overflow and convert:
```cpp
uint32_t extra_us = static_cast<uint32_t>(Timer1.count()) * 64;   // 64 us/tick @ DIV1024
```

---

## Timer Conflicts

### Timer1 and `PWM1Driver`

`PWM1Driver` (in `pwm.hpp`) uses Timer1 internally. It configures TCCR1A, TCCR1B,
ICR1, OCR1A, and OCR1B. **Do not use `Timer1` at the same time as `PWM1`** — they
will overwrite each other's register settings.

If you need both PWM output and a periodic ISR, choose a different timer for the ISR:
Timer0 for the tick (see Example 1), and `PWM1` for the PWM signal.

### Timer0 and `_delay_ms()`

`_delay_ms()` from `<util/delay.h>` is a pure busy-loop based on `F_CPU` — it does
not use any hardware timer. Using Timer0 for your own purposes does **not** break
`_delay_ms()`. The two coexist safely.

### Two drivers, one peripheral

`Timer0` and `Timer1` are `static` globals defined in `timer.hpp`. Their members store
only the mode and prescaler — the actual hardware registers (`TCCRx`, `OCRx`, `TCNTx`,
`TIMSKx`, `TIFRx`) are the shared, physical peripheral. Calling `Timer1.start()` a
second time with different settings overwrites the registers set by the first call.
This is by design — each `start()` is a complete, self-contained reconfiguration.

---

## Common Mistakes

**Forgetting `sei()`**
`enableInterruptA()` sets `OCIE1A` in TIMSK1, but the global I-bit in SREG must also
be set before any ISR fires. If your ISR never runs, check that `sei()` is called after
`start()`.

**Calling `ticksForHz()` before `prescaler()`**
`ticksForHz()` reads the internally stored prescaler value. If you call it before
`prescaler()`, it uses the default (`DIV1`), which gives the wrong OCR value.
Always set the prescaler first.

**Choosing a prescaler that overflows `uint16_t`**
`ticksForHz()` returns `uint16_t`. With DIV1 at 16 MHz and `hz = 1`, the result is
15 999 999 — which truncates. Use DIV256 or DIV1024 for frequencies below ~245 Hz.

**Polling `overflowFlag()` without clearing it**
The overflow flag stays set until you explicitly clear it. If you do not call
`clearOverflowFlag()`, the polling loop exits immediately on every subsequent check —
it looks like overflows happen continuously with no delay.

**Not resetting before measuring**
`count()` reads `TCNT1`, which starts at whatever value it held last time. Call
`reset()` before `start()` when measuring elapsed time, or the baseline is unknown.

**Sharing `Timer1` with `PWM1`**
Both operate on the same silicon. Starting `Timer1` after `PWM1.begin()` overwrites
TCCR1A/TCCR1B and breaks the PWM output. Keep each timer dedicated to one driver.

**Missing `volatile` on ISR-shared variables**
Without `volatile`, the compiler may keep a variable in a CPU register and the main
loop never sees the ISR's update. This is the most common cause of ISR-based code that
"never fires" even when the hardware is working correctly.

**Calling `_delay_ms()` or `USART::write()` inside an ISR**
Both require either other interrupts to fire (UART TX-empty interrupt) or simply burn
CPU time. Inside an ISR, global interrupts are disabled. Blocking calls deadlock.
Set a flag in the ISR and do the real work in the main loop.

---

## Register Map (ATmega328P)

The SDK manages these registers. This table is for cross-referencing with the datasheet.

| Register | Purpose |
|----------|---------|
| `TCCR0A` | Timer0 control A — WGM bits (mode), COM bits (output compare pin) |
| `TCCR0B` | Timer0 control B — WGM bit, CS bits (prescaler) |
| `TCNT0` | Timer0 counter value |
| `OCR0A` | Timer0 compare A value |
| `OCR0B` | Timer0 compare B value |
| `TIMSK0` | Timer0 interrupt mask (`TOIE0`, `OCIE0A`, `OCIE0B`) |
| `TIFR0` | Timer0 interrupt flags (`TOV0`, `OCF0A`, `OCF0B`) |
| `TCCR1A` | Timer1 control A — WGM bits, COM bits |
| `TCCR1B` | Timer1 control B — WGM bits, ICES (capture edge), CS bits |
| `TCNT1` | Timer1 counter (16-bit, split across `TCNT1H:TCNT1L`) |
| `OCR1A` | Timer1 compare A (16-bit) |
| `OCR1B` | Timer1 compare B (16-bit) |
| `ICR1` | Timer1 input capture / PWM TOP |
| `TIMSK1` | Timer1 interrupt mask (`TOIE1`, `OCIE1A`, `OCIE1B`, `ICIE1`) |
| `TIFR1` | Timer1 interrupt flags (`TOV1`, `OCF1A`, `OCF1B`) |

WGM (Waveform Generation Mode) bits are split across TCCRnA and TCCRnB — the SDK
combines them correctly in `start()`.

---

## Quick Reference

| Task | Call |
|------|------|
| Set Timer0 to CTC mode | `Timer0.mode(TimerMode::CTC)` |
| Set Timer1 to Normal mode | `Timer1.mode(TimerMode::Normal)` |
| Set prescaler to DIV64 | `Timer0.prescaler(TimerPrescaler::DIV64)` |
| Compute OCR1A for 50 Hz (after setting prescaler) | `Timer1.compareA(Timer1.ticksForHz(50))` |
| Set Timer0 compare A to 249 | `Timer0.compareA(249)` |
| Set Timer1 compare B to half-period | `Timer1.compareB(Timer1.ticksForHz(2) / 2)` |
| Start the timer | `Timer0.start()` / `Timer1.start()` |
| Stop the timer (counter freezes) | `Timer0.stop()` / `Timer1.stop()` |
| Reset counter to 0 | `Timer0.reset()` / `Timer1.reset()` |
| Read counter | `uint8_t v = Timer0.count()` / `uint16_t v = Timer1.count()` |
| Enable compare-A ISR | `Timer1.enableInterruptA()` then `sei()` |
| Enable compare-B ISR | `Timer1.enableInterruptB()` then `sei()` |
| Enable overflow ISR | `Timer1.enableOverflow()` then `sei()` |
| Poll compare-A flag (no ISR) | `while (!Timer1.compareAFlag()); Timer1.clearCompareAFlag();` |
| Poll overflow flag (no ISR) | `while (!Timer1.overflowFlag()); Timer1.clearOverflowFlag();` |
| Declare ISR for Timer1 compare A | `ISR(TIMER1_COMPA_vect) { ... }` |
| Shared variable with ISR | `volatile uint32_t g_ms = 0;` |
| Atomic read of 32-bit variable | `ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { t = g_ms; }` |
