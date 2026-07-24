# MikroDuino Interrupt Reference

## Overview

An **interrupt** is a hardware signal that pauses whatever the CPU is currently
doing, runs a short function called an **ISR** (Interrupt Service Routine), then
resumes exactly where it left off — all transparently, with no polling required.

The MikroDuino SDK wraps the AVR external interrupt controller through
`MikroDuino::InterruptManager`. It handles the low-level register writes
(EICRA, EIMSK, SREG) so you only deal with pin names, sense modes, and plain
C++ function pointers.

Include:
```cpp
#include <mikroduino/interrupt.hpp>
using namespace MikroDuino;
```

**You must also add `interrupt.cpp` to your project's `sourceFiles`:**
```json
"sourceFiles": [
    "src/main.cpp",
    "../../sdk/core/avr/src/interrupt.cpp"
]
```
`interrupt.cpp` contains the actual `ISR(INT0_vect)` / `ISR(INT1_vect)` bodies
that dispatch to your handler. Without it the linker will report
`undefined reference to _int_handlers::fn0`.

---

## How AVR External Interrupts Work

The ATmega family has dedicated **external interrupt pins** — specific physical
pins wired directly to the interrupt controller hardware. When the signal on one
of these pins meets the configured condition (rising edge, falling edge, etc.),
the hardware:

1. Sets a **flag bit** in the EIFR register (interrupt flag register).
2. If the corresponding **mask bit** in EIMSK is set AND the **global I-bit**
   in SREG is set, the CPU finishes its current instruction.
3. The CPU **saves its state** (pushes the program counter and status register
   onto the stack automatically).
4. Execution jumps to the **interrupt vector** — a fixed address in flash
   reserved for that interrupt source.
5. The vector body (in `interrupt.cpp`) calls your registered handler.
6. The ISR returns, the CPU **restores its state**, and resumes the main loop
   from the exact instruction that was interrupted.

This entire process — from the edge on the pin to the first instruction of your
handler — takes around **4 CPU cycles** (250 ns at 16 MHz).

---

## Interrupt Pins by MCU

Only specific pins are connected to the external interrupt controller.
Connecting your button to any other pin will not trigger an interrupt.

### ATmega328P

| Source | Pin | Port bit |
|--------|-----|----------|
| INT0   | PD2 | Port D, bit 2 |
| INT1   | PD3 | Port D, bit 3 |

### ATmega32 / ATmega16

| Source | Pin | Port bit |
|--------|-----|----------|
| INT0   | PD2 | Port D, bit 2 |
| INT1   | PD3 | Port D, bit 3 |
| INT2   | PB2 | Port B, bit 2 |

### ATmega64 / ATmega128

| Source | Pin  | Port bit |
|--------|------|----------|
| INT0   | PD0  | Port D, bit 0 |
| INT1   | PD1  | Port D, bit 1 |
| INT2   | PD2  | Port D, bit 2 |
| INT3   | PD3  | Port D, bit 3 |
| INT4   | PE4  | Port E, bit 4 |
| INT5   | PE5  | Port E, bit 5 |
| INT6   | PE6  | Port E, bit 6 |
| INT7   | PE7  | Port E, bit 7 |

> Configure the pin as an input (with or without pull-up) using `GPIO::inputPullup()`
> before attaching the interrupt. The interrupt controller reads the physical pin
> state — it ignores DDRx.

---

## Types and Enums

### `IntSource`

Selects which hardware interrupt line to use.

```cpp
enum class IntSource : uint8_t {
    INT0,   // always available
    INT1,   // always available
    INT2,   // ATmega32/16 only
    // INT3–INT7 on ATmega64/128
};
```

### `IntSense`

Controls what signal condition triggers the interrupt.

```cpp
enum class IntSense : uint8_t {
    Low     = 0,  // fires continuously while pin is LOW
    Change  = 1,  // fires on any logic change (LOW→HIGH or HIGH→LOW)
    Falling = 2,  // fires once on HIGH→LOW transition
    Rising  = 3,  // fires once on LOW→HIGH transition
};
```

**Choosing the right sense mode:**

| Wiring | Recommended sense | Reason |
|--------|-------------------|--------|
| Button pulls pin LOW (pull-up) | `Falling` | fires when button is pressed |
| Button pulls pin HIGH (pull-down) | `Rising` | fires when button is pressed |
| Encoder or signal edge detection | `Change` | fires on both edges |
| Level detection (rare) | `Low` | fires repeatedly while held |

`Low` is the only level-triggered mode. It fires as long as the pin stays low,
not just once — without careful masking, it will re-enter the ISR continuously.
Use an edge mode (`Falling`, `Rising`, `Change`) for buttons.

