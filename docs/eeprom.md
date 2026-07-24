# EEPROM — MikroDuino SDK Reference

**Header:** `sdk/core/avr/include/mikroduino/eeprom.hpp`  
**Source:** `sdk/core/avr/src/eeprom.cpp`  
**Namespace:** `MikroDuino`  
**Global instance:** `EEPROM`

---

## Table of Contents

1. [Hardware Overview](#1-hardware-overview)
2. [Internal Architecture](#2-internal-architecture)
3. [Register Map](#3-register-map)
4. [Enumerations](#4-enumerations)
5. [Global Instance](#5-global-instance)
6. [API Reference](#6-api-reference)
7. [Write Cycle Timing](#7-write-cycle-timing)
8. [Usage Patterns](#8-usage-patterns)
9. [MCU Differences](#9-mcu-differences)
10. [Common Pitfalls](#10-common-pitfalls)

---

## 1. Hardware Overview

The AVR internal EEPROM (Electrically Erasable Programmable Read-Only Memory) is a non-volatile byte-addressable memory array that retains data when power is removed. It is independent of Flash program memory and SRAM.

Key properties:

| Property | Value |
|---|---|
| Cell type | Floating-gate NVM (same family as Flash) |
| Access width | 8 bits (byte-addressed) |
| Factory erased state | `0xFF` (all bits 1) |
| Default write mode | Atomic erase + write (single operation) |
| Endurance | 100,000 write/erase cycles minimum |
| Data retention | 20 years at 25 °C (per datasheet) |
| Write time (erase+write) | ~3.4 ms |
| Write time (erase-only or write-only) | ~1.8 ms each |
| Read time | 1 CPU cycle after asserting EERE |
| Read blocks CPU | Yes — EERE stalls the pipeline for the read cycle |

**EEPROM sizes by MCU:**

| MCU | Size | Address range |
|---|---|---|
| ATmega16 | 512 bytes | `0x000`–`0x1FF` |
| ATmega328P | 1024 bytes | `0x000`–`0x3FF` |
| ATmega32 | 1024 bytes | `0x000`–`0x3FF` |
| ATmega64 | 2048 bytes | `0x000`–`0x7FF` |
| ATmega128 | 4096 bytes | `0x000`–`0xFFF` |

The SDK exposes the size for the current target via `EEPROM.capacity()`, which reads the `MD_EEPROM_SIZE` macro from `platform.hpp`.

---

## 2. Internal Architecture

### NVM Cell and Erase State

Each EEPROM cell stores one bit as charge on a floating gate. The erased state (all charge removed by high-voltage erase pulse) reads as logic **1**. A programmed cell (charge injected) reads as logic **0**. Because of this:

- Erasing a byte writes `0xFF` (all bits 1).
- Writing can only turn 1s into 0s in a single pass — turning a 0 back to a 1 requires an erase first.
- The default atomic write mode (erase + write) handles this automatically in one operation.
- The split `EraseOnly` + `WriteOnly` mode (ATmega328P) lets you erase a cell first, then write it separately at 1.8 ms each. Total is 3.6 ms — slightly slower than the atomic 3.4 ms, but useful for bulk erase followed by sequential writes.

### Write Sequencer

The AVR EEPROM controller requires a precise four-step sequence enforced in hardware:

```
Step 1: Write address to EEAR
Step 2: Write data to EEDR
Step 3: Set EEMPE (Master Program Enable) — opens a 4-cycle write window
Step 4: Set EEPE (Program Enable) within 4 CPU cycles of step 3 — starts the write
```

Steps 3 and 4 must happen within 4 CPU cycles. This is why the SDK disables interrupts using `ATOMIC_BLOCK_START` / `ATOMIC_BLOCK_END` around them — an interrupt between steps 3 and 4 would cause the write window to expire and the operation would silently not happen.

```
            ┌─────────────────────────────────────────────────────────┐
            │                   AVR EEPROM Controller                 │
            │                                                         │
  EEAR ─────┤──→ 12-bit address latch ──→ Row/Column decoder          │
  EEDR ─────┤──→ 8-bit data latch     ──→ Sense amplifiers ←── NVM   │
  EECR ─────┤──→ Control logic                                        │
            │         │                                               │
            │    ┌────┴─────────────────────────────────────────┐    │
            │    │  Write sequencer:                             │    │
            │    │  EEMPE=1 → 4-cycle window opens              │    │
            │    │  EEPE=1  → charge pump starts (~3.4 ms)      │    │
            │    │  EEPE=0  → write complete, EERIE fires        │    │
            │    └──────────────────────────────────────────────┘    │
            │                                                         │
            │  Read: EERE=1 → data driven to EEDR in 1 CPU cycle     │
            └─────────────────────────────────────────────────────────┘
```

### Charge Pump

Erasing and programming EEPROM cells requires voltages higher than VCC (typically 8–12 V internally). The AVR contains an on-chip charge pump that generates this voltage when a write cycle starts. The 3.4 ms write time is dominated by this charge pump operation, not by software or bus speed.

### SPM Interaction

On devices with a bootloader section, the Self-Programming (SPM) instruction also uses the charge pump. If an SPM instruction is in progress when an EEPROM write is attempted, the EEPROM controller will malfunction. The SDK guards against this by waiting for `SPMCSR.SPMEN` (or `SPMCR.SPMEN`) to clear before starting a write.

---

## 3. Register Map

### EEAR — EEPROM Address Register

On devices with more than 256 bytes of EEPROM, EEAR is a 16-bit register composed of `EEARH` (high byte) and `EEARL` (low byte). The avr-libc `EEAR` macro accesses both as a 16-bit unit.

```
  Bits 15:12 — reserved (always 0)
  Bits 11:0  — EEPROM address (0 to capacity-1)
```

Only bits within the valid address range are used. Writing an out-of-range address wraps in an undefined way — the SDK does not perform bounds checking.

### EEDR — EEPROM Data Register

```
  Bit:  7    6    5    4    3    2    1    0
       D7   D6   D5   D4   D3   D2   D1   D0
```

For a **read**: hardware writes the byte at EEAR into EEDR when EERE is asserted. Software reads EEDR after EERE is set.

For a **write**: software writes the byte to EEDR before the write sequence. Hardware programs the NVM cell from EEDR when EEPE fires.

### EECR — EEPROM Control Register

```
  Bit:   7    6    5     4     3     2     1     0
        —    —   EEPM1 EEPM0 EERIE EEMPE  EEPE  EERE
```

| Bit(s) | Name | Description |
|---|---|---|
| 7:6 | — | Reserved. Always read as 0. Do not write 1. |
| 5:4 | `EEPM1:EEPM0` | Programming mode — **ATmega328P only** (see [Section 4](#4-enumerations)). Reserved on other MCUs; write 0. |
| 3 | `EERIE` | EEPROM Ready Interrupt Enable. When 1 and global interrupts are enabled, fires `EE_READY_vect` whenever EEPE is 0 (EEPROM idle). |
| 2 | `EEMPE` | Master Program Enable. Must be set to 1 before EEPE. Hardware clears it automatically after 4 CPU cycles if EEPE is not set in time. |
| 1 | `EEPE` | Program Enable. Set 1 within 4 cycles of EEMPE=1 to start a write. Hardware clears it when the write cycle completes (~3.4 ms). Polling `EEPE == 0` is how `busy()` and `waitReady()` detect completion. |
| 0 | `EERE` | Read Enable. Write 1 to start a read. Hardware immediately places the byte from EEAR into EEDR and clears EERE. |

> **Register name compatibility:** Older AVR families (ATmega32, ATmega16) use `EEWE` and `EEMWE` instead of `EEPE` and `EEMPE`. Modern avr-libc defines `EEPE`/`EEMPE` as aliases on those devices. The SDK also defines a `#ifndef EEPE` / `#define EEPE EEWE` guard for toolchains where the alias is absent.

---

## 4. Enumerations

### `EEPROMMode`

Controls how the NVM cell is programmed during a write cycle. Available **only on ATmega328P** (and devices that implement `EEPM1:EEPM0` in EECR). On other MCUs the bits are reserved and the atomic mode is always used.

```cpp
#if defined(EEPM0) && defined(EEPM1)
enum class EEPROMMode : uint8_t {
    EraseWrite = 0b00,  // Atomic erase+write (default, ~3.4 ms)
    EraseOnly  = 0b01,  // Erase cell to 0xFF only (~1.8 ms)
    WriteOnly  = 0b10,  // Write without erase (~1.8 ms) — cell must be 0xFF
};
#endif
```

| Mode | EEPM1:EEPM0 | Time | Use case |
|---|---|---|---|
| `EraseWrite` | `00` | ~3.4 ms | Default — always safe regardless of current cell value |
| `EraseOnly` | `01` | ~1.8 ms | Bulk erase a region before a WriteOnly pass |
| `WriteOnly` | `10` | ~1.8 ms | Write to a cell already known to be `0xFF` |

**Total split time:** `EraseOnly` + `WriteOnly` = ~3.6 ms — marginally slower than atomic mode. The practical benefit is flexibility: you can erase a whole block in one pass, then write each byte individually, rather than taking the full 3.4 ms penalty per byte.

> `WriteOnly` applied to a cell that is not `0xFF` produces undefined data — bits can only be turned from 1→0, not 0→1, without an erase. Always erase first.

---

## 5. Global Instance

```cpp
static EEPROMDriver EEPROM;   // defined at file scope in eeprom.hpp
```

`EEPROM` is a zero-size convenience token — the class has no data members and all methods are static. Its only purpose is to allow `EEPROM.read(addr)` syntax alongside the equivalent `EEPROMDriver::read(addr)`.

```cpp
#include <mikroduino/eeprom.hpp>
using namespace MikroDuino;

// All three forms are identical in generated code:
uint8_t a = EEPROM.read(0x10);
uint8_t b = EEPROMDriver::read(0x10);

#include <mikroduino/mikroduino.hpp>   // master header includes eeprom.hpp
```

---

## 6. API Reference

---

### `capacity()`

```cpp
static constexpr uint16_t capacity();
```

Returns the EEPROM size in bytes for the current compilation target. Resolved at compile time from `MD_EEPROM_SIZE` in `platform.hpp`. No runtime cost.

**Returns:** `uint16_t` — 512, 1024, 2048, or 4096 depending on MCU.

**Example:**

```cpp
// Bounds-check before write
if (addr < EEPROM.capacity()) {
    EEPROM.write(addr, data);
}
```

---

### `busy()`

```cpp
static bool busy();
```

Returns `true` if an EEPROM write cycle is currently in progress (EEPE bit is set in EECR).

**Hardware effect:**

```cpp
return (EECR >> EEPE) & 1;
```

**Notes:**

- Returns `false` during a read — reads complete in 1 CPU cycle and EERE is never held.
- A write started by `write()` or `_write()` returns immediately after setting EEPE. The hardware write takes ~3.4 ms. Calling `busy()` immediately after `write()` will almost always return `true`.
- Useful for non-blocking write patterns: start a write, return to other work, poll `busy()` periodically, and read back only after it returns `false`.

**Example:**

```cpp
EEPROM.write(0x00, 0xAB);           // starts write, returns immediately
while (EEPROM.busy()) {
    do_something_else();             // non-blocking wait
}
uint8_t v = EEPROM.read(0x00);      // now safe
```

---

### `waitReady()`

```cpp
static void waitReady();
```

Spins until the EEPE bit clears (write cycle complete). If no write is in progress, it returns immediately.

**Hardware effect:**

```cpp
while (EECR & (1 << EEPE));
```

**Notes:**

- Every `read()`, `write()`, and `_write()` call invokes `waitReady()` internally before touching the registers. You do not need to call it manually before read/write operations.
- Use it explicitly when you need a synchronization fence — for example, before going to sleep, before disabling the clock, or before flashing the program memory.
- At 16 MHz, the spin loop runs approximately 54,400 iterations over a 3.4 ms write cycle (~16 cycles per iteration).

---

### `read()`

```cpp
static uint8_t read(uint16_t addr);
```

Reads one byte from EEPROM address `addr`.

**Parameters:**

| Parameter | Description |
|---|---|
| `addr` | EEPROM address, 0 to `capacity() - 1` |

**Returns:** `uint8_t` — the stored byte value. Returns `0xFF` for factory-erased or explicitly erased cells.

**Hardware sequence:**

```cpp
// 1. Wait for any pending write to finish
while (EECR & (1 << EEPE));

// 2. Load address
EEAR = addr;

// 3. Assert read enable — data appears in EEDR within 1 CPU cycle
EECR |= (1 << EERE);

// 4. Return data register
return EEDR;
```

**Notes:**

- Reading is non-destructive. A cell can be read an unlimited number of times.
- Reading stalls the CPU pipeline for 1 cycle after `EERE` is set (the hardware drives EEDR synchronously). This is invisible to software.
- `read()` calls `waitReady()` first — it is safe to call immediately after a `write()` with no intervening delay.

---

### `write()`

```cpp
static void write(uint16_t addr, uint8_t data);
```

Writes one byte to EEPROM address `addr`, performing a full erase+write cycle regardless of the current stored value. Returns after starting the write — the ~3.4 ms hardware cycle runs in the background.

**Parameters:**

| Parameter | Description |
|---|---|
| `addr` | EEPROM address, 0 to `capacity() - 1` |
| `data` | Byte to write |

**Hardware sequence (inside `_write()`):**

```cpp
// 1. Wait for previous write to complete
while (EECR & (1 << EEPE));

// 2. Wait for any in-progress SPM instruction
while (SPMCSR & (1 << SPMEN));   // omitted on devices without boot section

// 3. Load address and data
EEAR = addr;
EEDR = data;

// 4. Atomic: EEMPE=1, then EEPE=1 within 4 CPU cycles
cli();                            // disable interrupts to guarantee timing
EECR |= (1 << EEMPE);
EECR |= (1 << EEPE);             // starts write; EEPE stays 1 for ~3.4 ms
sei();                            // (restores SREG, not unconditional sei())
```

**Notes:**

- **Always incurs a write cycle** — even if the stored byte already equals `data`. Each cell is rated for 100,000 cycles minimum. Use `update()` instead when writing data that may not have changed.
- Returns before the write completes. The next call to `read()`, `write()`, or any other method will call `waitReady()` and block until the hardware finishes.
- `busy()` returns `true` immediately after this call in virtually all cases (EEPE is set).
- Writing `0xFF` still incurs a full write cycle — it is not equivalent to a free "erase." Use `erase()` by name to make intent clear, but know it costs the same as `write()`.

---

### `update()`  *(byte overload)*

```cpp
static void update(uint16_t addr, uint8_t data);
```

Writes `data` to `addr` **only if the stored byte differs**. If the stored value already equals `data`, the function returns without touching the NVM array.

**Hardware sequence:**

```cpp
if (read(addr) != data) _write(addr, data);
```

**Notes:**

- This is the primary endurance-preservation technique. In code that frequently saves the same value (e.g. a settings struct that rarely changes), replacing `write()` with `update()` can extend cell lifetime by orders of magnitude.
- `busy()` returns `false` immediately after `update()` when the value matched (no write was started). This provides a reliable way to observe whether a write actually occurred.
- The read adds one extra EEPROM access (~1 CPU cycle), which is negligible compared to the 3.4 ms write penalty it avoids.

---

### `erase()`

```cpp
static void erase(uint16_t addr);
```

Erases the byte at `addr` to `0xFF`. Equivalent to `write(addr, 0xFF)`.

**Notes:**

- Still incurs a full write cycle (~3.4 ms). "Erasing" is not free on AVR — the charge pump must run whether you are writing `0xFF` or any other value.
- Use `erase()` by name when readability matters — it signals intent clearly. Use before a `WriteOnly` split sequence (see [Section 4](#4-enumerations)).
- The result is identical to the factory-erased state: `read()` returns `0xFF`.

---

### `readBlock()`

```cpp
static void readBlock(uint16_t addr, void* buf, uint16_t len);
```

Reads `len` consecutive bytes from EEPROM starting at `addr` into `buf`.

**Parameters:**

| Parameter | Description |
|---|---|
| `addr` | Start address |
| `buf` | Destination buffer — must be at least `len` bytes |
| `len` | Number of bytes to read |

**Notes:**

- Implemented as `len` individual `read()` calls. Each call is a single-byte hardware read (1 CPU cycle after EERE).
- `buf` may be any memory region (`uint8_t[]`, struct pointer, etc.) — the function takes a `void*`.
- No bounds checking — ensure `addr + len <= capacity()`.

---

### `writeBlock()`

```cpp
static void writeBlock(uint16_t addr, const void* buf, uint16_t len);
```

Writes `len` bytes from `buf` to consecutive EEPROM addresses starting at `addr`. Every byte triggers an individual write cycle (~3.4 ms each).

**Parameters:**

| Parameter | Description |
|---|---|
| `addr` | Start address |
| `buf` | Source buffer — must be at least `len` bytes |
| `len` | Number of bytes to write |

**Notes:**

- Total time ≈ `len × 3.4 ms` — blocking the CPU for that duration due to `waitReady()` inside each `write()`.
- Prefer `updateBlock()` unless you know every byte has changed.

---

### `updateBlock()`

```cpp
static void updateBlock(uint16_t addr, const void* buf, uint16_t len);
```

Writes `len` bytes from `buf` to EEPROM, skipping each byte that already matches the stored value.

**Notes:**

- Calls `update()` per byte — reads each cell first, writes only if different.
- Time is proportional to the number of bytes that actually changed: `N_changed × 3.4 ms + N_total × ~1 cycle (reads)`.
- **Strongly preferred over `writeBlock()`** whenever endurance matters, which is essentially always. The overhead of the reads is negligible.

---

### `get<T>()`

```cpp
template<typename T>
static T get(uint16_t addr);
```

Reads `sizeof(T)` bytes from EEPROM starting at `addr` and returns them as a value of type `T`.

**Returns:** A value of type `T` reconstructed from the stored bytes.

**Requirements:**

- `T` must be trivially copyable (no virtual functions, no user-defined copy constructor, etc.). Structs of scalar fields qualify; `std::string`, `std::vector`, and similar heap-owning types do not.

**Example:**

```cpp
struct Config {
    uint8_t  magic;
    uint16_t runCount;
    char     name[4];
} __attribute__((packed));

Config cfg = EEPROM.get<Config>(0x020);
```

**Notes:**

- Implemented via `readBlock(addr, &val, sizeof(T))`.
- The byte order in EEPROM is the order bytes appear in memory (little-endian on AVR for multi-byte scalars).
- Returns by value — no in-place reference parameter like the Arduino `EEPROM.get(addr, obj)` API.

---

### `put()`

```cpp
template<typename T>
static void put(uint16_t addr, const T& val);
```

Writes `sizeof(T)` bytes of `val` to EEPROM starting at `addr`. Every byte is written unconditionally (incurs a write cycle per byte).

**Notes:**

- Implemented via `writeBlock(addr, &val, sizeof(T))`.
- Prefer `update<T>()` to avoid wearing cells that already contain the correct value.

---

### `update<T>()`  *(template overload)*

```cpp
template<typename T>
static void update(uint16_t addr, const T& val);
```

Writes `sizeof(T)` bytes of `val` to EEPROM, skipping bytes that already match. This is the typed equivalent of `updateBlock()`.

**Notes:**

- Implemented via `updateBlock(addr, &val, sizeof(T))`.
- When saving a configuration struct that changes rarely, use this instead of `put()`. Only the fields that actually changed will trigger write cycles.
- The byte overload `update(addr, uint8_t)` and this template overload are both named `update`. The compiler picks the non-template for `uint8_t` arguments and the template for all other types.

---

### `enableInterrupt()`

```cpp
static void enableInterrupt();
```

Sets `EERIE` (EEPROM Ready Interrupt Enable) in EECR. When EERIE is set and the global I-bit in SREG is set, the `EE_READY_vect` ISR fires whenever EEPE is 0 (EEPROM is idle).

**Hardware effect:**

```cpp
EECR |= (1 << EERIE);
```

**Important behavior:**

- If EEPE is already 0 when EERIE is set, the ISR fires immediately (before the next instruction). To avoid this, call `enableInterrupt()` *after* calling `write()` — EEPE will be 1 at that point, so the ISR waits until the write completes.
- The ISR fires once per transition of EEPE from 1 to 0. Because EEPE is 0 whenever the EEPROM is idle, the ISR will keep firing as long as EERIE is set and no new write is in progress. Always call `disableInterrupt()` inside the ISR to make it one-shot.

**Correct usage pattern:**

```cpp
volatile bool g_done = false;

ISR(EE_READY_vect) {          // or EE_RDY_vect on ATmega32/16
    EEPROM.disableInterrupt(); // prevent continuous re-entry while idle
    g_done = true;
}

// In application code:
g_done = false;
EEPROM.write(addr, data);      // sets EEPE = 1; write in progress
EEPROM.enableInterrupt();      // safe: EEPE is 1, so ISR won't fire yet
sei();                         // enable global interrupts
while (!g_done) {              // ISR fires ~3.4 ms later
    do_other_work();
}
```

---

### `disableInterrupt()`

```cpp
static void disableInterrupt();
```

Clears `EERIE` in EECR, preventing the `EE_READY_vect` ISR from firing.

**Hardware effect:**

```cpp
EECR &= ~(1 << EERIE);
```

Always call this inside `EE_READY_vect` as the first action to prevent the ISR from re-triggering continuously while the EEPROM is idle.

---

### `setProgrammingMode()`

```cpp
// ATmega328P / devices with EEPM bits only
#if defined(EEPM0) && defined(EEPM1)
static void setProgrammingMode(EEPROMMode mode);
#endif
```

Sets `EEPM1:EEPM0` in EECR to select the programming mode for subsequent `write()` calls.

**Hardware effect:**

```cpp
EECR = (EECR & ~((1<<EEPM1)|(1<<EEPM0))) | (mode << EEPM0);
```

**Notes:**

- Only available when `EEPM0` and `EEPM1` are defined (ATmega328P). The function body is conditionally compiled out on other MCUs.
- The mode applies to **all subsequent writes** until changed. Always restore `EEPROMMode::EraseWrite` after a split sequence to leave the peripheral in the safe default state.
- Does not apply retroactively to a write already in progress.

**Split erase+write sequence:**

```cpp
// Step 1: erase cell to 0xFF
EEPROM.setProgrammingMode(EEPROMMode::EraseOnly);
EEPROM.write(addr, 0x00);      // data byte ignored; cell becomes 0xFF
EEPROM.waitReady();

// Step 2: write data to the now-erased cell
EEPROM.setProgrammingMode(EEPROMMode::WriteOnly);
EEPROM.write(addr, 0xDE);
EEPROM.waitReady();

// Step 3: restore default
EEPROM.setProgrammingMode(EEPROMMode::EraseWrite);
```

---

## 7. Write Cycle Timing

### Single-byte write

```
CPU                  EEPROM hardware
─────────────────    ─────────────────────────────────────────
write() called   →   waitReady() — blocks if previous write ongoing
                     write EEAR, EEDR
                     cli / EEMPE=1 / EEPE=1 / restore SREG
write() returns  ←
                     [hardware: charge pump, erase, program]
                     ~3.4 ms later: EEPE clears, EERIE fires if armed
```

### Timing at common F_CPU values

| Operation | Time | Notes |
|---|---|---|
| Read (`read()`) | ~200 ns at 16 MHz | 1 CPU cycle after EERE |
| Write — erase+write (`EraseWrite`) | ~3.4 ms | Independent of F_CPU |
| Write — erase only (`EraseOnly`) | ~1.8 ms | ATmega328P only |
| Write — write only (`WriteOnly`) | ~1.8 ms | ATmega328P only; cell must be `0xFF` |
| `waitReady()` spin at 16 MHz | ~3.4 ms worst case | ~54,400 loop iterations |

### Block write time

```
writeBlock(addr, buf, len)   ≈  len × 3.4 ms
updateBlock(addr, buf, len)  ≈  N_changed × 3.4 ms   (N_changed ≤ len)
```

A 16-byte `writeBlock` takes approximately **54 ms**. For large writes (configuration structs, log entries), `updateBlock` is essential when most bytes are unchanged.

---

## 8. Usage Patterns

### 8.1 Single-byte read and write

```cpp
#include <mikroduino/eeprom.hpp>
using namespace MikroDuino;

int main() {
    // Write a byte
    EEPROM.write(0x000, 0x42);

    // Read it back (waitReady() is called internally)
    uint8_t v = EEPROM.read(0x000);   // v == 0x42
}
```

### 8.2 Persistent run counter (survives resets and power cycles)

The most common real-world EEPROM use case. Data at a fixed address accumulates across boots.

```cpp
#include <mikroduino/eeprom.hpp>
using namespace MikroDuino;

static constexpr uint16_t ADDR_COUNTER = 0x050;

int main() {
    uint16_t count = EEPROM.get<uint16_t>(ADDR_COUNTER);

    if (count == 0xFFFF) count = 0;  // first boot: factory-erased state
    count++;

    EEPROM.put(ADDR_COUNTER, count);   // save incremented value

    // count now reflects total number of boots (including this one)
}
```

### 8.3 Saving a configuration struct

Use `__attribute__((packed))` to eliminate implicit padding and make the EEPROM layout exactly match your struct layout. Without packing, the compiler may insert padding bytes that waste EEPROM space and shift addresses unpredictably.

```cpp
struct Config {
    uint8_t  magic;       // 0xAB — confirms data was written by this firmware
    uint8_t  mode;
    uint16_t runCount;
    char     ssid[16];
} __attribute__((packed));  // exactly 20 bytes, no padding

static constexpr uint16_t ADDR_CFG = 0x000;

void load_config(Config& out) {
    out = EEPROM.get<Config>(ADDR_CFG);
    if (out.magic != 0xAB) {
        // First run or corrupt data — apply defaults
        out = Config{ 0xAB, 0, 0, "default" };
        EEPROM.put(ADDR_CFG, out);
    }
}

void save_config(const Config& cfg) {
    EEPROM.update(ADDR_CFG, cfg);   // only writes bytes that changed
}
```

### 8.4 update() for endurance preservation

```cpp
// BAD — wastes write cycles even when value is unchanged
void save_mode(uint8_t mode) {
    EEPROM.write(0x002, mode);  // always writes, every time
}

// GOOD — only writes when the value actually changes
void save_mode(uint8_t mode) {
    EEPROM.update(0x002, mode);
}
```

At 100,000 cycle endurance, calling `write()` once per second burns through the limit in ~28 hours. Calling `update()` with the same value costs only a read (negligible wear) — the cell lasts for decades.

### 8.5 Block operations

```cpp
static const char LABEL[16] = "SensorNode-01  ";  // 15 chars + null

// Write the label
EEPROM.writeBlock(0x010, LABEL, 16);

// Read it back
char buf[16];
EEPROM.readBlock(0x010, buf, 16);
buf[15] = '\0';

// Update only if changed (preserves wear on unchanged bytes)
EEPROM.updateBlock(0x010, LABEL, 16);
```

### 8.6 Detecting the factory-erased state

Factory-fresh chips and explicitly erased cells read as `0xFF`. Use a magic byte to distinguish between "data was never written" and "data is valid":

```cpp
static constexpr uint8_t  MAGIC    = 0xAB;
static constexpr uint16_t ADDR_MAGIC = 0x000;

bool eeprom_valid() {
    return EEPROM.read(ADDR_MAGIC) == MAGIC;
}

void eeprom_format() {
    // Erase the magic byte — next boot will re-initialize
    EEPROM.erase(ADDR_MAGIC);
}
```

### 8.7 Interrupt-driven write completion

Use when you want the CPU free during the ~3.4 ms write without spinning in `waitReady()`.

```cpp
#include <avr/interrupt.h>
#include <mikroduino/eeprom.hpp>
using namespace MikroDuino;

// EE_READY_vect on ATmega328P/64/128; EE_RDY_vect on ATmega32/16
#if defined(__AVR_ATmega32__) || defined(__AVR_ATmega16__)
#  define EE_READY_VECT EE_RDY_vect
#else
#  define EE_READY_VECT EE_READY_vect
#endif

volatile bool g_write_done = false;

ISR(EE_READY_VECT) {
    EEPROM.disableInterrupt();  // prevent continuous re-entry while idle
    g_write_done = true;
}

void write_async(uint16_t addr, uint8_t data) {
    g_write_done = false;
    EEPROM.write(addr, data);       // EEPE=1 after this returns
    EEPROM.enableInterrupt();       // safe: EEPE=1, ISR won't fire yet
    sei();
    while (!g_write_done) {
        // CPU free — do other work here
    }
    cli();
}
```

### 8.8 Split erase+write (ATmega328P only)

Useful when you need to erase a block of cells up front, then fill them with data without the overhead of double-cycling each cell.

```cpp
static constexpr uint16_t ADDR_LOG  = 0x100;
static constexpr uint8_t  LOG_LEN   = 32;

void clear_log() {
    // Erase 32 cells to 0xFF (1.8 ms each)
    EEPROM.setProgrammingMode(EEPROMMode::EraseOnly);
    for (uint8_t i = 0; i < LOG_LEN; ++i) {
        EEPROM.write(ADDR_LOG + i, 0x00);   // data ignored; cell → 0xFF
    }
    EEPROM.waitReady();
    EEPROM.setProgrammingMode(EEPROMMode::EraseWrite);  // restore default
}

void write_log_entry(uint8_t offset, uint8_t val) {
    // Cell already 0xFF from clear_log() — WriteOnly is safe
    EEPROM.setProgrammingMode(EEPROMMode::WriteOnly);
    EEPROM.write(ADDR_LOG + offset, val);   // 1.8 ms
    EEPROM.waitReady();
    EEPROM.setProgrammingMode(EEPROMMode::EraseWrite);  // restore default
}
```

### 8.9 Structured EEPROM address map

Assign regions by address constant rather than hardcoded numbers. Keeps layouts readable and collision-free as firmware grows.

```cpp
// eeprom_map.hpp
static constexpr uint16_t EEPROM_MAGIC    = 0x000;   // 1 byte
static constexpr uint16_t EEPROM_COUNTER  = 0x002;   // 2 bytes (uint16_t)
static constexpr uint16_t EEPROM_CONFIG   = 0x010;   // 20 bytes (Config struct)
static constexpr uint16_t EEPROM_LABEL    = 0x030;   // 16 bytes (char[16])
static constexpr uint16_t EEPROM_LOG      = 0x100;   // 128 bytes (log ring buffer)
// Total used: 0x180 = 384 bytes (fits ATmega16's 512 bytes with 128 to spare)
```

---

## 9. MCU Differences

### EEPROM size

| MCU | Size | `MD_EEPROM_SIZE` | `EEPROM.capacity()` |
|---|---|---|---|
| ATmega16 | 512 bytes | `512UL` | `512` |
| ATmega32 | 1024 bytes | `1024UL` | `1024` |
| ATmega328P | 1024 bytes | `1024UL` | `1024` |
| ATmega64 | 2048 bytes | `2048UL` | `2048` |
| ATmega128 | 4096 bytes | `4096UL` | `4096` |

### Write-enable bit names

| MCU | Write enable | Master write enable | SDK handling |
|---|---|---|---|
| ATmega328P | `EEPE` | `EEMPE` | Native names |
| ATmega32 | `EEWE` | `EEMWE` | `#define EEPE EEWE` in eeprom.hpp |
| ATmega16 | `EEWE` | `EEMWE` | `#define EEPE EEWE` in eeprom.hpp |
| ATmega64 | `EEPE` | `EEMPE` | Native names |
| ATmega128 | `EEPE` | `EEMPE` | Native names |

The SDK normalizes these with:

```cpp
#ifndef EEPE
#  define EEPE  EEWE
#  define EEMPE EEMWE
#endif
```

### Programming mode bits (EEPM)

`EEPM0` and `EEPM1` exist only on ATmega328P. The `EEPROMMode` enum and `setProgrammingMode()` are wrapped in `#if defined(EEPM0) && defined(EEPM1)` and compile to nothing on other MCUs. On those devices, the write mode is always atomic erase+write.

### EEPROM Ready interrupt vector name

| MCU | ISR vector name |
|---|---|
| ATmega16 | `EE_RDY_vect` |
| ATmega32 | `EE_RDY_vect` |
| ATmega328P | `EE_READY_vect` |
| ATmega64 | `EE_READY_vect` |
| ATmega128 | `EE_READY_vect` |

The example in [Section 8.7](#87-interrupt-driven-write-completion) shows the portability guard pattern.

### SPM register name

The SDK guards for both `SPMCSR` (ATmega328P, 64, 128) and `SPMCR` (ATmega32, ATmega16) when waiting for self-programming to complete:

```cpp
#if defined(SPMCSR) && defined(SPMEN)
    while (SPMCSR & (1 << SPMEN));
#elif defined(SPMCR) && defined(SPMEN)
    while (SPMCR  & (1 << SPMEN));
#endif
```

---

## 10. Common Pitfalls

### Not using `update()` for frequently-written values

Every `write()` call consumes one write cycle whether or not the value changed. With a 100,000-cycle limit, a value written 10 times per second is exhausted in less than 3 hours:

```
100,000 cycles / (10 writes/s × 3600 s/h) ≈ 2.8 hours
```

Use `update()` or `updateBlock()` wherever data may not have changed. The only cost is a read per byte (negligible).

### Treating `erase()` as free

```cpp
// WRONG mental model
EEPROM.erase(addr);   // "this is free, right?"

// CORRECT: erase() calls _write(addr, 0xFF) — costs a full ~3.4 ms write cycle
```

Erasing is a write. Erased cells count against the 100,000-cycle endurance exactly as any other write does.

### Setting EERIE while EEPE is clear

```cpp
// WRONG — EEPE is already 0 (no write in progress); ISR fires immediately
EEPROM.enableInterrupt();   // interrupts must be off or this ISR triggers at once
sei();
EEPROM.write(addr, data);   // too late: ISR already fired

// CORRECT — start write first, then arm the interrupt
EEPROM.write(addr, data);   // EEPE = 1 after this
EEPROM.enableInterrupt();   // safe: EEPE=1, ISR waits for write to finish
sei();
```

### Forgetting `disableInterrupt()` inside the ISR

```cpp
// WRONG — ISR fires continuously while EEPROM is idle
ISR(EE_READY_vect) {
    g_done = true;           // fires over and over, burning CPU
}

// CORRECT — disable EERIE immediately on entry
ISR(EE_READY_vect) {
    EEPROM.disableInterrupt();  // must be first
    g_done = true;
}
```

### `WriteOnly` on a non-erased cell

```cpp
// WRONG — cell contains 0x55; WriteOnly cannot turn 0-bits back to 1
EEPROM.setProgrammingMode(EEPROMMode::WriteOnly);
EEPROM.write(addr, 0xAA);   // result is 0x00 (0x55 AND 0xAA), not 0xAA

// CORRECT — erase first
EEPROM.setProgrammingMode(EEPROMMode::EraseOnly);
EEPROM.write(addr, 0x00);   // cell → 0xFF
EEPROM.waitReady();
EEPROM.setProgrammingMode(EEPROMMode::WriteOnly);
EEPROM.write(addr, 0xAA);   // now correct
EEPROM.waitReady();
EEPROM.setProgrammingMode(EEPROMMode::EraseWrite);  // restore default
```

### Forgetting to restore `EraseWrite` after a split sequence

```cpp
// Leaving WriteOnly mode active corrupts subsequent writes
EEPROM.setProgrammingMode(EEPROMMode::WriteOnly);
EEPROM.write(addr1, data1);
EEPROM.waitReady();
// ... forgot to restore ...

EEPROM.write(addr2, data2);  // also WriteOnly! cell may corrupt if not pre-erased
```

Always restore `EEPROMMode::EraseWrite` immediately after the split sequence completes.

### Struct padding corrupting the EEPROM layout

```cpp
// WRONG — compiler may insert padding bytes between fields
struct Config {
    uint8_t  mode;      // offset 0
    // implicit 1-byte pad here (alignment for uint16_t)
    uint16_t count;     // offset 2 (not 1!)
    uint8_t  flags;     // offset 4
};
// sizeof(Config) == 6, not 5 — layout in EEPROM has a garbage byte at offset 1

// CORRECT
struct Config {
    uint8_t  mode;
    uint16_t count;
    uint8_t  flags;
} __attribute__((packed));
// sizeof(Config) == 4 — deterministic, no padding
```

Without `packed`, upgrading the firmware with a different compiler or different optimization flags may produce a different struct layout, silently corrupting saved data.

### Address out of bounds

The EEPROM controller does not enforce address bounds. Writing beyond `capacity()` wraps around or accesses undefined cells depending on the MCU. The SDK does not add runtime bounds checking to keep code size small — validate addresses in application code:

```cpp
void safe_write(uint16_t addr, uint8_t data) {
    if (addr >= EEPROM.capacity()) return;  // or assert
    EEPROM.write(addr, data);
}
```

### Interrupts during the EEMPE → EEPE window

The write sequence requires `EEPE` to be set within 4 CPU cycles of `EEMPE`. If an interrupt fires in that window, the write silently fails (EEMPE resets after 4 cycles). The SDK prevents this with `ATOMIC_BLOCK_START` / `ATOMIC_BLOCK_END` which save and restore SREG around the critical 2-instruction window:

```cpp
// Inside _write() — do not replicate this without the atomic guard
ATOMIC_BLOCK_START;    // saves SREG, executes CLI
BITSET(EECR, EEMPE);
BITSET(EECR, EEPE);    // must reach here within 4 cycles of EEMPE
ATOMIC_BLOCK_END;      // restores SREG
```

If you write EEPROM directly via registers, always include this guard.

---

## Quick Reference

| Task | Call |
|---|---|
| Read one byte | `EEPROM.read(addr)` |
| Write one byte | `EEPROM.write(addr, data)` |
| Write if changed | `EEPROM.update(addr, data)` |
| Erase to 0xFF | `EEPROM.erase(addr)` |
| Read N bytes | `EEPROM.readBlock(addr, buf, len)` |
| Write N bytes | `EEPROM.writeBlock(addr, buf, len)` |
| Write N bytes if changed | `EEPROM.updateBlock(addr, buf, len)` |
| Read a struct | `MyStruct s = EEPROM.get<MyStruct>(addr)` |
| Write a struct | `EEPROM.put(addr, s)` |
| Write struct if changed | `EEPROM.update(addr, s)` |
| Check write in progress | `EEPROM.busy()` |
| Wait for write to finish | `EEPROM.waitReady()` |
| EEPROM size in bytes | `EEPROM.capacity()` |
| Arm write-done interrupt | `EEPROM.enableInterrupt()` |
| Disarm interrupt | `EEPROM.disableInterrupt()` |
| Set programming mode | `EEPROM.setProgrammingMode(EEPROMMode::…)` |
