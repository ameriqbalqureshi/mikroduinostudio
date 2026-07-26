# MikroDuino GPIO Reference

## Overview

`MikroDuino::GPIO` is a **pure-static** utility class — you never create an instance.
Every method is called directly on the class: `GPIO::output(PB5)`.

Include:
```cpp
#include <mikroduino/gpio.hpp>      // GPIO only
// — or —
#include <mikroduino/mikroduino.hpp> // entire SDK
using namespace MikroDuino;
```

---

## Pin Constants

Pin constants encode both the **port** and the **bit number** in a single `uint8_t`.
They replace the raw AVR macros (`DDRB`, `PORTB`, etc.) with names that are
portable across all supported MCUs.

### ATmega328P

| Port | Pins | Notes |
|------|------|-------|
| B | `PB0` … `PB7` | SPI (PB2–PB5), crystal (PB6–PB7) |
| C | `PC0` … `PC6` | ADC0–ADC5 (PC0–PC5), RESET (PC6) |
| D | `PD0` … `PD7` | UART (PD0–PD1), INT0/INT1 (PD2–PD3) |

> ATmega328P has no Port A, E, F, or G.

### ATmega32 / ATmega16

| Port | Pins |
|------|------|
| A | `PA0` … `PA7` |
| B | `PB0` … `PB7` |
| C | `PC0` … `PC7` |
| D | `PD0` … `PD7` |

### ATmega64 / ATmega128

All ports available: `PA0`–`PA7`, `PB0`–`PB7`, `PC0`–`PC7`, `PD0`–`PD7`,
`PE0`–`PE7`, `PF0`–`PF7`, `PG0`–`PG4`.

### ATmega2560

`PA0`–`PA7`, `PB0`–`PB7`, `PC0`–`PC7`, `PD0`–`PD7`, `PE0`–`PE7`, `PF0`–`PF7`,
`PG0`–`PG5` (this MCU has a 6-bit Port G, unlike the 5-bit `PG0`–`PG4` on
ATmega64/128), plus four ports the smaller MCUs don't have at all:
`PH0`–`PH7`, `PJ0`–`PJ7`, `PK0`–`PK7`, `PL0`–`PL7`.