### `ISRHandler`

```cpp
using ISRHandler = void(*)();
```

The handler must be a plain C++ function: no arguments, no return value,
no captures. A `static` member function or a free function both work.
A lambda that captures variables does **not** — it cannot decay to a raw
function pointer.

```cpp
// correct
void myHandler() { ... }
static void MyClass::handler() { ... }

// wrong — lambda with capture cannot be an ISRHandler
Interrupt.attach(IntSource::INT0, [&]() { ... }, IntSense::Falling); // compile error
```

---

## API Reference

### `InterruptManager::attach()`

```cpp
static void attach(IntSource src, ISRHandler handler, IntSense sense = IntSense::Rising);
```

Registers `handler` for interrupt source `src` and arms it. Does three things
in order:

| Step | What | Register written |
|------|------|-----------------|
| 1 | Saves the function pointer | RAM (`_int_handlers::fn0/fn1/fn2`) |
| 2 | Sets the sense mode | `EICRA` (INT0/INT1) or `MCUCSR` (INT2 on ATmega32/16) |
| 3 | Unmasks the source | `EIMSK` |

The interrupt will not actually fire until `enableGlobal()` is also called.

Default sense mode is `Rising` if the third argument is omitted.

---

### `InterruptManager::detach()`

```cpp
static void detach(IntSource src);
```

Masks the source in `EIMSK` and clears the stored handler pointer. After
`detach()`, even if `enableGlobal()` is active, no ISR will fire for this
source. The sense bits in `EICRA` are not modified.

---

### `InterruptManager::enable()` / `disable()`

```cpp
static void enable(IntSource src);
static void disable(IntSource src);
```

Temporarily unmask or mask a single interrupt source by setting/clearing its
bit in `EIMSK`. Unlike `detach()`, these preserve the handler pointer and
sense configuration — calling `enable()` after `disable()` resumes the same
handler without a new `attach()`.

Use these for short critical sections where you need to ignore one source
momentarily without losing its registration.

---

### `InterruptManager::enableGlobal()` / `disableGlobal()`

```cpp
static void enableGlobal();   // executes AVR sei() instruction
static void disableGlobal();  // executes AVR cli() instruction
```

The **global interrupt enable** (I-bit in SREG) is the master switch for all
interrupts. Individual sources can be armed in `EIMSK`, but nothing fires until
`sei()` is called. `cli()` blocks all interrupt sources simultaneously —
useful for protecting multi-step atomic operations.

**Order matters:** always attach and configure all sources *before* calling
`enableGlobal()`. Enabling globally first and then calling `attach()` creates
a window where the source is masked but globals are live — harmless here, but
confusing.

---

## How `interrupt.cpp` Connects to Your Handler

The dispatch chain when an interrupt fires:

```
Hardware edge on PD2
        │
        ▼
AVR jumps to ISR(INT0_vect)     ← defined in interrupt.cpp
        │
        ▼
if (_int_handlers::fn0)         ← fn0 was set by attach()
    fn0();                      ← calls your handler function
        │
        ▼
Your handler runs
        │
        ▼
ISR returns → CPU resumes main loop
```

`_int_handlers::fn0`, `fn1`, and `fn2` are variables shared between
`interrupt.hpp` (where `attach()` writes them) and `interrupt.cpp` (where the
ISR bodies read them). They are declared `extern` in the header and defined
once in `interrupt.cpp` — this is why that file must be compiled as part of
your project.

---

## Writing a Safe ISR Handler

### Keep it short

The CPU is paused for the entire duration of the ISR. Long ISRs cause timing
problems, missed subsequent interrupts, and unresponsive behaviour.

**Pattern: set a flag in the ISR, do the work in the main loop.**

```cpp
static volatile bool g_fired = false;

void onPress() {
    g_fired = true;      // short — just set a flag
}

int main() {
    // ... setup ...
    while (true) {
        if (g_fired) {
            g_fired = false;
            doTheActualWork();   // work done outside the ISR
        }
    }
}
```

For very simple, fast operations (a single `GPIO::toggle()`) it is acceptable
to act directly in the handler.

### Use `volatile` for shared variables

Any variable written by an ISR and read by the main loop (or vice versa) must
be declared `volatile`. Without it the compiler may cache the value in a
register and the main loop will never see the ISR's update.

```cpp
static volatile bool g_fired = false;    // correct
static bool g_fired = false;             // wrong — compiler may cache it
```

### No blocking calls inside the ISR

`_delay_ms()`, `USART::print()`, any function that waits for a condition —
these must never appear inside an ISR. The CPU is already inside an interrupt;
many of those functions depend on other interrupts (UART TX complete, timer
overflow) that cannot fire while you are blocking.

