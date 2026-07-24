# MikroDuino Register Access Reference

## Overview

`registers.hpp` is the lowest layer of the MikroDuino SDK. It provides a thin set
of macros that map directly onto AVR hardware instructions — no abstraction, no
overhead, no hidden state. Every other SDK module is built on top of these macros.

Include:
```cpp
#include <mikroduino/registers.hpp>
// — or —
#include <mikroduino/mikroduino.hpp>   // entire SDK
```

`registers.hpp` does not pull in any driver or peripheral header. It only requires
`<avr/io.h>` and `<stdint.h>`.

---

## Why use these macros instead of raw C?

| Raw C | registers.hpp | Advantage |
|-------|--------------|-----------|
| `PORTB \|= (1 << PB5)` | `BITSET(PORTB, PB5)` | Reads as a verb — set, clear, toggle |
| `PORTB &= ~(1 << PB5)` | `BITCLEAR(PORTB, PB5)` | No chance of writing `\|=` when you meant `&= ~` |
| `(PINB >> PB2) & 1` | `BITREAD(PINB, PB2)` | Returns 0 or 1 — no unexpected multi-bit result |
| `r = (r & ~mask) \| ((v << s) & mask)` | `FIELD_SET(r, mask, s, v)` | Named operation; the pattern is error-prone to write manually |

All macros expand to standard C expressions. The compiler sees through them and
generates the same `SBI`, `CBI`, `IN`, `OUT`, or `LDS`/`STS` instructions it
would for hand-written register access.

---

## Register Read / Write

### `REG(r)`

```cpp
REG(UDR0) = c;
uint8_t val = REG(PINB);
```

Identity macro — expands to `(r)`. Use it when you want to make a raw register
access visually distinct from an ordinary variable read or write. It carries no
runtime cost and generates no extra instructions.

---

## Bit Manipulation

These are the most frequently used macros. Each operates on a single bit within
a register.

### `BITSET(reg, bit)`

```cpp
BITSET(DDRB, PB5);      // configure PB5 as output
BITSET(ADCSRA, ADSC);   // start ADC conversion
```

Sets bit `bit` in register `reg` to 1, leaving all other bits untouched.

Expands to: `(reg) |= (1U << (bit))`

On ATmega328P the compiler often emits a single `SBI` instruction (1 cycle, atomic).

---

### `BITCLEAR(reg, bit)`

```cpp
BITCLEAR(PORTB, PB4);   // drive PB4 low
BITCLEAR(DDRD, PD2);    // configure PD2 as input
```

Clears bit `bit` in register `reg` to 0, leaving all other bits untouched.

Expands to: `(reg) &= ~(1U << (bit))`

Compiles to a single `CBI` instruction for I/O registers in the low address range.

---

### `BITTOGGLE(reg, bit)`

```cpp
BITTOGGLE(PORTB, PB5);   // flip LED state
```

Flips bit `bit` without reading the current state first.

Expands to: `(reg) ^= (1U << (bit))`

On AVR I/O ports you can also toggle by writing 1 to the corresponding PINx bit
(a hardware toggle trick), but `BITTOGGLE` on PORTx is universally portable.

---

### `BITREAD(reg, bit)`

```cpp
if (BITREAD(PIND, PD2)) { ... }   // true when PD2 is high

uint8_t ready = BITREAD(UCSR0A, UDRE0);   // 0 or 1
```

Returns `1` if the bit is set, `0` if clear. The result is always 0 or 1 —
never a multi-bit value, unlike the common `(reg & (1 << bit))` idiom which
returns the bit shifted to its original position.

Expands to: `(((reg) >> (bit)) & 1U)`

---

### `BITWRITE(reg, bit, value)`

```cpp
BITWRITE(PORTB, PB5, btn_pressed);   // mirror a boolean to a pin
BITWRITE(TCCR1A, COM1A1, enable);    // conditionally enable compare output
```

Sets the bit if `value` is non-zero, clears it otherwise. Equivalent to an
`if/else` between `BITSET` and `BITCLEAR` but written as a single expression.

Expands to: `((value) ? BITSET(reg, bit) : BITCLEAR(reg, bit))`

