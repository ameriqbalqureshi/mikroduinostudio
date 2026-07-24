# MikroDuino PWM Reference

## Overview

The ATmega328P has **three independent hardware timers**, each capable of generating
PWM signals on dedicated output-compare pins. The MikroDuino SDK currently exposes
Timer1 (16-bit) through `PWM1Driver`, available as the global `PWM1`.

Include:
```cpp
#include <mikroduino/pwm.hpp>
// — or —
#include <mikroduino/mikroduino.hpp>   // entire SDK
using namespace MikroDuino;
```

---

## Hardware PWM Modes — ATmega328P

### What PWM modes exist in hardware?

The AVR hardware offers four distinct PWM generation strategies, each a different
waveform algorithm baked into the timer silicon.

---

#### 1. Fast PWM

The timer counts **up** from BOTTOM (0) to TOP, then immediately resets to 0.
The output pin is **set** at BOTTOM and **cleared** when the counter matches OCRnx.

```
Count:   0 ──────────────▶ TOP ──▶ 0 ──────▶ ...
Output:  ████████░░░░░░░░░░░      ████████...
               ↑ OCRnx
```

- Highest frequency for a given prescaler — one slope, no backswing.
- Output frequency: `F_CPU / (prescaler × (TOP + 1))`
- Small asymmetry at extreme duties: 0 % never goes fully low (one glitch pulse).
  Use `stopA()` / `stopB()` to force true-off instead.
- **Best for**: LED dimming, audio, power conversion, anything where frequency
  stability matters more than waveform symmetry.

---

#### 2. Phase-Correct PWM

The timer counts **up** from BOTTOM to TOP, then **back down** to BOTTOM (dual-slope).
The output pin is cleared on the way up when counter matches OCRnx, and set again
on the way down.

```
Count:   0 ───▶ TOP ───▶ 0 ───▶ TOP ...
Output:  ████░░░░░░░░████████░░░░░░░░████...
              ↑ OCRnx  ↑ OCRnx
```

- Output frequency: `F_CPU / (2 × prescaler × TOP)`  — roughly half of Fast PWM.
- The output pulse is perfectly centred in time — equal leading and trailing
  edges relative to the midpoint.
- **Best for**: motor control, servo PWM, anything sensitive to phase symmetry
  such as H-bridge drives where asymmetric pulses cause torque ripple.

---

#### 3. Phase and Frequency-Correct PWM *(Timer1 only)*

Identical waveform to Phase-Correct but OCRnx is only **updated at BOTTOM**, not
at the moment you write to the register. This prevents mid-cycle glitches when you
change duty on the fly.

- Output frequency: `F_CPU / (2 × prescaler × TOP)`
- **Best for**: motor drives where you update duty inside an ISR; eliminates the
  occasional rogue pulse that Phase-Correct can produce when OCRnx is written near
  the peak.

---

#### 4. CTC (Clear Timer on Compare)

Not technically PWM — the timer resets on compare match and toggles the OC pin.
Produces a **50 % duty cycle square wave** at an exact frequency. No duty control.

- Output frequency: `F_CPU / (2 × prescaler × (OCRnA + 1))`
- **Best for**: precise clock generation, bit-bang protocols, tone generation.

---

### Timer summary

| Timer | Width | Channels | PWM Modes available in hardware |
|-------|-------|----------|----------------------------------|
| Timer0 | 8-bit | OC0A (PD6), OC0B (PD5) | Fast PWM, Phase-Correct PWM |
| Timer1 | 16-bit | OC1A (PB1), OC1B (PB2) | Fast PWM, Phase-Correct PWM, Phase+Freq-Correct PWM |
| Timer2 | 8-bit | OC2A (PB3), OC2B (PD3) | Fast PWM, Phase-Correct PWM |

---

### PWM output pins (ATmega328P)

| SDK channel | AVR pin | Arduino Uno pin | Timer |
|-------------|---------|-----------------|-------|
| OC0A | PD6 | 6 | Timer0 A |
| OC0B | PD5 | 5 | Timer0 B |
| OC1A | PB1 | 9 | Timer1 A |
| OC1B | PB2 | 10 | Timer1 B |
| OC2A | PB3 | 11 | Timer2 A |
| OC2B | PD3 | 3 | Timer2 B |

> **Important**: each pin is hard-wired to its timer by the silicon. You cannot
> reroute OC1A to a different pin.

---

### Frequency vs resolution tradeoff

Higher frequency = smaller TOP = fewer steps between 0 % and 100 %.

**Timer1 (16-bit), Fast PWM, ATmega328P @ 16 MHz**