### Atomic multi-byte reads in the main loop

If the ISR writes a value wider than 8 bits (e.g. a `uint16_t` counter), the
main loop must read it atomically. A context switch between the high and low
bytes gives a corrupted value:

```cpp
#include <util/atomic.h>

static volatile uint16_t g_count = 0;

void onTick() { ++g_count; }

int main() {
    uint16_t snapshot;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        snapshot = g_count;   // reads both bytes with interrupts off
    }
    // use snapshot safely
}
```

For `bool` and `uint8_t` (single-byte) values, reads and writes are already
atomic on AVR — no `ATOMIC_BLOCK` needed.

---

## Debouncing

Mechanical buttons bounce — they make and break contact 5–50 times in the first
few milliseconds of a press. Each bounce fires a separate interrupt.

**Hardware debounce (recommended):** place a 100 nF capacitor across the button
(between the INT pin and GND). The capacitor slows the voltage transition below
the bounce frequency and the interrupt fires exactly once.

**Software debounce:** after the ISR fires, ignore further interrupts for a
short window. Implemented by temporarily masking the source:

```cpp
static volatile bool g_pressed = false;

void onPress() {
    Interrupt.disable(IntSource::INT0);  // mask while debouncing
    g_pressed = true;
}

int main() {
    // ... setup ...
    while (true) {
        if (g_pressed) {
            g_pressed = false;
            GPIO::toggle(LED);
            _delay_ms(50);                          // bounce settle time
            Interrupt.enable(IntSource::INT0);      // re-arm
        }
    }
}
```

Do **not** call `_delay_ms()` inside the ISR itself — that is a blocking call
inside an interrupt, which is always wrong.

---

## Examples

### Minimal — Toggle LED on Button Press

```cpp
#include <mikroduino/gpio.hpp>
#include <mikroduino/interrupt.hpp>
using namespace MikroDuino;

static constexpr uint8_t LED    = PB5;
static constexpr uint8_t BUTTON = PD2;   // INT0 pin

void onButtonPress() {
    GPIO::toggle(LED);
}

int main() {
    GPIO::output(LED);
    GPIO::clear(LED);
    GPIO::inputPullup(BUTTON);   // internal pull-up; button wired to GND

    Interrupt.attach(IntSource::INT0, onButtonPress, IntSense::Falling);
    Interrupt.enableGlobal();

    while (true) {}              // CPU idles; ISR does all the work
}
```

---

### Flag Pattern — Work in the Main Loop

```cpp
#include <mikroduino/gpio.hpp>
#include <mikroduino/interrupt.hpp>
#include <util/delay.h>
using namespace MikroDuino;

static constexpr uint8_t LED    = PB5;
static constexpr uint8_t BUTTON = PD2;

static volatile bool g_pressed = false;

void onPress() { g_pressed = true; }

int main() {
    GPIO::output(LED);
    GPIO::clear(LED);
    GPIO::inputPullup(BUTTON);

    Interrupt.attach(IntSource::INT0, onPress, IntSense::Falling);
    Interrupt.enableGlobal();

    while (true) {
        if (g_pressed) {
            g_pressed = false;
            GPIO::toggle(LED);
            _delay_ms(50);       // simple software debounce
        }
    }
}
```

---

### Two Buttons — Up/Down Counter on LEDs

```cpp
#include <mikroduino/gpio.hpp>
#include <mikroduino/interrupt.hpp>
#include <util/delay.h>
using namespace MikroDuino;

static constexpr uint8_t BTN_UP   = PD2;   // INT0
static constexpr uint8_t BTN_DOWN = PD3;   // INT1
static constexpr uint8_t LEDS[4]  = { PB0, PB1, PB2, PB3 };

static volatile bool g_up   = false;
static volatile bool g_down = false;

void onUp()   { g_up   = true; }
void onDown() { g_down = true; }

static void showCount(uint8_t val) {
    for (uint8_t i = 0; i < 4; ++i)
        GPIO::write(LEDS[i], static_cast<bool>(val & (1u << i)));
}

int main() {
    GPIO::inputPullup(BTN_UP);
    GPIO::inputPullup(BTN_DOWN);
    for (uint8_t i = 0; i < 4; ++i) GPIO::output(LEDS[i]);

    uint8_t count = 8;
    showCount(count);

    Interrupt.attach(IntSource::INT0, onUp,   IntSense::Falling);
    Interrupt.attach(IntSource::INT1, onDown, IntSense::Falling);
    Interrupt.enableGlobal();

    while (true) {
        if (g_up)   { g_up   = false; if (count < 15) showCount(++count); _delay_ms(30); }
        if (g_down) { g_down = false; if (count >  0) showCount(--count); _delay_ms(30); }
    }
}
```