---

## Mask Operations

Mask macros operate on **multiple bits at once** using a bitmask. Use them when
you need to change a group of bits in a single atomic operation, or when the bits
are not contiguous.

### `SETMASK(reg, mask)`

```cpp
SETMASK(UCSR0B, (1 << TXEN0) | (1 << RXEN0));   // enable TX and RX together
SETMASK(ADCSRA, (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0));
```

OR the mask into the register. All bits set in `mask` become 1; all bits clear
in `mask` are left untouched.

Expands to: `(reg) |= (mask)`

---

### `CLEARMASK(reg, mask)`

```cpp
CLEARMASK(UCSR0C, (1 << UPM01) | (1 << UPM00));   // clear parity bits
CLEARMASK(ADMUX,  (1 << MUX3)  | (1 << MUX2) | (1 << MUX1) | (1 << MUX0));
```

AND the complement of the mask into the register. All bits set in `mask` become
0; all bits clear in `mask` are left untouched.

Expands to: `(reg) &= ~(mask)`

---

### `TOGGLEMASK(reg, mask)`

```cpp
TOGGLEMASK(PORTB, (1 << PB5) | (1 << PB4));   // flip two LEDs simultaneously
```

XOR the mask into the register. Every bit that is 1 in `mask` is flipped; bits
that are 0 in `mask` are left untouched.

Expands to: `(reg) ^= (mask)`

**Key difference from two `BITTOGGLE` calls:** both bits change in the same write
cycle. There is no intermediate state where one pin has changed and the other has
not — important for H-bridges, LED matrices, and any output where partial updates
are visible or dangerous.

---

### `READMASK(reg, mask)`

```cpp
uint8_t nibble = READMASK(PINB, 0x0F);                    // lower 4 bits
uint8_t leds   = READMASK(PORTB, (1 << PB5) | (1 << PB4)); // just the LED bits
```

AND the register with the mask and return the result. The returned value is in
the same bit positions as the original register — not right-shifted.

Expands to: `((reg) & (mask))`

To test whether any masked bit is set: `if (READMASK(PINB, 0x0F)) { ... }`
To test whether a specific combination is set: `if (READMASK(PINB, 0x0F) == 0x05) { ... }`

---

## Polling

Busy-wait loops for hardware flags. Both macros compile to a tight 2-instruction
loop (IN + SBIS/SBIC + RJMP) on I/O registers.

### `WAIT_BITSET(reg, bit)`

```cpp
WAIT_BITSET(UCSR0A, UDRE0);   // wait until USART TX buffer is empty
WAIT_BITSET(SPSR, SPIF);      // wait until SPI transfer complete
```

Spins until `bit` in `reg` becomes 1 (set). Use when hardware sets the flag to
signal readiness or completion.

Expands to: `do { } while (!BITREAD(reg, bit))`

---

### `WAIT_BITCLEAR(reg, bit)`

```cpp
WAIT_BITCLEAR(ADCSRA, ADSC);   // wait until ADC conversion done (ADSC clears)
WAIT_BITCLEAR(TWCR, TWSTO);    // wait until I2C STOP condition sent
```

Spins until `bit` in `reg` becomes 0 (clear). Use when hardware clears the flag
to signal completion — the inverse of `WAIT_BITSET`.

Expands to: `do { } while (BITREAD(reg, bit))`

> **Note**: both WAIT macros are blocking busy-loops. They hold the CPU until the
> condition is met. Never use them inside an ISR on a flag that requires another
> ISR to fire (deadlock). For non-blocking operation, check the flag manually in
> your main loop using `BITREAD`.

---

## Field Operations

Multi-bit fields — prescaler selectors, reference selectors, waveform mode bits —
are common in AVR control registers. The field macros handle the read-modify-write
safely without touching surrounding bits.

### `FIELD_SET(reg, mask, shift, value)`

```cpp
// Set Timer0 WGM bits (bits [1:0] of TCCR0A) to CTC mode (0b10)
FIELD_SET(TCCR0A, 0x03, WGM00, 0b10);

// Set Timer0 clock prescaler (bits [2:0] of TCCR0B) to /64 (0b011)
FIELD_SET(TCCR0B, 0x07, CS00, 0b011);
```