| Target frequency | Prescaler chosen | TOP (ICR1) | Resolution (steps) |
|-----------------|-----------------|------------|-------------------|
| 10 Hz | 1024 | 62499 | ~16-bit |
| 50 Hz | 1024 | 12499 | ~14-bit |
| 490 Hz | 64 | 510 | ~9-bit (Arduino default) |
| 1 000 Hz | 1 | 15999 | ~14-bit |
| 10 000 Hz | 1 | 1599 | ~10.6-bit |
| 25 000 Hz | 1 | 639 | ~9.3-bit |
| 100 000 Hz | 1 | 159 | ~7.2-bit |
| 1 000 000 Hz | 1 | 15 | 4-bit |

> `PWM1.begin()` automatically picks the smallest prescaler that keeps TOP ≤ 65535.
> You never set the prescaler manually.

**Timer0/2 (8-bit)** — TOP is always ≤ 255. Maximum useful resolution is 8-bit
regardless of frequency. Not yet exposed by the SDK driver.

---

## SDK Implementation Status

| Feature | Status | Driver |
|---------|--------|--------|
| Timer1 Fast PWM | **Implemented** | `PWM1` |
| Timer1 Phase-Correct PWM | **Implemented** | `PWM1` |
| Timer1 Phase+Freq-Correct PWM | Planned | — |
| Timer0 Fast / Phase-Correct PWM | Planned | — |
| Timer2 Fast / Phase-Correct PWM | Planned | — |
| Inverting output mode | Planned | — |

---

## PWM1Driver API Reference

`PWM1Driver` controls **Timer1** — the 16-bit timer. It is pre-instantiated as the
global `PWM1`. Never create your own instance.

---

### `begin(frequencyHz, type)`

```cpp
void begin(uint32_t frequencyHz, PWMType type = PWMType::FastPWM);
```

Configure Timer1 for PWM at the requested frequency and mode. Must be called before
any duty-cycle method.

| Parameter | Type | Description |
|-----------|------|-------------|
| `frequencyHz` | `uint32_t` | Target PWM frequency in Hz |
| `type` | `PWMType` | `PWMType::FastPWM` (default) or `PWMType::PhaseCorrect` |

The driver tries prescalers 1 → 8 → 64 → 256 → 1024 in order and picks the first
one that keeps ICR1 within 16 bits. Calling `begin()` again at runtime switches
frequency and/or mode instantly; any active duty values are reset.

```cpp
PWM1.begin(10000);                          // 10 kHz Fast PWM
PWM1.begin(50, PWMType::PhaseCorrect);      // 50 Hz Phase-Correct (servo)
```

---

### `dutyA(percent)` / `dutyB(percent)`

```cpp
void dutyA(uint8_t percent);
void dutyB(uint8_t percent);
```

Set duty cycle as a percentage (0–100). Automatically enables the output pin and
the compare output unit on first call.

```cpp
PWM1.dutyA(75);   // OC1A (PB1) at 75 %
PWM1.dutyB(25);   // OC1B (PB2) at 25 %
```

Both channels are fully independent and can run simultaneously.

---

### `rawA(value)` / `rawB(value)`

```cpp
void rawA(uint16_t value);
void rawB(uint16_t value);
```

Set duty cycle as a raw compare register value in the range `[0, top()]`.
Use this when you need the finest possible resolution or are mapping from a
sensor reading directly.

```cpp
uint16_t t = PWM1.top();
PWM1.rawA(t / 2);          // exact 50 %
PWM1.rawB(t * 3 / 4);      // exact 75 %
```

Writing a value greater than `top()` results in the output staying high
permanently (100 % duty) — not an error, but rarely intended.

---

### `top()`

```cpp
uint16_t top() const;
```

Return the current ICR1 value — the number the timer counts to. Use it to
scale external values (ADC readings, sensor data) to the full PWM range.

```cpp
uint16_t adcVal = /* 0–1023 */;
PWM1.rawA(adcVal * PWM1.top() / 1023);
```

`top()` is only meaningful after `begin()` has been called.

---

### `stopA()` / `stopB()`

```cpp
void stopA();
void stopB();
```

Disconnect the compare output unit and tri-state the pin (configured as input).
The timer keeps running — the other channel is unaffected. Calling `dutyA()` or
`rawA()` after `stopA()` re-enables the channel.

```cpp
PWM1.stopB();           // kill channel B, A still running
_delay_ms(1000);
PWM1.dutyB(50);         // restart B at 50 %
```

Use `stopA()` / `stopB()` instead of `dutyA(0)` when you need the pin to be
truly inactive (not driven low by the PWM hardware).

---

### Enumerations

```cpp
enum class PWMType : uint8_t {
    FastPWM,        // single-slope, higher frequency
    PhaseCorrect,   // dual-slope, centred pulse, lower frequency
};

enum class PWMChannel : uint8_t {
    OC0A = 0,   // Timer0 A — PD6
    OC0B = 1,   // Timer0 B — PD5
    OC1A = 2,   // Timer1 A — PB1
    OC1B = 3,   // Timer1 B — PB2
    OC2A = 4,   // Timer2 A — PB3
    OC2B = 5,   // Timer2 B — PD3
};
```

