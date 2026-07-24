# SevenSegShift

N-digit multiplexed 7-segment display driver over two cascaded 74HC595 shift registers — 3 MCU pins for 4 or 8 digits

**Header:** `include/SevenSegShift.hpp`
**Source:** `src/SevenSegShift.cpp`

## Overview

`SevenSegShift<N>` drives an N-digit multiplexed 7-segment display through
two cascaded 74HC595 shift registers — the common "4-digit" or "8-digit
7-segment display module" boards found on eBay/AliExpress. One 74HC595
drives the shared segment bus (7 segments + DP), the other drives one
digit-select line per digit; both are loaded and latched together on every
`refresh()` call.

Only 3 MCU pins are needed regardless of digit count — DATA (SER), CLOCK
(SRCLK), and LATCH (RCLK) — versus the 7 + N pins `SevenSegMux<N>`
(`sdk/modules/SevenSeg`) needs when wired directly to the MCU. Reach for
`SevenSegShift` when you're short on GPIO pins and have (or are adding) a
74HC595-based board; reach for `SevenSegMux` when you have pins to spare
and want to skip the shift registers entirely.

## Wiring

Two 74HC595s, cascaded (QH' of the first feeds SER of the second):

| Signal | Connect to |
|---|---|
| MCU `dataPin` | SER of chip A |
| MCU `clockPin` | SRCLK of chip A **and** chip B (shared) |
| MCU `latchPin` | RCLK of chip A **and** chip B (shared) |
| QH' of chip A | SER of chip B |

One chip's outputs (QA..QH) drive the 7 segments + DP; the other chip's
outputs drive one digit-select line per digit (through a transistor per
digit on most boards). Which chip is "first" in the chain (closest to the
MCU) varies by board — tell the constructor via the `ChainOrder` argument:

- `ChainOrder::SEGMENTS_FIRST` (default) — chip A drives segments, chip B drives digit-select.
- `ChainOrder::DIGITS_FIRST` — chip A drives digit-select, chip B drives segments.

If the display shows garbled segments or the wrong digit lit, swap this
flag before checking anything else in the wiring.

Segment bit order: bit0=a, bit1=b, bit2=c, bit3=d, bit4=e, bit5=f, bit6=g, bit7=dp (same encoding as `SevenSeg`).

### Polarity

| Constructor param | Default | Meaning |
|---|---|---|
| `commonAnode` | `false` | Segment byte polarity. `false` = common cathode (segment bit 1 = lit, sent as-is). `true` = common anode (byte is inverted before shifting, since the 74HC595 output must idle HIGH to keep an anode segment off). |
| `digitActiveHigh` | `true` | Digit-select byte polarity. `true` = selected digit's transistor driven by a HIGH output (common-cathode boards with an NPN driver per digit — the typical wiring). `false` = active-LOW digit driver (common-anode boards with a PNP driver per digit). |

## API

**Timing model:** `refresh()` shows one digit and shifts+latches 16 bits
(both chips) per call. Call it every ~2 ms (main loop, Scheduler, or Timer2
auto mode) so the full N-digit cycle repeats fast enough to look solid.

| Method | Description |
|---|---|
| `SevenSegShift<N>(dataPin, clockPin, latchPin, commonAnode=false, digitActiveHigh=true, order=SEGMENTS_FIRST)` | Constructor |
| `begin()` | Configures the 3 control pins as outputs and blanks the display |
| `setDigit(pos, val)` | Write a decimal (0-9) or hex (0-15) digit at position `pos`; DP preserved |
| `setChar(pos, c)` | Write a character (`'0'-'9'`, `'A'-'F'`, `-`, `_`, and more); DP preserved |
| `setRaw(pos, seg)` | Write a raw segment bitmask (bit0=a … bit6=g, bit7=dp) |
| `setDP(pos, on)` | Turn the decimal point on/off at `pos` without changing the digit |
| `clear()` | Blank the buffer (does not touch the outputs until the next `refresh()`) |
| `print(int16_t n, leadingZero=false)` | Display a signed integer, right-justified |
| `print(uint16_t n, leadingZero=false)` | Display an unsigned integer, right-justified |
| `printHex(n)` | Display a 16-bit value as N hex digits, left-padded with `0` |
| `printFloat(val, decimals=1)` | Display a float with N-1 flash overhead; needs `!MD_NO_FLOAT` |
| `refresh()` | Advance to the next digit: shift + latch one 16-bit frame. Call every ~2 ms |
| `beginTimer2(prescaler=64)` | Configure Timer2 to call `refresh()` automatically (see conflict note below) |

### Timer2 conflict note

`beginTimer2()` defines `TIMER2_OVF_vect`. Do **not** call it alongside
`SevenSeg`'s `SevenSegMux::beginTimer2()`, `IRRemote::begin()` (also owns
`TIMER2_OVF_vect`), or `DCMotor` using `OC2A`/`OC2B` pins. Only one Timer2
AUTO owner is allowed per project — drive the others manually instead
(call `refresh()` yourself, e.g. from a `Scheduler` task).

## Example

```cpp
#include <util/delay.h>
#include <SevenSegShift.hpp>

using namespace MikroDuino;

SevenSegShift<4> display(PD4, PD5, PD6);   // data, clock, latch

int main() {
    display.begin();
    display.print(static_cast<int16_t>(1234));

    while (true) {
        display.refresh();
        _delay_ms(2);   // ~2 ms per digit → solid-looking 4-digit display
    }
}
```

For an 8-digit board, only the template argument and constructor differ:

```cpp
SevenSegShift<8> display(PD4, PD5, PD6);
```

A full six-project walkthrough — from this basic manual refresh, through
ISR and Timer2-auto refresh strategies, a Button-driven hex counter, up
to an 8-digit scoreboard with a boot-time scrolling marquee and
EEPROM-backed high score — ships in
`examples/Modules/SevenSegShift/01_hello_shift_basics` through
`06_eight_digit_scoreboard_marquee` (also under
`Samples/Modules/SevenSegShift/`). Open any of them from the IDE's
**File > Examples** menu.

## See also

- `sdk/modules/SevenSeg` — the direct-GPIO counterpart (`SevenSegMux<N>`) for boards/projects with 7+N pins to spare instead of a 74HC595 pair.