| Parameter | Meaning |
|-----------|---------|
| `reg` | The register to modify |
| `mask` | Bitmask covering all bits of the field (in their register position) |
| `shift` | Bit position of the field's LSB (often the name of the lowest bit) |
| `value` | New value to place in the field (unshifted — the macro shifts it) |

Expands to:
```cpp
(reg) = ((reg) & ~(mask)) | (((value) << (shift)) & (mask))
```

Steps:
1. Clear all bits covered by `mask` in `reg`.
2. Shift `value` left by `shift` so it aligns with the field.
3. AND with `mask` to clamp any overflow.
4. OR the result back in.

All other bits in `reg` are preserved.

---

### `FIELD_GET(reg, mask, shift)`

```cpp
uint8_t cs  = FIELD_GET(TCCR0B, 0x07, CS00);    // read prescaler field
uint8_t wgm = FIELD_GET(TCCR0A, 0x03, WGM00);   // read waveform mode
```

Extracts the field and returns it right-shifted so the LSB is at bit 0.

Expands to: `(((reg) & (mask)) >> (shift))`

Use it to read back a field you just wrote (startup verification), or to inspect
hardware-modified fields (e.g., ADC multiplexer selection).

---

### Field example — Timer0 at 1 ms

```cpp
// CTC mode: WGM01:WGM00 = 0b10 in TCCR0A[1:0]
FIELD_SET(TCCR0A, 0x03, WGM00, 0b10);
OCR0A = 249;                              // TOP for 1 ms at /64

// Start clock: CS02:CS00 = 0b011 (/64) in TCCR0B[2:0]
FIELD_SET(TCCR0B, 0x07, CS00, 0b011);

// Verify
uint8_t cs = FIELD_GET(TCCR0B, 0x07, CS00);   // reads back 3 (= 0b011 = /64)
```

---

## Atomic Block

The AVR is an 8-bit CPU. Reading or writing 16-bit values (e.g., `TCNT1`, `ADC`,
a `volatile uint16_t` shared with an ISR) is not atomic — it takes two bus cycles.
An interrupt that fires between the two byte reads can corrupt the value.

### `ATOMIC_BLOCK_START` / `ATOMIC_BLOCK_END`

```cpp
uint16_t t;
ATOMIC_BLOCK_START;
t = g_ms;           // 16-bit read — safe from ISR corruption
ATOMIC_BLOCK_END;
```

`ATOMIC_BLOCK_START` saves the current `SREG` (Status Register, which contains
the global interrupt enable flag `I`) to a local variable, then executes `CLI`
(clear interrupt enable). `ATOMIC_BLOCK_END` restores `SREG` from that saved
copy — re-enabling interrupts only if they were enabled before the block.

Expands to:
```cpp
// ATOMIC_BLOCK_START:
uint8_t _sreg_save = SREG;
__asm__ volatile ("cli" ::: "memory");

// ... your critical section ...

// ATOMIC_BLOCK_END:
SREG = _sreg_save;
__asm__ volatile ("" ::: "memory");
```

The `"memory"` clobber forces the compiler to flush all cached register values
to memory before the CLI and reload them after the restore — preventing the
compiler from hoisting reads/writes out of the critical section.

> **Rule**: keep atomic blocks as short as possible. Every cycle spent inside a
> block with interrupts disabled adds to ISR latency. Never call a function that
> blocks (WAIT_BITSET, _delay_ms) inside an atomic block.

---

## Compiler Hints

These are GCC `__attribute__` wrappers. They affect code generation and binary
size but have no runtime cost.

### `MD_INLINE`

```cpp
static MD_INLINE void usart_putc(uint8_t c) { ... }
```

Expands to: `__attribute__((always_inline)) inline`

Forces the compiler to inline the function at every call site, even at `-Os`
(size-optimised) when the compiler would normally refuse. Use on small, hot-path
functions where the call/ret overhead is significant relative to the function body.

---

### `MD_NOINLINE`