> Adding ports beyond G required widening the pin-encoding scheme from a 3-bit
> to a 4-bit port field (see `gpio.hpp`'s header comment). Existing pin
> constants for ports A–G keep the same numeric value, so this is not a
> breaking change for existing code.

---

## API Reference

### Direction

```cpp
GPIO::output(uint8_t pin)
```
Configure `pin` as a **digital output** (sets the DDRx bit).

```cpp
GPIO::input(uint8_t pin)
```
Configure `pin` as a **floating input** (clears DDRx and PORTx bits).

```cpp
GPIO::inputPullup(uint8_t pin)
```
Configure `pin` as an **input with internal pull-up** (clears DDRx, sets PORTx).
Use this for buttons wired to GND — no external resistor needed.

---

### Output

```cpp
GPIO::set(uint8_t pin)
```
Drive `pin` **HIGH** (3.3 V or 5 V depending on MCU supply).

```cpp
GPIO::clear(uint8_t pin)
```
Drive `pin` **LOW** (0 V / GND).

```cpp
GPIO::write(uint8_t pin, bool value)
```
Drive `pin` HIGH if `value` is `true`, LOW if `false`.
Equivalent to `if (value) GPIO::set(pin); else GPIO::clear(pin);`

```cpp
GPIO::toggle(uint8_t pin)
```
Flip the current output state of `pin`.
Implemented using the **AVR hardware toggle trick** (writing 1 to the PINx
register), which is a single-cycle atomic operation — faster and safer than a
read-modify-write on PORTx.

---

### Input

```cpp
bool GPIO::read(uint8_t pin)
```
Return `true` if `pin` is currently HIGH, `false` if LOW.
Reads the PINx register. Only meaningful on pins configured as inputs;
on output pins it reads back the driven value.

---

### Port-wide (batch) Operations

These operate on an entire 8-bit port at once using a **port index** (not a pin
constant) and a **bitmask**.

| Index | Port |
|-------|------|
| 0 | A |
| 1 | B |
| 2 | C |
| 3 | D |
| 4 | E |
| 5 | F |
| 6 | G |

```cpp
GPIO::portOutput(uint8_t portIdx, uint8_t mask)
```
Set all bits in `mask` as outputs (SETMASK on DDRx).

```cpp
GPIO::portInput(uint8_t portIdx, uint8_t mask)
```
Set all bits in `mask` as inputs (CLEARMASK on DDRx).

```cpp
GPIO::portWrite(uint8_t portIdx, uint8_t value)
```
Write `value` directly to PORTx — sets all 8 bits at once.

```cpp
uint8_t GPIO::portRead(uint8_t portIdx)
```
Read the full PINx register and return it as a `uint8_t`.

---

## Examples

### Blink an LED (SDK style)

LED on PB5, toggles every 500 ms.

```cpp
#include <mikroduino/gpio.hpp>
#include <util/delay.h>
using namespace MikroDuino;

int main() {
    GPIO::output(PB5);          // configure PB5 as output

    while (1) {
        GPIO::toggle(PB5);      // hardware toggle — single cycle
        _delay_ms(500);
    }
}
```

Compare with raw AVR:
```cpp
// raw AVR equivalent — more verbose, easier to make mistakes
DDRB  |=  (1 << DDB5);
while (1) {
    PINB = (1 << PINB5);       // same AVR toggle trick, but manual
    _delay_ms(500);
}
```

---

### Button with Internal Pull-up

Button between PD2 and GND; LED on PB5 mirrors it.

```cpp
#include <mikroduino/gpio.hpp>
using namespace MikroDuino;

int main() {
    GPIO::output(PB5);          // LED output
    GPIO::inputPullup(PD2);     // button input, pull-up to VCC

    while (1) {
        // Button is active-low: pressed = LOW = false
        GPIO::write(PB5, !GPIO::read(PD2));
    }
}
```

---

### Multiple Outputs on the Same Port

Configure PB0–PB3 as outputs and cycle through them (shift register style).

```cpp
#include <mikroduino/gpio.hpp>
#include <util/delay.h>
using namespace MikroDuino;

int main() {
    GPIO::portOutput(1, 0x0F);  // port B (index 1), lower 4 bits as output

    uint8_t pattern = 0x01;
    while (1) {
        GPIO::portWrite(1, pattern);
        pattern = (pattern << 1) | (pattern >> 7);  // rotate left
        _delay_ms(200);
    }
}
```

---

### Read a Full Port at Once

```cpp
#include <mikroduino/gpio.hpp>
using namespace MikroDuino;

int main() {
    GPIO::portInput(3, 0xFF);   // port D (index 3), all pins as input

    while (1) {
        uint8_t switches = GPIO::portRead(3);
        // switches bit 0 = PD0, bit 1 = PD1, …
        GPIO::portWrite(1, switches);  // mirror on port B
    }
}
```

---

## Notes

**No instance required.**
`GPIO` has a deleted constructor. Calling `GPIO g;` is a compile error.
Always use the static form: `GPIO::output(...)`.

**`toggle()` is atomic.**
It writes to the PINx register (an AVR extension), not PORTx.
This makes it equivalent to a single `SBI` instruction — no interrupts can
corrupt it mid-operation, unlike a read-modify-write on PORTx.

**Pull-up on output pins.**
Calling `GPIO::inputPullup()` on a pin that is still configured as an output
has no safe meaning. Always call `GPIO::input()` or `GPIO::inputPullup()` before
using `GPIO::read()`.

**`portWrite()` writes all 8 bits.**
It overwrites every bit of PORTx, including pins configured as inputs (where
the bit controls the pull-up). Use it only when you own all pins on the port.

**Pin constants are MCU-gated.**
`PA0`–`PA7` and `PE0`–`PG4` are only defined when the target MCU actually has
those ports (controlled by `platform.hpp` flags). Using them on an unsupported
MCU is a compile error.

---

## Quick Reference Card

| Task | Call |
|------|------|
| Set pin as output | `GPIO::output(PBx)` |
| Set pin as floating input | `GPIO::input(PBx)` |
| Set pin as input + pull-up | `GPIO::inputPullup(PBx)` |
| Drive HIGH | `GPIO::set(PBx)` |
| Drive LOW | `GPIO::clear(PBx)` |
| Toggle | `GPIO::toggle(PBx)` |
| Write HIGH or LOW | `GPIO::write(PBx, value)` |
| Read state | `bool b = GPIO::read(PBx)` |
| Configure port bits | `GPIO::portOutput(idx, mask)` |
| Write whole port | `GPIO::portWrite(idx, value)` |
| Read whole port | `uint8_t v = GPIO::portRead(idx)` |