---

### Temporary Masking — Ignore Interrupts During a Critical Section

```cpp
#include <mikroduino/interrupt.hpp>
using namespace MikroDuino;

void criticalOperation() {
    Interrupt.disable(IntSource::INT0);   // mask INT0 only; INT1 still active

    // ... multi-step operation that must not be interrupted by INT0 ...

    Interrupt.enable(IntSource::INT0);    // restore — handler still registered
}
```

---

### Change Sense Mode at Runtime

```cpp
// Start detecting falling edges, switch to both-edges later
Interrupt.attach(IntSource::INT0, myHandler, IntSense::Falling);
Interrupt.enableGlobal();

// ... later in the program ...

Interrupt.detach(IntSource::INT0);                              // disarm first
Interrupt.attach(IntSource::INT0, myHandler, IntSense::Change); // re-arm with new sense
```

Always `detach()` before changing the sense mode. Calling `attach()` while the
source is live re-writes EICRA while interrupts are enabled, which could cause
a spurious trigger on some AVR silicon.

---

## Register Map (ATmega328P)

The SDK manages these registers for you. This table is for reference when
reading the datasheet alongside the SDK source.

| Register | Purpose |
|----------|---------|
| `EICRA`  | External Interrupt Control A — ISC bits set sense mode for INT0 and INT1 |
| `EIMSK`  | External Interrupt Mask — one bit per source; must be set to allow firing |
| `EIFR`   | External Interrupt Flag — set by hardware when an edge is detected; cleared on ISR entry |
| `SREG`   | Status Register — bit I is the global interrupt enable (`sei()`/`cli()`) |

EICRA bit layout for INT0 and INT1:

```
EICRA:  [ ISC11 | ISC10 | ISC01 | ISC00 | — | — | — | — ]
         \_____INT1_____/ \_____INT0_____/
```

| ISCx1 | ISCx0 | Sense |
|-------|-------|-------|
| 0     | 0     | Low level |
| 0     | 1     | Any change |
| 1     | 0     | Falling edge |
| 1     | 1     | Rising edge |

---

## Common Mistakes

**Forgetting `interrupt.cpp` in `sourceFiles`**
Results in `undefined reference to _int_handlers::fn0` at link time.
The ISR vector bodies live in `interrupt.cpp` — it must be compiled.

**Forgetting `enableGlobal()`**
`attach()` arms the source in EIMSK but the CPU ignores all interrupts until
`sei()` sets the I-bit. If the handler never fires, verify `enableGlobal()` is
called.

**Forgetting `volatile` on shared variables**
The compiler does not know that an ISR can modify a variable between two
consecutive reads in the main loop. Without `volatile`, it caches the value and
the main loop stalls forever waiting for a change that already happened.

**Blocking inside the ISR**
Calling `_delay_ms()`, `USART::print()`, or any wait-loop inside a handler
locks up the CPU. Do the minimum work in the ISR (set a flag) and handle it in
the main loop.

**Using the wrong pin**
Only the dedicated INT pins trigger external interrupts. Wiring your button to
PB3 instead of PD2 (INT0) produces no interrupt regardless of configuration.
Check the pin table for your MCU.

**Lambda with captures as handler**
`ISRHandler` is `void(*)()` — a plain function pointer. Lambdas that capture
variables are objects, not raw pointers. Pass a plain `static` function or a
free function instead.

**Sense mode `Low` without masking**
`IntSense::Low` fires continuously while the pin is held low — not just once.
Without disabling the source inside the handler, the ISR will re-enter itself
thousands of times per second while the button is held. Use `Falling` for
buttons.

---

## Quick Reference Card

| Task | Call |
|------|------|
| Attach handler (falling edge) | `Interrupt.attach(IntSource::INT0, fn, IntSense::Falling)` |
| Attach handler (rising edge) | `Interrupt.attach(IntSource::INT0, fn, IntSense::Rising)` |
| Attach handler (both edges) | `Interrupt.attach(IntSource::INT0, fn, IntSense::Change)` |
| Remove handler | `Interrupt.detach(IntSource::INT0)` |
| Temporarily mask one source | `Interrupt.disable(IntSource::INT0)` |
| Re-enable one source | `Interrupt.enable(IntSource::INT0)` |
| Enable all interrupts | `Interrupt.enableGlobal()` |
| Disable all interrupts | `Interrupt.disableGlobal()` |
| Shared variable declaration | `static volatile bool g_flag = false;` |
| Atomic multi-byte read | `ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { ... }` |