```cpp
static MD_NOINLINE void usart_puts(const char* s) { ... }
```

Expands to: `__attribute__((noinline))`

Prevents the compiler from inlining the function. Use on functions called from
multiple sites — inlining a 30-instruction function 10 times wastes 270 bytes
of flash versus one copy plus 10 two-instruction `CALL`/`RET` pairs.

---

### `MD_NORETURN`

```cpp
MD_NORETURN void run(void);
```

Expands to: `__attribute__((noreturn))`

Tells the compiler the function never returns (it either loops forever or calls
`__builtin_unreachable()`). The compiler skips generating the dead `ret` instruction
after the call site and suppresses "control reaches end of non-void function" warnings.
Useful on `main()`-equivalent entry points that contain an infinite loop.

---

### `MD_UNUSED`

```cpp
MD_UNUSED uint8_t diagnostic = READMASK(PINB, 0x0F);
```

Expands to: `__attribute__((unused))`

Suppresses the "unused variable" or "unused function" warning for something that
is intentionally not used in all code paths — for example, a variable read for
its side effect, a debug helper compiled out by a flag, or a function declared
for a future driver.

---

### `MD_PACKED`

```cpp
struct MD_PACKED Frame {
    uint8_t  id;
    uint16_t payload;
    uint8_t  crc;
};
// sizeof(Frame) == 4, not 6 (no padding inserted by compiler)
```

Expands to: `__attribute__((packed))`

Instructs the compiler to remove all padding bytes from a struct. Use when the
struct must map exactly onto a hardware register block, a communication protocol
frame, or a memory-mapped peripheral layout where padding would misalign fields.

> **Warning**: reading a `uint16_t` member of a packed struct on an architecture
> that requires word alignment causes undefined behaviour (or a fault on strict
> platforms). On AVR (no alignment requirement) this is safe.

---

## Quick Reference

### Bit macros

| Macro | Expands to | Use for |
|-------|-----------|---------|
| `BITSET(r, b)` | `r \|= (1U << b)` | Set one bit to 1 |
| `BITCLEAR(r, b)` | `r &= ~(1U << b)` | Set one bit to 0 |
| `BITTOGGLE(r, b)` | `r ^= (1U << b)` | Flip one bit |
| `BITREAD(r, b)` | `(r >> b) & 1U` | Read one bit (returns 0 or 1) |
| `BITWRITE(r, b, v)` | `BITSET` or `BITCLEAR` | Conditional bit write |

### Mask macros

| Macro | Expands to | Use for |
|-------|-----------|---------|
| `SETMASK(r, m)` | `r \|= m` | Set multiple bits at once |
| `CLEARMASK(r, m)` | `r &= ~m` | Clear multiple bits at once |
| `TOGGLEMASK(r, m)` | `r ^= m` | Flip multiple bits simultaneously |
| `READMASK(r, m)` | `r & m` | Extract a group of bits |

### Field macros

| Macro | Use for |
|-------|---------|
| `FIELD_SET(r, mask, shift, val)` | Write a multi-bit field |
| `FIELD_GET(r, mask, shift)` | Read a multi-bit field (right-shifted to bit 0) |

### Poll macros

| Macro | Loops until |
|-------|------------|
| `WAIT_BITSET(r, b)` | bit `b` in `r` becomes 1 |
| `WAIT_BITCLEAR(r, b)` | bit `b` in `r` becomes 0 |

### Atomic macros

| Macro | Action |
|-------|--------|
| `ATOMIC_BLOCK_START` | Save SREG, disable interrupts (CLI) |
| `ATOMIC_BLOCK_END` | Restore SREG (re-enables interrupts if they were on) |

### Compiler hint attributes

| Macro | GCC attribute | Effect |
|-------|--------------|--------|
| `MD_INLINE` | `always_inline` | Force inlining at every call site |
| `MD_NOINLINE` | `noinline` | Prevent inlining — one copy in flash |
| `MD_NORETURN` | `noreturn` | Function never returns — omit dead `ret` |
| `MD_UNUSED` | `unused` | Suppress unused variable/function warning |
| `MD_PACKED` | `packed` | Remove struct padding bytes |