`PWMChannel` is defined for future drivers (Timer0, Timer2). `PWM1Driver` always
operates on OC1A / OC1B — the channel argument is implicit.

---

## Examples

### Smooth LED fade (maximum resolution)

```cpp
#include <mikroduino/pwm.hpp>
#include <util/delay.h>
using namespace MikroDuino;

int main() {
    PWM1.begin(10000);                  // 10 kHz Fast PWM, top = 1599

    const uint16_t top = PWM1.top();

    while (true) {
        for (uint16_t v = 0; v <= top; ++v) {
            PWM1.rawA(v);
            _delay_us(500);
        }
        for (uint16_t v = top; v != 0; --v) {
            PWM1.rawA(v);
            _delay_us(500);
        }
        PWM1.rawA(0);
    }
}
```

---

### Servo control (50 Hz Phase-Correct PWM)

Standard RC servos expect a 50 Hz signal with pulse width 1–2 ms (1 ms = 0°,
1.5 ms = 90°, 2 ms = 180°).

```cpp
#include <mikroduino/pwm.hpp>
using namespace MikroDuino;

// Map angle 0-180 to a raw OCR1A value
uint16_t servoRaw(uint8_t degrees) {
    // At 50 Hz Phase-Correct: top = F_CPU / (2 * 8 * 50) - 1 = 19999
    // 1 ms = top/20, 2 ms = top/10
    uint16_t t = PWM1.top();
    uint16_t minPulse = t / 20;         // 1 ms
    uint16_t maxPulse = t / 10;         // 2 ms
    return minPulse + (uint32_t)(maxPulse - minPulse) * degrees / 180;
}

int main() {
    PWM1.begin(50, PWMType::PhaseCorrect);

    while (true) {
        PWM1.rawA(servoRaw(0));         // 0°
        _delay_ms(1000);
        PWM1.rawA(servoRaw(90));        // 90°
        _delay_ms(1000);
        PWM1.rawA(servoRaw(180));       // 180°
        _delay_ms(1000);
    }
}
```

---

### Two channels at independent duties

```cpp
#include <mikroduino/pwm.hpp>
using namespace MikroDuino;

int main() {
    PWM1.begin(1000);       // 1 kHz

    PWM1.dutyA(75);         // OC1A — 75 %
    PWM1.dutyB(25);         // OC1B — 25 %

    while (true) {}
}
```

---

### ADC-controlled brightness

Scale a 10-bit ADC reading directly to the PWM range using `top()`.

```cpp
#include <mikroduino/pwm.hpp>
#include <mikroduino/adc.hpp>
using namespace MikroDuino;

int main() {
    PWM1.begin(10000);

    while (true) {
        uint16_t adc = ADC::read(0);                        // 0–1023
        PWM1.rawA(adc * PWM1.top() / 1023);
    }
}
```

---

## Notes

**`begin()` resets the duty.**
Calling `begin()` a second time reconfigures ICR1 and TCCR1x. Any previously set
OCR1A/OCR1B value may no longer be valid for the new TOP — call `dutyA()` / `rawA()`
again immediately after `begin()`.

**Timer1 conflicts.**
`PWM1Driver` owns Timer1 entirely. Do not use `Timer1` (from `timer.hpp`) at the
same time — they share TCCR1A, TCCR1B, and ICR1.

**`dutyA(0)` is not fully off.**
In Fast PWM mode the output produces one narrow spike per period even at 0 %.
Use `stopA()` to tri-state the pin when you need true zero output.

**Phase-Correct frequency is halved.**
Because the counter runs up and back down, the output frequency at a given TOP is
`F_CPU / (2 × prescaler × TOP)` — roughly half that of Fast PWM with the same
settings. `begin()` accounts for this automatically, so `begin(50, PhaseCorrect)`
gives you 50 Hz regardless.

**No instance needed.**
`PWM1` is a `static` global defined in `pwm.hpp`. Do not declare your own
`PWM1Driver` — it would create a second object that shares the hardware registers
with the first.

---

## Quick Reference

| Task | Call |
|------|------|
| Start Fast PWM at 10 kHz | `PWM1.begin(10000)` |
| Start Phase-Correct PWM at 50 Hz | `PWM1.begin(50, PWMType::PhaseCorrect)` |
| Set channel A to 75 % | `PWM1.dutyA(75)` |
| Set channel B to 25 % | `PWM1.dutyB(25)` |
| Set channel A via raw value | `PWM1.rawA(value)` |
| Get TOP (ICR1) value | `uint16_t t = PWM1.top()` |
| Stop channel A (tri-state pin) | `PWM1.stopA()` |
| Stop channel B (tri-state pin) | `PWM1.stopB()` |
