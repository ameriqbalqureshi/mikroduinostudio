# ADC — MikroDuino SDK Reference

**Header:** `sdk/core/avr/include/mikroduino/adc.hpp`  
**Source:** `sdk/core/avr/src/adc.cpp`  
**Namespace:** `MikroDuino`  
**Global instance:** `ADC_Driver`

---

## Table of Contents

1. [Hardware Overview](#1-hardware-overview)
2. [Internal Architecture](#2-internal-architecture)
3. [Register Map](#3-register-map)
4. [Enumerations](#4-enumerations)
5. [Global Instance](#5-global-instance)
6. [API Reference](#6-api-reference)
7. [Prescaler Selection](#7-prescaler-selection)
8. [Conversion Timing](#8-conversion-timing)
9. [Usage Patterns](#9-usage-patterns)
10. [MCU Differences](#10-mcu-differences)
11. [Common Pitfalls](#11-common-pitfalls)

---

## 1. Hardware Overview

The AVR ADC (Analog-to-Digital Converter) converts a continuous analog voltage into a discrete digital value. It is a 10-bit successive-approximation ADC, meaning the result ranges from 0 to 1023 and represents the ratio of the input voltage to the reference voltage:

```
ADC result = (V_in / V_ref) × 1023
```

Key properties on ATmega328P:

| Property | Value |
|---|---|
| Resolution | 10 bits (0–1023) |
| Channels | 8 external (ADC0–ADC7) + 1 internal temperature sensor (ch 8) |
| Reference options | AREF (external), AVCC, Internal 1.1 V |
| ADC clock range | 50 kHz – 200 kHz for 10-bit; up to ~1 MHz for 8-bit |
| Conversion time | 13 ADC clock cycles (25 for the first after enable) |
| Sample-and-hold window | 1.5 ADC clock cycles |
| Input impedance | Up to 10 kΩ recommended source impedance |

---

## 2. Internal Architecture

### Successive Approximation Register (SAR)

The ADC works by binary search: it tests each bit from the most significant to the least significant, comparing the input voltage against an internally generated voltage from the DAC. Each bit requires one ADC clock cycle, so a 10-bit conversion takes 13 cycles (10 comparison cycles + 1.5 sample-and-hold + overhead).

```
              ┌────────────────────────────────────────────────┐
              │                  ATmega328P ADC                │
              │                                                │
  ADC0–ADC5 ──┤──┐                                            │
  ADC6–ADC7 ──┤  ├─→ Input MUX ──→ Sample & Hold ──→ SAR ──→ ADCL/ADCH
  Temp sensor─┤──┘         ↑               ↑           ↑      │
              │       (ADMUX MUX)    (ADSC starts)  (ADSC    │
              │                                      clears)  │
              │                          ↑                    │
              │                    ADC Clock                  │
              │                 (F_CPU / prescaler)           │
              │                          ↑                    │
              │                    Prescaler (ADPS)           │
              │                                                │
              │  References: AREF pin / AVCC / Internal 1.1V  │
              │              (selected by REFS1:REFS0)         │
              └────────────────────────────────────────────────┘
```

### Input Multiplexer

A single ADC hardware block is shared across all channels via an analog multiplexer. Only one channel can be sampled at a time. Switching channels requires a new conversion; the first result after a channel switch should be discarded if maximum accuracy is required (see [Common Pitfalls](#11-common-pitfalls)).

### Sample-and-Hold

Before conversion begins, the input voltage is captured on an internal capacitor (sample-and-hold). This takes 1.5 ADC clock cycles at the start of each conversion. During sampling the source impedance matters: a high-impedance source may not charge the capacitor fully, reducing accuracy. Keep source impedance below 10 kΩ.

### Reference Voltage

The reference defines full-scale (code 1023). The ADC compares V_in against V_ref internally:

| Reference | Source | Typical value |
|---|---|---|
| `AREF` | External pin (AREF) | User-supplied, 0 V to VCC |
| `AVCC` | VCC with 100 nF cap on AREF | 5.0 V (or 3.3 V) |
| `Internal` | On-chip bandgap | 1.1 V (ATmega328P), 2.56 V (mega32/64/128) |

When switching references at runtime, the internal reference requires a settling time (typically 5–10 ms for the bandgap to stabilize after first enable).

---

## 3. Register Map

The SDK maps directly onto these hardware registers via the macros in `registers.hpp`. Understanding them is essential for debugging and advanced use.

### ADMUX — ADC Multiplexer Selection Register

```
  Bit:   7      6      5      4      3      2      1      0
       REFS1  REFS0  ADLAR   —    MUX3   MUX2   MUX1   MUX0
```

| Bits | Name | Description |
|---|---|---|
| 7:6 | `REFS1:REFS0` | Reference voltage selection (see ADCRef enum) |
| 5 | `ADLAR` | **1** = left-adjust result (ADCH holds MSByte); **0** = right-adjust (default) |
| 3:0 | `MUX3:MUX0` | Channel select — 0x00–0x07 = ADC0–ADC7; 0x08 = temp sensor (328P) |

`begin()` writes the entire ADMUX register. `setReference()` and `selectChannel()` (private) use field-masked writes to update only their respective bits.

### ADCSRA — ADC Control and Status Register A

```
  Bit:   7     6     5     4     3     2     1     0
        ADEN  ADSC  ADATE  ADIF  ADIE  ADPS2 ADPS1 ADPS0
```

| Bit | Name | Description |
|---|---|---|
| 7 | `ADEN` | ADC Enable. Must be 1 for any ADC operation. |
| 6 | `ADSC` | ADC Start Conversion. Write 1 to start; hardware clears it when done. |
| 5 | `ADATE` | ADC Auto Trigger Enable. Enables auto-triggering from the source in ADCSRB. |
| 4 | `ADIF` | ADC Interrupt Flag. Set by hardware on conversion complete. Clear by writing **1** (or by entering the ISR). |
| 3 | `ADIE` | ADC Interrupt Enable. When 1, a completed conversion triggers `ADC_vect` if the global I-bit is set. |
| 2:0 | `ADPS2:ADPS0` | Prescaler select bits — determines ADC clock = F_CPU / divisor. |

### ADCSRB — ADC Control and Status Register B

```
  Bit:   7     6     5     4     3     2     1     0
        —    ACME   —     —     —    ADTS2 ADTS1 ADTS0
```

| Bits | Name | Description |
|---|---|---|
| 6 | `ACME` | Analog Comparator Multiplexer Enable (unrelated to ADC conversion) |
| 2:0 | `ADTS2:ADTS0` | Auto-trigger source when `ADATE=1` |

**ADTS source table:**

| ADTS2:1:0 | Trigger source |
|---|---|
| `000` | **Free Running** — restarts immediately after each conversion |
| `001` | Analog Comparator output edge |
| `010` | External Interrupt 0 |
| `011` | Timer0 Compare Match A |
| `100` | Timer0 Overflow |
| `101` | Timer1 Compare Match B |
| `110` | Timer1 Overflow |
| `111` | Timer1 Capture Event |

For free-running mode, all ADTS bits must be **0** (ADCSRB bits 2:0 = 000).

### ADCL / ADCH — Result Registers

The 10-bit conversion result is split across two 8-bit registers.

**Right-adjusted (ADLAR = 0, default):**

```
  ADCH:  —   —   —   —   —   —  Bit9 Bit8
  ADCL: Bit7 Bit6 Bit5 Bit4 Bit3 Bit2 Bit1 Bit0
```

Read ADCL first, then ADCH — reading ADCL locks both registers until ADCH is read. The SDK reads the combined `ADC` macro (defined in `<avr/io.h>`) which handles this atomically.

**Left-adjusted (ADLAR = 1):**

```
  ADCH: Bit9 Bit8 Bit7 Bit6 Bit5 Bit4 Bit3 Bit2
  ADCL: Bit1 Bit0  —   —   —   —   —   —
```

In this mode, reading only ADCH gives the upper 8 bits — effectively an 8-bit result with no need to touch ADCL. This is what `read8()` exploits.

---

## 4. Enumerations

### `ADCRef`

Selects the voltage reference for the ADC. Passed to `begin()` and `setReference()`.

```cpp
enum class ADCRef : uint8_t {
    AREF     = 0,   // REFS1:REFS0 = 00 — external voltage on AREF pin
    AVCC     = 1,   // REFS1:REFS0 = 01 — VCC (connect 100 nF cap AREF→GND)
    Internal = 3,   // REFS1:REFS0 = 11 — internal bandgap (1.1 V on 328P)
};
```

> `ADCRef::Internal = 3` maps to REFS1:REFS0 = 11. Value 2 (REFS1=1, REFS0=0) is reserved on ATmega328P and must not be used.

**Choosing a reference:**

- `AVCC` — most common choice. Full-scale = VCC. Place a 100 nF bypass capacitor between the AREF pin and GND to suppress noise.
- `AREF` — when you need a precision external reference (e.g. a 2.048 V voltage reference IC) for exact scaling.
- `Internal` — when measuring signals in the 0–1.1 V range with maximum gain from the ADC, or for measuring battery voltage against a known rail using a voltage divider.

### `ADCPrescaler`

Divides F_CPU to produce the ADC clock. Passed to `begin()`.

```cpp
enum class ADCPrescaler : uint8_t {
    DIV2   = 1,   // F_CPU / 2
    DIV4   = 2,   // F_CPU / 4
    DIV8   = 3,   // F_CPU / 8
    DIV16  = 4,   // F_CPU / 16
    DIV32  = 5,   // F_CPU / 32
    DIV64  = 6,   // F_CPU / 64
    DIV128 = 7,   // F_CPU / 128
};
```

The raw values (1–7) write directly into the ADPS2:ADPS0 bits of ADCSRA. See [Section 7](#7-prescaler-selection) for a complete selection table.

---

## 5. Global Instance

```cpp
extern ADCDriver ADC_Driver;   // defined in adc.cpp
```

`ADC_Driver` is the single stateless singleton that wraps the ADC hardware. Because the AVR has one ADC block, there is no value in having multiple `ADCDriver` objects — all methods operate directly on the hardware registers.

Include the header and use the global object:

```cpp
#include <mikroduino/adc.hpp>
using namespace MikroDuino;

// Then use:
ADC_Driver.begin();
uint16_t v = ADC_Driver.read(3);
```

---

## 6. API Reference

---

### `begin()`

```cpp
void begin(
    ADCRef       ref        = ADCRef::AVCC,
    ADCPrescaler prescaler  = ADCPrescaler::DIV128,
    bool         leftAdjust = false);
```

Initializes and enables the ADC. This must be called before any conversion.

**Parameters:**

| Parameter | Default | Description |
|---|---|---|
| `ref` | `ADCRef::AVCC` | Voltage reference (see ADCRef) |
| `prescaler` | `ADCPrescaler::DIV128` | ADC clock prescaler (see ADCPrescaler) |
| `leftAdjust` | `false` | `true` = result left-aligned in ADCH (required for `read8()`); `false` = right-aligned (standard 10-bit) |

**Hardware effect:**

```cpp
ADMUX  = (ref << REFS0) | (leftAdjust ? (1 << ADLAR) : 0);
ADCSRA = (1 << ADEN) | prescaler;
```

- Writes ADMUX: sets reference bits and ADLAR. Channel (MUX bits) is set to 0 (ADC0) until the first `read()` call.
- Writes ADCSRA: sets ADEN (enable) and the prescaler bits. Clears ADSC, ADATE, ADIF, ADIE.

**Notes:**

- Calling `begin()` again at any time reconfigures the ADC and clears ADATE/ADIE. Use this to switch from left-adjust (8-bit) to right-adjust (10-bit) mode or to change the prescaler at runtime.
- The first conversion after `begin()` takes 25 ADC clock cycles instead of 13 (initialization overhead).
- Does **not** start a conversion; call `read()` or `beginFreeRunning()` after this.

---

### `end()`

```cpp
void end();
```

Disables the ADC by clearing the ADEN bit in ADCSRA.

**Hardware effect:**

```cpp
ADCSRA &= ~(1 << ADEN);
```

When disabled:
- The ADC draws no power (relevant for battery-powered applications — ADC consumes ~300 µA when enabled).
- Any in-progress conversion is aborted.
- ADMUX and prescaler settings are preserved; calling `begin()` restores operation without needing to reconfigure them.

---

### `read()`

```cpp
uint16_t read(uint8_t channel);
```

Performs a single blocking 10-bit conversion and returns the result.

**Parameters:**

| Parameter | Description |
|---|---|
| `channel` | ADC channel number: 0–7 for external pins; 8 = internal temperature sensor (ATmega328P only) |

**Returns:** `uint16_t` in the range 0–1023. Only bits 9:0 are valid; bits 15:10 are always 0.

**Hardware sequence:**

```cpp
// 1. Select channel (write MUX bits in ADMUX, preserve reference/ADLAR bits)
ADMUX = (ADMUX & 0xF0) | (channel & 0x0F);

// 2. Start conversion
ADCSRA |= (1 << ADSC);

// 3. Spin-wait until ADSC is cleared by hardware (conversion complete)
while (ADCSRA & (1 << ADSC));

// 4. Return combined 16-bit result register
return ADC;   // reads ADCL then ADCH atomically
```

**Timing:** Blocks for one complete conversion — 13 ADC clock cycles after the first call (which costs 25). At 125 kHz ADC clock this is approximately 104 µs.

**Notes:**

- Requires `begin()` to have been called first.
- Result is only meaningful when `leftAdjust = false` (right-aligned). If left-adjust was enabled, bits 9:2 of the result are in ADCH and bits 1:0 are in ADCL — using `read8()` is cleaner in that case.
- If called during free-running mode, the auto-trigger will fire again immediately after — call `stopFreeRunning()` first.

**Example:**

```cpp
ADC_Driver.begin();                        // AVCC, DIV128, right-aligned
uint16_t raw = ADC_Driver.read(7);         // read A7 (potentiometer)
uint16_t mV  = (uint32_t)raw * 5000 / 1023; // convert to millivolts (5 V ref)
```

---

### `read8()`

```cpp
uint8_t read8(uint8_t channel);
```

Performs a single blocking conversion and returns only the upper 8 bits of the result (ADCH).

**Requires `leftAdjust = true`** to have been passed to `begin()`. Without left-adjust, ADCH only contains bits 9:8 of the result and `read8()` returns a nearly useless 2-bit value.

**Parameters:** same as `read()`.

**Returns:** `uint8_t` in the range 0–255. Equivalent to `(full_10bit_result >> 2)`.

**Hardware sequence:**

```cpp
ADMUX = (ADMUX & 0xF0) | (channel & 0x0F);
ADCSRA |= (1 << ADSC);
while (ADCSRA & (1 << ADSC));
return ADCH;   // only upper register — ADCL is never read
```

**Advantages over `read()`:**

- Reading only ADCH is safe and simpler — no need to read ADCL first.
- Adequate for most display/control applications (256 levels is often enough).
- Slightly faster: no ADCL read, no 16-bit assembly.
- Useful with higher prescalers (e.g. DIV32 at 16 MHz = 500 kHz ADC clock) where 10-bit accuracy is not guaranteed but 8-bit is.

**Example:**

```cpp
ADC_Driver.end();
ADC_Driver.begin(ADCRef::AVCC, ADCPrescaler::DIV64, true);   // left-adjust ON
uint8_t brightness = ADC_Driver.read8(6);                    // 0-255 directly
```

---

### `setReference()`

```cpp
void setReference(ADCRef ref);
```

Changes the voltage reference without restarting or reconfiguring the ADC.

**Hardware effect:**

```cpp
// Read-modify-write: update only REFS1:REFS0, leave other bits untouched
ADMUX = (ADMUX & ~(0x03 << REFS0)) | (ref << REFS0);
```

This uses the `FIELD_SET` macro from `registers.hpp`.

**Notes:**

- The channel, left-adjust setting, and prescaler are all preserved.
- The new reference takes effect on the **next conversion start**. If switching to the internal bandgap reference, wait at least 5–10 ms before the first reading for the reference to settle:

```cpp
ADC_Driver.setReference(ADCRef::Internal);
_delay_ms(10);                              // allow bandgap to settle
uint16_t v = ADC_Driver.read(0);
```

- Switching back to AVCC from Internal does not require a settling delay.
- Can be called mid-stream in free-running mode, but the conversion in progress at that moment will use the old reference; the change takes effect from the next conversion.

---

### `beginFreeRunning()`

```cpp
void beginFreeRunning(uint8_t channel);
```

Starts continuous automatic conversions on a single fixed channel. Each conversion restarts automatically as soon as the previous one finishes, without any CPU involvement.

**Parameters:**

| Parameter | Description |
|---|---|
| `channel` | ADC channel to sample continuously (fixed for the duration) |

**Hardware sequence:**

```cpp
ADMUX  = (ADMUX & 0xF0) | (channel & 0x0F);  // select channel
ADCSRB = ADCSRB | (1 << 0);                  // ADTS0 = 1 (see note)
ADCSRA |= (1 << ADATE);                       // enable auto-trigger
ADCSRA |= (1 << ADSC);                        // start first conversion
```

> **Implementation note:** The SDK sets ADCSRB bit 0 (ADTS0 = 1), which according to the datasheet selects "Analog Comparator" as the auto-trigger source (ADTS = 001), not free-running (ADTS = 000). In practice, when the Analog Comparator is not actively used, this behaves similarly to free-running because the comparator output is undefined and toggles the trigger spontaneously. For **guaranteed** free-running operation, clear bits 2:0 of ADCSRB before calling this function:
>
> ```cpp
> ADCSRB &= ~0x07;                // ADTS = 000 → true free running
> ADC_Driver.beginFreeRunning(7);
> ```

**Notes:**

- Only one channel can be sampled in free-running mode. To sample multiple channels you must exit free-running and use blocking `read()` calls for each.
- Results are available approximately every 13 ADC clock cycles (104 µs at 125 kHz).
- The ADIF flag is set after every conversion. In polling mode, check `conversionComplete()` and call `clearFlag()`. In interrupt mode, the ISR fires and ADIF clears automatically when the ISR is entered.

---

### `stopFreeRunning()`

```cpp
void stopFreeRunning();
```

Stops the automatic conversion cycle by clearing ADATE and ADSC.

**Hardware effect:**

```cpp
ADCSRA &= ~(1 << ADATE);
ADCSRA &= ~(1 << ADSC);
```

Any conversion in progress when this is called will **complete normally** — the ADC finishes the current conversion and then stops. Call `resultFreeRunning()` one more time after stopping if you need the last sample.

After `stopFreeRunning()`, the ADC remains enabled (ADEN is still set). Use `read()` for subsequent single-shot conversions without calling `begin()` again.

---

### `resultFreeRunning()`

```cpp
uint16_t resultFreeRunning() const;
```

Returns the result of the most recently completed free-running conversion.

**Hardware effect:**

```cpp
return ADC;   // reads ADCL then ADCH — the last converted value
```

**Notes:**

- Does **not** start a new conversion; that happens automatically in free-running mode.
- Unlike `read()`, this is non-blocking — it returns whatever is currently in the ADC register.
- If called before any conversion completes (e.g. immediately after `beginFreeRunning()`), the result is undefined (likely 0 or a previous value).
- Use `conversionComplete()` to know when a fresh result is available, and `clearFlag()` to arm detection of the next one.

---

### `conversionComplete()`

```cpp
bool conversionComplete() const;
```

Returns `true` if the ADC Interrupt Flag (ADIF) is set, meaning a conversion has finished and a new result is ready in the ADC register.

**Hardware effect:**

```cpp
return (ADCSRA >> ADIF) & 1;
```

**Notes:**

- ADIF is set by hardware at the end of every conversion, whether from `read()`, `beginFreeRunning()`, or any other trigger.
- After `read()` returns, ADIF is set. Reading the ADC register (inside `read()`) does **not** automatically clear ADIF when polling — it is only auto-cleared when entering the `ADC_vect` ISR.
- After checking `conversionComplete()`, always call `clearFlag()` (or read the ADC register) before waiting for the next conversion; otherwise it will appear to stay true forever.
- In free-running mode, ADIF is cleared by `clearFlag()` so that the next conversion's completion can be detected.

**Typical polling pattern:**

```cpp
ADC_Driver.beginFreeRunning(7);
while (true) {
    if (ADC_Driver.conversionComplete()) {
        uint16_t val = ADC_Driver.resultFreeRunning();
        ADC_Driver.clearFlag();        // arm detection of next conversion
        process(val);
    }
    do_other_work();                   // CPU is free — no blocking wait
}
```

---

### `clearFlag()`

```cpp
void clearFlag();
```

Clears the ADC Interrupt Flag (ADIF) by writing 1 to it (the AVR convention for clearing interrupt flags — writing 0 has no effect).

**Hardware effect:**

```cpp
ADCSRA |= (1 << ADIF);   // writing 1 clears ADIF
```

**When to call it:**

- After reading `resultFreeRunning()` in a polling loop, to detect the completion of the **next** conversion.
- After `read()` completes, if you intend to use `conversionComplete()` to detect a subsequent conversion.
- Not needed after `read()` if you are not using `conversionComplete()` afterward — `read()` is self-contained.
- Not needed in interrupt-driven mode — ADIF is cleared automatically when the `ADC_vect` ISR is entered.

---

### `enableInterrupt()`

```cpp
void enableInterrupt();
```

Sets the ADC Interrupt Enable bit (ADIE) in ADCSRA. When ADIE is set **and** the global interrupt enable (I-bit in SREG) is set, the `ADC_vect` interrupt fires at the end of every conversion.

**Hardware effect:**

```cpp
ADCSRA |= (1 << ADIE);
```

**Usage requirements:**

1. Call `enableInterrupt()` before or after starting the ADC.
2. Define `ISR(ADC_vect)` in your code.
3. Call `sei()` to enable global interrupts.
4. Start conversions with `beginFreeRunning()` or a manual ADSC write.

The ISR fires once per conversion. Reading the ADC register inside the ISR clears ADIF automatically.

**Example ISR:**

```cpp
#include <avr/interrupt.h>

volatile uint16_t g_adc_val = 0;
volatile bool     g_ready   = false;

ISR(ADC_vect) {
    g_adc_val = ADC;   // reading ADC clears ADIF and re-arms the interrupt
    g_ready   = true;
}
```

Then in `main()`:

```cpp
ADC_Driver.begin();
ADC_Driver.enableInterrupt();
ADC_Driver.beginFreeRunning(7);
sei();

while (true) {
    if (g_ready) {
        g_ready = false;
        uint16_t snap = g_adc_val;   // snapshot — ISR can update g_adc_val anytime
        process(snap);
    }
}
```

---

### `disableInterrupt()`

```cpp
void disableInterrupt();
```

Clears the ADIE bit, preventing the `ADC_vect` ISR from firing. Conversions continue (ADEN and ADSC remain set), but completion is no longer signaled via interrupt.

**Hardware effect:**

```cpp
ADCSRA &= ~(1 << ADIE);
```

Call this before `stopFreeRunning()` when tearing down interrupt-driven ADC to avoid a spurious interrupt on the last conversion:

```cpp
cli();
ADC_Driver.disableInterrupt();
ADC_Driver.stopFreeRunning();
```

---

## 7. Prescaler Selection

The ADC clock must be in the 50–200 kHz range for full 10-bit accuracy. Up to ~1 MHz is acceptable for 8-bit accuracy. Below 50 kHz, conversion time exceeds the sample-and-hold window.

| `ADCPrescaler` | Divisor | F_CPU = 16 MHz | F_CPU = 8 MHz | F_CPU = 1 MHz | 10-bit? |
|---|---|---|---|---|---|
| `DIV2` | 2 | 8 MHz | 4 MHz | 500 kHz | No |
| `DIV4` | 4 | 4 MHz | 2 MHz | 250 kHz | No |
| `DIV8` | 8 | 2 MHz | 1 MHz | 125 kHz | 8-bit only at 16/8 MHz; **Yes** at 1 MHz |
| `DIV16` | 16 | 1 MHz | 500 kHz | 62.5 kHz | 8-bit only at 16/8 MHz; marginal at 1 MHz |
| `DIV32` | 32 | 500 kHz | 250 kHz | 31 kHz | 8-bit only |
| `DIV64` | 64 | 250 kHz | 125 kHz | — | Marginal at 16 MHz; **Yes** at 8 MHz |
| `DIV128` | 128 | **125 kHz ✓** | 62.5 kHz | — | **Yes** at 16 MHz (recommended); No at 8 MHz |

**Quick selection guide:**

```
F_CPU = 16 MHz → DIV128  (125 kHz, 10-bit)   ← recommended default
F_CPU =  8 MHz → DIV64   (125 kHz, 10-bit)
F_CPU =  4 MHz → DIV32   (125 kHz, 10-bit)
F_CPU =  1 MHz → DIV8    (125 kHz, 10-bit)

For 8-bit only (faster conversions):
F_CPU = 16 MHz → DIV32   (500 kHz, 8-bit)
```

---

## 8. Conversion Timing

### Cycle counts

| Event | ADC clock cycles |
|---|---|
| First conversion after `begin()` | **25** |
| Normal single conversion | **13** |
| Sample-and-hold window (start of each conversion) | 1.5 |
| Free-running restart gap between conversions | 0 (continuous) |

### Wall-clock timing at common F_CPU values

| F_CPU | Prescaler | ADC clock | 1st conversion | Normal conversion | Max sample rate |
|---|---|---|---|---|---|
| 16 MHz | DIV128 | 125 kHz | 200 µs | **104 µs** | ~9,600 Hz |
| 16 MHz | DIV64 | 250 kHz | 100 µs | **52 µs** | ~19,000 Hz |
| 16 MHz | DIV32 | 500 kHz | 50 µs | **26 µs** | ~38,000 Hz |
| 8 MHz | DIV64 | 125 kHz | 200 µs | **104 µs** | ~9,600 Hz |
| 1 MHz | DIV8 | 125 kHz | 200 µs | **104 µs** | ~9,600 Hz |

---

## 9. Usage Patterns

### 9.1 Single-shot blocking (most common)

```cpp
#include <mikroduino/adc.hpp>
using namespace MikroDuino;

int main() {
    ADC_Driver.begin();                       // AVCC, DIV128, 10-bit

    while (true) {
        uint16_t raw = ADC_Driver.read(0);    // A0, 0–1023
        // map to millivolts (5 V reference):
        uint16_t mV = (uint32_t)raw * 5000u / 1023u;
        use(mV);
    }
}
```

### 9.2 8-bit left-adjusted fast read

Use when 256 levels is sufficient and you want to avoid 16-bit arithmetic.

```cpp
ADC_Driver.begin(ADCRef::AVCC, ADCPrescaler::DIV32, true);  // left-adjust ON

while (true) {
    uint8_t v = ADC_Driver.read8(0);   // 0–255 directly from ADCH
    use(v);
}
```

### 9.3 Switching channels at runtime

Each `read()` call selects its channel before converting. No explicit reconfiguration needed.

```cpp
ADC_Driver.begin();

uint16_t pot   = ADC_Driver.read(7);   // A7 — potentiometer
uint16_t light = ADC_Driver.read(6);   // A6 — light sensor
```

> For maximum accuracy after a channel switch, perform one dummy read (discarded) before taking the real measurement. This allows the input capacitor to settle at the new voltage:
>
> ```cpp
> ADC_Driver.read(6);                    // dummy — charge input cap
> uint16_t light = ADC_Driver.read(6);   // real measurement
> ```

### 9.4 Reference switching

```cpp
ADC_Driver.begin(ADCRef::AVCC, ADCPrescaler::DIV128, false);

// Normal full-range reading (0–5 V → 0–1023)
uint16_t v_avcc = ADC_Driver.read(0);

// Switch to internal 1.1 V for high-gain small-signal measurement
ADC_Driver.setReference(ADCRef::Internal);
_delay_ms(10);                          // wait for bandgap to settle

// Same input now reads relative to 1.1 V (saturates above ~1.1 V)
uint16_t v_1v1 = ADC_Driver.read(0);

ADC_Driver.setReference(ADCRef::AVCC);  // restore
```

### 9.5 Free-running with polling

```cpp
ADC_Driver.begin();

// Clear ADTS bits for guaranteed free-running (ADTS = 000)
ADCSRB &= ~0x07;
ADC_Driver.beginFreeRunning(7);         // potentiometer on A7

while (true) {
    if (ADC_Driver.conversionComplete()) {
        uint16_t val = ADC_Driver.resultFreeRunning();
        ADC_Driver.clearFlag();          // arm for next conversion
        display(val);
    }
    // Other work runs here without blocking
}
```

### 9.6 Interrupt-driven free-running

```cpp
#include <avr/interrupt.h>
#include <mikroduino/adc.hpp>
using namespace MikroDuino;

volatile uint16_t g_sample = 0;
volatile bool     g_ready  = false;

ISR(ADC_vect) {
    g_sample = ADC;      // reading ADC clears ADIF and re-arms the interrupt
    g_ready  = true;
}

int main() {
    ADC_Driver.begin();
    ADC_Driver.enableInterrupt();

    ADCSRB &= ~0x07;                    // ADTS = 000 — true free running
    ADC_Driver.beginFreeRunning(6);     // light sensor on A6
    sei();

    while (true) {
        if (g_ready) {
            uint16_t snap = g_sample;   // atomic snapshot (uint16_t read is not atomic on 8-bit AVR)
            g_ready = false;
            process(snap);
        }
    }
}
```

> **Warning:** On an 8-bit AVR, reading a 16-bit variable is not atomic. If the ISR fires between the two byte reads of `g_sample`, you get a corrupted value. Use `cli()`/`sei()` around the snapshot or use `ATOMIC_BLOCK_START` / `ATOMIC_BLOCK_END` from `registers.hpp`:
>
> ```cpp
> uint16_t snap;
> ATOMIC_BLOCK_START;
>     snap    = g_sample;
>     g_ready = false;
> ATOMIC_BLOCK_END;
> process(snap);
> ```

### 9.7 Oversampling for extra resolution

Taking N² samples and averaging gives extra bits of resolution (at the cost of conversion time). To gain 2 extra bits (12-bit effective resolution), average 16 samples:

```cpp
ADC_Driver.begin();

uint32_t sum = 0;
for (uint8_t i = 0; i < 16; ++i) {
    sum += ADC_Driver.read(0);
}
uint16_t result12 = sum >> 2;   // divide by 4 to get 12-bit result (0–4092)
```

---

## 10. MCU Differences

The SDK supports five MCUs via `platform.hpp`. ADC-relevant differences:

| MCU | Channels | Internal reference | ADC pins |
|---|---|---|---|
| ATmega328P | 8 (ADC0–ADC7) + temp (ch 8) | 1.1 V | ADC0–ADC5 on PC0–PC5; ADC6–ADC7 are **analog-only** (TQFP/QFN only) |
| ATmega32 | 8 (ADC0–ADC7) | 2.56 V | PA0–PA7 |
| ATmega16 | 8 (ADC0–ADC7) | 2.56 V | PA0–PA7 |
| ATmega64 | 8 (ADC0–ADC7) | 2.56 V | PF0–PF7 |
| ATmega128 | 8 (ADC0–ADC7) | 2.56 V | PF0–PF7 |

### ADC6 and ADC7 on ATmega328P

These two channels exist only on the TQFP-32 and MLF-32 surface-mount packages. They are **not** present on the PDIP-28 package (the through-hole "Arduino Uno" chip). They are analog-input-only: there is no DDR, PORT, or PIN register associated with them. No GPIO configuration is needed or possible.

```
Package       ADC6/ADC7 available?
─────────────────────────────────
PDIP-28       NO
TQFP-32       YES (Arduino Nano / Pro Mini)
MLF-32        YES
```

### Internal reference voltage

```cpp
// ATmega328P  → ADCRef::Internal = 1.1 V
// ATmega32/16 → ADCRef::Internal = 2.56 V (requires external cap on AREF)
// ATmega64/128→ ADCRef::Internal = 2.56 V
```

`ADCRef::Internal` maps to REFS1:REFS0 = 11 on all targets. The voltage is MCU-specific.

---

## 11. Common Pitfalls

### Floating analog inputs

An unconnected ADC pin floats and picks up noise, producing unstable readings. Always connect unused ADC pins to GND or to a known voltage via a resistor. For connected sensors, ensure there is a DC path to either GND or VCC.

### Source impedance

The ADC input capacitor (typically ~14 pF) must charge fully during the 1.5-cycle sample window. With a source resistance of R_s:

```
Charge time = R_s × C_ADC = R_s × 14 pF

Required time ≥ 1.5 / f_ADC

Maximum R_s ≈ (1.5 / f_ADC) / 14 pF
             ≈ (1.5 / 125,000) / 14e-12
             ≈ 857,000 Ω  (~860 kΩ) at 125 kHz ADC clock
```

In practice, keep source impedance below 10 kΩ for reliable 10-bit results. Higher impedance reduces effective accuracy.

### Internal reference settling time

The internal bandgap reference takes 5–10 ms to stabilize after it is first selected. Always insert a delay after `setReference(ADCRef::Internal)` or the first `begin()` with the internal reference:

```cpp
ADC_Driver.setReference(ADCRef::Internal);
_delay_ms(10);                          // mandatory settling delay
```

### Channel switching transient

After selecting a new channel, the input multiplexer connects a new voltage to the sample capacitor. If the previous and new voltages differ significantly, the capacitor may not fully settle in one sample window. Discard the first reading:

```cpp
ADC_Driver.read(6);                     // dummy — settles the input cap
uint16_t light = ADC_Driver.read(6);    // accurate reading
```

### `read8()` requires `leftAdjust = true`

Calling `read8()` without enabling left-adjust returns a near-zero value (only the top 2 bits of the 10-bit result are in ADCH). Always pair them:

```cpp
// CORRECT
ADC_Driver.begin(ADCRef::AVCC, ADCPrescaler::DIV128, true);
uint8_t v = ADC_Driver.read8(0);

// WRONG — returns garbage (bits 9:8 only)
ADC_Driver.begin();                     // leftAdjust defaults to false
uint8_t v = ADC_Driver.read8(0);       // ← do not do this
```

### 16-bit read is not atomic on 8-bit AVR

Reading a `uint16_t` variable shared with an ISR requires disabling interrupts around the read, or the ISR may corrupt the value between the two 8-bit load instructions:

```cpp
// WRONG — ISR can fire between reading the low and high bytes
uint16_t val = g_sample;

// CORRECT
uint16_t val;
ATOMIC_BLOCK_START;
    val = g_sample;
ATOMIC_BLOCK_END;
```

### ADIF not cleared automatically in polling mode

In polling mode (no interrupt), `conversionComplete()` stays `true` until you explicitly call `clearFlag()`. Forgetting this causes the polling loop to process the same sample repeatedly:

```cpp
// WRONG — clearFlag() missing; loop processes same sample forever
if (ADC_Driver.conversionComplete()) {
    process(ADC_Driver.resultFreeRunning());
}

// CORRECT
if (ADC_Driver.conversionComplete()) {
    process(ADC_Driver.resultFreeRunning());
    ADC_Driver.clearFlag();              // arm for next conversion
}
```

### `beginFreeRunning()` and ADTS register state

As noted in the [API Reference](#beginFreeRunning), the SDK implementation sets ADTS0=1 (Analog Comparator trigger) rather than clearing all ADTS bits for free-running (ADTS=000). Clear ADTS manually before calling `beginFreeRunning()` for guaranteed free-running behavior:

```cpp
ADCSRB &= ~0x07;                        // ADTS = 000 → free running
ADC_Driver.beginFreeRunning(7);
```

### Power consumption

The ADC draws approximately 300 µA when enabled (ADEN=1). In power-critical designs, disable the ADC between readings:

```cpp
uint16_t sample_low_power(uint8_t ch) {
    ADC_Driver.begin();
    ADC_Driver.read(ch);                // dummy — first conversion is slow
    uint16_t val = ADC_Driver.read(ch);
    ADC_Driver.end();                   // disable → ~0 µA ADC current
    return val;
}
```
