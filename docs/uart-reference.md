# USART Reference — MikroDuino SDK

Complete reference for `MikroDuino::USARTDriver` — hardware internals, configuration, the full API, interrupt patterns, and PROGMEM string handling.

---

## Table of Contents

1. [What is USART?](#1-what-is-usart)
2. [Hardware Architecture](#2-hardware-architecture)
3. [Hardware Registers](#3-hardware-registers)
4. [Hardware Pins](#4-hardware-pins)
5. [Global Instances](#5-global-instances)
6. [Configuration Enums](#6-configuration-enums)
7. [USARTConfig Struct](#7-usartconfig-struct)
8. [Quick Start](#8-quick-start)
9. [Transmit API](#9-transmit-api)
10. [PROGMEM Strings](#10-progmem-strings)
11. [Receive API](#11-receive-api)
12. [Status and Control API](#12-status-and-control-api)
13. [Interrupt API](#13-interrupt-api)
14. [Interrupt-Driven RX (Ring Buffer)](#14-interrupt-driven-rx-ring-buffer)
15. [Interrupt-Driven TX (UDRE)](#15-interrupt-driven-tx-udre)
16. [TXC Interrupt](#16-txc-interrupt)
17. [ISR Vector Names by MCU](#17-isr-vector-names-by-mcu)
18. [Baud Rate Accuracy](#18-baud-rate-accuracy)
19. [Multi-USART Devices](#19-multi-usart-devices)
20. [Recipes](#20-recipes)
21. [Common Mistakes](#21-common-mistakes)

---

## 1. What is USART?

USART (Universal Synchronous/Asynchronous Receiver/Transmitter) is the AVR hardware block for serial communication. In **asynchronous mode** — which is almost always what you want — it behaves as a plain UART. Data is transmitted one bit at a time with a fixed clock derived from the CPU clock.

Both sides of a UART link must agree on four parameters:

| Parameter | Description | Typical value |
|-----------|-------------|---------------|
| Baud rate | Bits per second on the wire | 9600, 115200 |
| Data bits | Payload bits per frame | 8 |
| Parity | Optional error-check bit appended | None |
| Stop bits | Idle bit(s) at end of frame | 1 |

Shorthand `9600-8-N-1` means 9600 baud, 8 data bits, No parity, 1 stop bit — the most common configuration.

A UART connection needs three wires:

```
MCU                  Peripheral (USB-serial adapter, GPS, etc.)
TX ──────────────►   RX
RX ◄──────────────   TX
GND ─────────────    GND
```

**Only** TX, RX, and GND are connected. Do not wire 5 V or 3.3 V from the adapter if the MCU has its own power supply.

---

## 2. Hardware Architecture

```
                ┌─────────────────────────────────────────────┐
                │              ATmega328P USART0               │
                │                                              │
  CPU writes ──►│  UDR0 (TX)                    Baud generator│◄── F_CPU
                │    │                         UBRR0 → ÷(16n) │
                │    ▼                               │         │
                │  TX shift register ─────────────► PD1 (TXD) │──► wire
                │                                              │
  PD0 (RXD) ──►│ ◄── RX shift register                       │
  from wire     │          │                                   │
                │          ▼                                   │
                │       UDR0 (RX) ──► CPU reads               │
                │                                              │
                │  UCSR0A  UCSR0B  UCSR0C  (control/status)   │
                └─────────────────────────────────────────────┘
```

### Frame format

Every transmitted byte is wrapped in a serial frame:

```
   [START] [D0] [D1] [D2] [D3] [D4] [D5] [D6] [D7] [PARITY?] [STOP] [STOP?]
      0      ←─── data bits (LSB first) ────►      optional     1       optional
```

- **Start bit**: always logic 0, signals the beginning of a frame.
- **Data bits**: 5–9 bits, sent LSB first.
- **Parity bit**: optional even or odd parity.
- **Stop bit(s)**: always logic 1; 1 or 2 stop bits.

The total time per frame determines the maximum baud rate.

### Clocking

The baud rate generator divides `F_CPU` by a prescaler stored in `UBRR0`:

| Mode | Divisor | UBRR formula |
|------|---------|-------------|
| Normal (U2X=0) | 16 | `UBRR = F_CPU / (16 × baud) − 1` |
| Double speed (U2X=1) | 8 | `UBRR = F_CPU / (8 × baud) − 1` |

---

## 3. Hardware Registers

### UCSR0A — USART Control and Status Register A

| Bit | Name | R/W | Description |
|-----|------|-----|-------------|
| 7 | **RXC0** | R | **RX Complete** — 1 when an unread byte is in UDR0. Cleared by reading UDR0. |
| 6 | **TXC0** | R/W | **TX Complete** — 1 after the last stop bit has left the shift register. Cleared by writing 1 or by the TXC ISR. |
| 5 | **UDRE0** | R | **Data Register Empty** — 1 when UDR0 is ready to accept a new byte. |
| 4 | **FE0** | R | **Frame Error** — 1 if the received frame's stop bit was 0 (wrong baud rate, line noise). Must be read before UDR0. |
| 3 | **DOR0** | R | **Data OverRun** — 1 if a new frame arrived before the previous UDR0 was read. |
| 2 | **UPE0** | R | **Parity Error** — 1 if the received parity bit didn't match. |
| 1 | **U2X0** | R/W | **Double Speed** — 1 halves the clock divisor (UBRR formula changes). |
| 0 | **MPCM0** | R/W | Multi-processor Communication Mode — not used in normal UART. |

`rxError()` returns `(FE0 | DOR0 | UPE0)`. These bits are only valid **before** reading `UDR0`.

---

### UCSR0B — USART Control and Status Register B

| Bit | Name | R/W | Description |
|-----|------|-----|-------------|
| 7 | **RXCIE0** | R/W | RX Complete Interrupt Enable — enables `USART_RX_vect`. |
| 6 | **TXCIE0** | R/W | TX Complete Interrupt Enable — enables `USART_TX_vect`. |
| 5 | **UDRIE0** | R/W | Data Register Empty Interrupt Enable — enables `USART_UDRE_vect`. |
| 4 | **RXEN0** | R/W | Receiver Enable — 0 disables the receiver; PD0 reverts to GPIO. |
| 3 | **TXEN0** | R/W | Transmitter Enable — 0 disables the transmitter; PD1 reverts to GPIO. |
| 2 | **UCSZ02** | R/W | Data Bits bit 2 — set together with UCSZ01:00 for 9-bit mode. |
| 1 | **RXB80** | R | Received 9th data bit (9-bit mode only). |
| 0 | **TXB80** | R/W | 9th data bit to transmit (9-bit mode only). |

`enableRxInterrupt()` sets RXCIE0. `end()` clears RXEN0, TXEN0, RXCIE0, TXCIE0, and UDRIE0.

---

### UCSR0C — USART Control and Status Register C

| Bits | Name | Description |
|------|------|-------------|
| 7:6 | UMSEL01:00 | Mode: 00=async UART, 01=sync UART, 11=SPI master. SDK always uses 00. |
| 5:4 | UPM01:00 | Parity: 00=None, 10=Even, 11=Odd. |
| 3 | USBS0 | Stop bits: 0=1 stop bit, 1=2 stop bits. |
| 2:1 | UCSZ01:00 | Data bits (together with UCSZ02 in UCSR0B): 000=5, 001=6, 010=7, 011=8, 111=9. |
| 0 | UCPOL0 | Clock polarity — only used in synchronous mode. |

---

### UBRR0 — Baud Rate Register (16-bit)

`UBRR0H:UBRR0L` — only the lower 12 bits are used. `begin()` writes the calculated UBRR value here based on `F_CPU` and the requested baud rate.

---

### UDR0 — USART Data Register

Dual-purpose register:
- **Writing** loads the TX buffer (starts transmission when `TXEN0=1` and `UDRE0=1`).
- **Reading** retrieves the oldest received byte from the two-level RX FIFO (and clears `RXC0` if no more bytes are waiting).

Reading `UDR0` also clears `FE0`, `DOR0`, and `UPE0` — always check `rxError()` before reading `UDR0`.

---

## 4. Hardware Pins

### ATmega328P / Arduino Nano

| Signal | AVR pin | Arduino label | Notes |
|--------|---------|---------------|-------|
| USART0 TX | PD1 | D1 / TX | Driven by USART hardware when TXEN0=1 |
| USART0 RX | PD0 | D0 / RX | Sampled by USART hardware when RXEN0=1 |

The USART hardware takes ownership of PD1 and PD0 when enabled — do not configure them as GPIO outputs while TXEN0/RXEN0 are set. When `end()` is called they revert to GPIO.

On Arduino Nano, PD0/PD1 are connected to the CH340 USB-serial chip on-board. Opening the Serial Monitor uses this bridge.

### ATmega64 / ATmega128

| Signal | Pin |
|--------|-----|
| USART0 TX | PE1 |
| USART0 RX | PE0 |
| USART1 TX | PD3 |
| USART1 RX | PD2 |

---

## 5. Global Instances

Include the header and bring the namespace into scope:

```cpp
#include <mikroduino/usart.hpp>
using namespace MikroDuino;
```

The SDK defines these global instances (as `static` variables in the header — C++17):

| Instance | Available on |
|----------|-------------|
| `USART0` | All supported MCUs |
| `USART1` | ATmega64, ATmega128 |

Because the instances are `static` in the header, they are defined once per translation unit. For the single-source-file project structure used by MikroDuino examples, this is exactly one instance per project. No `.cpp` file needs to be added to `sourceFiles`.

---

## 6. Configuration Enums

### `USARTDataBits`

Number of payload bits per frame.

| Value | Bits | UCSZ setting |
|-------|------|-------------|
| `USARTDataBits::Five` | 5 | UCSZ=000 |
| `USARTDataBits::Six` | 6 | UCSZ=001 |
| `USARTDataBits::Seven` | 7 | UCSZ=010 |
| `USARTDataBits::Eight` | 8 | UCSZ=011 (default) |
| `USARTDataBits::Nine` | 9 | UCSZ=111 (UCSZ02 in UCSR0B) |

9-bit mode is used in multi-drop networks (address vs data frame discrimination via the 9th bit). The SDK's `write(uint8_t)` always sets TXB80=0; to set the 9th bit, write directly to `UCSR0B`.

---

### `USARTParity`

| Value | Parity bit | UPM setting |
|-------|-----------|------------|
| `USARTParity::None` | None | UPM=00 (default) |
| `USARTParity::Even` | Even | UPM=10 |
| `USARTParity::Odd` | Odd | UPM=11 |

Even parity: the parity bit is set so the total number of 1-bits in the frame (data + parity) is even.  
Odd parity: same, but total is odd.  
If parity is enabled, the receiver checks it and sets `UPE0` on mismatch.

---

### `USARTStopBits`

| Value | Stop bits | USBS0 |
|-------|-----------|-------|
| `USARTStopBits::One` | 1 | 0 (default) |
| `USARTStopBits::Two` | 2 | 1 |

The receiver only checks the **first** stop bit, so two-stop-bit frames are always accepted by a one-stop-bit receiver. Two stop bits add one extra bit-time of guaranteed idle between frames, which helps slower or more jittery receivers synchronise.

---

### `USARTMode`

Controls which hardware path is enabled. Disabling a path returns the corresponding pin to GPIO.

| Value | TXEN0 | RXEN0 | PD1 (TX) | PD0 (RX) |
|-------|-------|-------|----------|----------|
| `USARTMode::TX` | 1 | 0 | USART output | free GPIO |
| `USARTMode::RX` | 0 | 1 | free GPIO | USART input |
| `USARTMode::TX_RX` | 1 | 1 | USART output | USART input (default) |

---

## 7. USARTConfig Struct

```cpp
struct USARTConfig {
    uint32_t      baudRate    = 9600;
    USARTDataBits dataBits    = USARTDataBits::Eight;
    USARTParity   parity      = USARTParity::None;
    USARTStopBits stopBits    = USARTStopBits::One;
    USARTMode     mode        = USARTMode::TX_RX;
    bool          doubleSpeed = false;
};
```

All fields have defaults corresponding to `9600-8-N-1 TX_RX`. Only set the fields you want to change:

```cpp
USARTConfig cfg;
cfg.baudRate = 115200;
cfg.doubleSpeed = true;    // better accuracy at 115200 / 16 MHz
USART0.begin(cfg);
```

---

## 8. Quick Start

```cpp
#include <mikroduino/usart.hpp>
using namespace MikroDuino;

int main() {
    USART0.begin(9600);                   // 9600-8-N-1, TX+RX enabled

    USART0.writeLine("Hello, world!");    // sends "Hello, world!\r\n"

    while (1) {
        if (USART0.available()) {
            uint8_t c = USART0.read();
            USART0.write(c);              // echo each received byte
        }
    }
}
```

Open the MikroDuino Serial Monitor at 9600 baud. You should see `Hello, world!` and every character you type echoed back.

---

## 9. Transmit API

### `begin(uint32_t baudRate)`

Initialises USART with the given baud rate and all other settings at their defaults (8-N-1, TX_RX, no double speed). Writes UCSR0A/B/C and UBRR0.

```cpp
USART0.begin(9600);
USART0.begin(115200);
```

---

### `begin(const USARTConfig& cfg)`

Full initialisation from a config struct. Applies every field — baud rate, data bits, parity, stop bits, TX/RX mode, and double speed.

```cpp
USARTConfig cfg;
cfg.baudRate = 9600;
cfg.parity   = USARTParity::Even;
USART0.begin(cfg);
```

Hardware effects:
- Computes UBRR from `F_CPU`, `cfg.baudRate`, and `cfg.doubleSpeed`; writes to UBRR0.
- Builds the UCSR0C byte from `dataBits`, `parity`, and `stopBits`; writes it.
- Sets TXEN0 and/or RXEN0 in UCSR0B according to `mode`.
- Sets or clears U2X0 in UCSR0A.

Can be called at any time to reconfigure. Always call `flush()` first if there may be data still in the TX shift register.

---

### `end()`

Disables the USART completely.

Hardware effects: clears RXEN0, TXEN0, RXCIE0, TXCIE0, UDRIE0. PD0 and PD1 revert to GPIO inputs.

```cpp
USART0.flush();    // drain TX first
USART0.end();
```

---

### `write(uint8_t data)`

Transmits one raw byte. Blocks until `UDRE0=1` (TX data register free), then loads the byte. Returns immediately — the byte has not left the wire yet when `write` returns; it is queued in the shift register.

```cpp
USART0.write('A');           // sends 0x41
USART0.write(0x0D);          // carriage return
USART0.write(0x00);          // NUL byte (not a string terminator here)
```

---

### `write(const char* str)`

Sends a null-terminated C string from **SRAM**. Calls `write(uint8_t)` for each character until the null terminator. Does not append a line ending.

```cpp
USART0.write("Voltage: ");
```

> **AVR memory warning**: string literals placed in `write("...")` calls are stored in SRAM (copied from flash at startup via the `.data` section). ATmega328P has only 2 KB of SRAM. A program with many such literals can overflow SRAM before `main()` even starts. Use `write_P(PSTR("..."))` for all constant strings — see §10.

---

### `writeLine(const char* str)`

Sends a SRAM string followed by `\r\n`. Equivalent to `write(str); write('\r'); write('\n');`.

```cpp
USART0.writeLine("Done.");    // sends "Done.\r\n"
```

Same SRAM warning as `write(str)`. Prefer `writeLine_P(PSTR("..."))`.

---

### `write_P(PGM_P str)`

Sends a null-terminated string stored in **flash** (PROGMEM). Reads each byte with `pgm_read_byte()` — zero SRAM cost for the string data.

```cpp
#include <avr/pgmspace.h>

USART0.write_P(PSTR("Hello from flash"));
```

Use this instead of `write(const char*)` for every constant string. See §10 for the full pattern.

---

### `writeLine_P(PGM_P str)`

Sends a PROGMEM string followed by `\r\n`. Use in place of `writeLine(const char*)`.

```cpp
USART0.writeLine_P(PSTR("Startup complete."));
```

---

### `writeInt(int32_t value, uint8_t base = 10)`

Converts `value` to ASCII using `ltoa()` into a 12-byte stack buffer, then sends the result. Base selects the numeral system.

```cpp
USART0.writeInt(1234);          // "1234"
USART0.writeInt(1234, 16);      // "4D2"    (hex, no 0x prefix)
USART0.writeInt(1234, 2);       // "10011010010"  (binary)
USART0.writeInt(1234, 8);       // "2322"   (octal)
USART0.writeInt(-99);           // "-99"
```

Note: base 16 does not add a `0x` prefix. Use `writeHex()` if you need the prefix and zero-padded 8-digit output.

---

### `writeHex(uint32_t value)`

Sends a fixed-width 32-bit hex value with a `0x` prefix, always 10 characters total. Uppercase hex digits.

```cpp
USART0.writeHex(0x1A2B);        // "0x00001A2B"
USART0.writeHex(0xDEADBEEF);    // "0xDEADBEEF"
USART0.writeHex(adc_value);     // "0x000001F4"  (for value 500)
```

Useful for register dumps, addresses, or status words where full width and alignment matter.

---

### `writeFloat(float value, uint8_t decimals = 2)`

Sends a fixed-decimal ASCII representation of a float — sign, integer part via `writeInt()`, then a `.` and `decimals` fractional digits computed by repeated `×10` extraction. No `<stdio.h>` printf/dtostrf involved.

```cpp
USART0.writeFloat(3.14159f);        // "3.14"   (default 2 decimals)
USART0.writeFloat(3.14159f, 4);     // "3.1415"
USART0.writeFloat(-0.5f, 1);        // "-0.5"
USART0.writeFloat(sensor_v, 3);     // "4.987"
```

Excluded when `MD_NO_FLOAT` is defined before including `mikroduino.hpp`. Uses AVR soft-float arithmetic — the same float support already linked in if you use any `float` in the project; it does not pull in `<stdio.h>`'s printf/dtostrf float formatter.

---

### `flush()`

Blocks until `TXC0=1` — the last stop bit has physically left the wire and the line is idle.

```cpp
USART0.writeLine_P(PSTR("Shutting down..."));
USART0.flush();     // wait before reconfiguring or entering sleep
USART0.end();
```

Not needed between ordinary `write()` calls — `write()` already waits for `UDRE0`. Only needed when you need the physical line to be idle (before `end()`, before sleep, before bit-banging PD1, before RS-485 direction switch).

---

### `txReady()`

Returns `true` if `UDRE0=1` — the TX data register is empty and a `write()` call will not block right now.

```cpp
if (USART0.txReady()) {
    USART0.write('.');    // guaranteed non-blocking
}
```

Use in real-time loops where you cannot afford to block:

```cpp
if (streaming && USART0.txReady()) {
    USART0.write_P(PSTR("POT:"));
    USART0.writeInt(pot_value);
    USART0.writeLine_P(PSTR(""));
}
```

---

## 10. PROGMEM Strings

### The problem

On AVR, the compiler places all string literals (including those inside `write("...")` calls) in the `.data` section, which is **copied from flash to SRAM at startup**. The ATmega328P has only 2 KB of SRAM. A program with dozens of string literals for menus, labels, and debug output can exhaust SRAM before `main()` runs, causing a silent hang or stack corruption.

The linker reports this as:
```
address 0x800da4 of MyProject.elf section '.data' is not within region 'data'
```

### The solution: PROGMEM + `PSTR()`

Use `PSTR("text")` to keep a string literal in flash. Pair it with `write_P()` / `writeLine_P()` to send it byte-by-byte from flash with zero SRAM cost.

```cpp
#include <avr/pgmspace.h>
#include <mikroduino/usart.hpp>
using namespace MikroDuino;

// Shorthand macros for convenience:
#define WP(s)  USART0.write_P(PSTR(s))
#define WLP(s) USART0.writeLine_P(PSTR(s))

int main() {
    USART0.begin(9600);
    WLP("MikroDuino USART Demo");      // "MikroDuino USART Demo\r\n" — zero SRAM
    WP("Value: ");
    USART0.writeInt(42);               // writeInt uses a 12-byte stack buffer
    WLP("");                           // just \r\n
}
```

### Named PROGMEM strings

For strings that are reused or too long for inline `PSTR()`:

```cpp
static const char MSG_BANNER[] PROGMEM = "MikroDuino v1.0 — ATmega328P @ 16 MHz";
static const char MSG_READY[]  PROGMEM = "Ready. Send 'h' for help.";

USART0.writeLine_P(MSG_BANNER);
USART0.writeLine_P(MSG_READY);
```

### PROGMEM in ISRs (UDRE pattern)

Async TX from a PROGMEM string via the UDRE interrupt — see §15. The ISR reads each byte with `pgm_read_byte()`:

```cpp
static const char TX_MSG[] PROGMEM = "Sent from ISR!\r\n";
static const char* volatile udre_ptr = nullptr;

ISR(USART_UDRE_vect) {
    const char* p = udre_ptr;
    uint8_t c = pgm_read_byte(p);
    if (c) {
        UDR0     = c;
        udre_ptr = p + 1;
    } else {
        USART0.disableUDREInterrupt();
        udre_ptr = nullptr;
    }
}
```

### What is safe in SRAM

These do **not** consume permanent SRAM:
- `writeInt(val)` — uses a 12-byte stack buffer (stack, not .data)
- `writeHex(val)` — computes digits directly
- `writeFloat(val, decimals)` — computes digits directly via `writeInt()`, no buffer
- Local `char buf[]` arrays — stack allocated, freed on function return
- `volatile uint8_t rx_buf[N]` — this is intentional SRAM (ring buffer data, not strings)

These **do** consume permanent SRAM (avoid for constant text):
- `write("literal")` — puts the string in .data
- `writeLine("literal")` — same
- `static const char msg[] = "text"` — same unless `PROGMEM` is added
- `const char* p = "text"` — same; the pointed-to data is in .data

### SRAM budget guidance (ATmega328P)

| Region | Size | Notes |
|--------|------|-------|
| Total SRAM | 2048 bytes | 0x0100–0x08FF |
| .data + .bss | Varies | Global/static variables, string literals |
| Stack | Grows downward from 0x08FF | Each function call, local arrays, ISR context |
| Heap | Not used | No `malloc` in typical AVR firmware |
| Recommended .data headroom | ≥ 200 bytes | Leaves room for ISR stack frames |

---

## 11. Receive API

### `available()`

Returns `true` if `RXC0=1` — at least one received byte is waiting in `UDR0`. Non-blocking, does not consume the byte.

```cpp
while (1) {
    if (USART0.available()) {
        uint8_t c = USART0.read();
        process(c);
    }
    // other work runs here unconditionally
}
```

---

### `read()`

Blocks until `RXC0=1`, then reads and returns `UDR0`. Clears `RXC0` (and FE0/DOR0/UPE0).

```cpp
uint8_t c = USART0.read();    // waits until a byte arrives
```

Safe only when:
- You guard it with `available()` first, or
- You deliberately want to halt until input arrives (e.g., a command prompt that blocks until the user presses Enter).

Never call inside an ISR — it spin-waits, which deadlocks.

---

### `readNonBlock(bool& dataReady)`

Immediately reads `UDR0` if `RXC0=1`, otherwise returns 0. Sets `dataReady` to indicate whether the return value is valid.

```cpp
bool got;
uint8_t c = USART0.readNonBlock(got);
if (got) {
    // c is a valid received byte
}
```

Equivalent to checking `available()` then `read()`, but in a single call. Useful when your loop structure makes a combined check cleaner.

---

## 12. Status and Control API

### `rxError()`

Returns `true` if any of FE0, DOR0, or UPE0 are set in UCSR0A. **Must be called before `read()`** — reading `UDR0` clears these flags.

```cpp
if (USART0.available()) {
    bool err = USART0.rxError();    // check first
    uint8_t c = USART0.read();      // clears error flags
    if (err) {
        WLP("[RX ERROR: FE/DOR/UPE]");
    } else {
        process(c);
    }
}
```

| Flag | Set when | Typical cause |
|------|----------|---------------|
| FE0 (Frame Error) | Stop bit was 0 | Baud rate mismatch; line break |
| DOR0 (Data OverRun) | New frame started before UDR0 was read | Main loop too slow; use interrupt-driven RX |
| UPE0 (Parity Error) | Parity bit did not match | Parity setting mismatch; electrical noise |

---

### `txReady()`

Returns `true` if `UDRE0=1` — the TX data register is empty and a write will not block. See §9.

---

### `flush()`

Blocks until `TXC0=1`. See §9.

---

## 13. Interrupt API

### RX Complete interrupt

```cpp
USART0.enableRxInterrupt();     // RXCIE0 = 1 — ISR(USART_RX_vect) armed
USART0.disableRxInterrupt();    // RXCIE0 = 0
```

ISR fires on each received byte. Must read `UDR0` inside the ISR (reading clears `RXC0`). See §14 for the ring buffer pattern.

---

### TX Complete interrupt

```cpp
USART0.enableTxInterrupt();     // TXCIE0 = 1 — ISR(USART_TX_vect) armed
USART0.disableTxInterrupt();    // TXCIE0 = 0
```

ISR fires after the last stop bit of the last transmitted byte has left the wire. More precise than `flush()` (which polls TXC0). Useful for RS-485 direction control (DE/RE pin) where you must drop the transmit-enable line the instant the line goes idle.

```cpp
static volatile bool txc_fired = false;

ISR(USART_TX_vect) {
    txc_fired = true;
    // In RS-485: GPIO::clear(DE_PIN);  // release bus
}
```

---

### UDRE interrupt (Data Register Empty)

```cpp
USART0.enableUDREInterrupt();    // UDRIE0 = 1 — ISR(USART_UDRE_vect) armed
USART0.disableUDREInterrupt();   // UDRIE0 = 0
```

ISR fires whenever `UDRE0=1` — the TX data register is free. Used for interrupt-driven TX: the ISR feeds bytes one at a time and disables itself when done. See §15.

**Note:** enabling UDRIE with an empty UDR causes the ISR to fire immediately. Always set up your TX pointer before enabling the interrupt.

---

### General rules for all USART ISRs

1. Always use `sei()` in `main()` to enable global interrupts before any USART ISR will fire.
2. Declare all variables shared between ISR and `main()` as `volatile`.
3. Never call `write()`, `read()`, or `flush()` inside an ISR — they all spin on hardware flags, which can deadlock inside an interrupt.
4. Keep ISR body short — store the byte or advance the pointer, then return.
5. `cli()` / `sei()` bracket any multi-byte access to shared state from the main thread if the ISR writes the same variable.

---

## 14. Interrupt-Driven RX (Ring Buffer)

The hardware has a 2-byte RX FIFO. If the main loop is busy for longer than two frame times (at 9600 baud, one frame = ~1 ms), a data overrun occurs. The solution is the RX-complete ISR with a ring buffer.

```cpp
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <mikroduino/usart.hpp>
using namespace MikroDuino;

#define WLP(s) USART0.writeLine_P(PSTR(s))

// Power-of-two size so wrap-around is a cheap bitmask
static constexpr uint8_t BUF_SIZE = 64;
static constexpr uint8_t BUF_MASK = BUF_SIZE - 1;

static volatile uint8_t rx_buf[BUF_SIZE];
static volatile uint8_t rx_head = 0;   // written by ISR
static volatile uint8_t rx_tail = 0;   // read by main

ISR(USART_RX_vect) {
    uint8_t b    = UDR0;                        // read clears RXC0
    uint8_t next = (rx_head + 1) & BUF_MASK;
    if (next != rx_tail) {                      // drop silently if full
        rx_buf[rx_head] = b;
        rx_head = next;
    }
}

static bool    rx_available() { return rx_head != rx_tail; }
static uint8_t rx_get()       {
    uint8_t d = rx_buf[rx_tail];
    rx_tail   = (rx_tail + 1) & BUF_MASK;
    return d;
}

int main() {
    USART0.begin(9600);
    USART0.enableRxInterrupt();
    sei();

    WLP("Ring buffer RX active.");

    while (1) {
        while (rx_available()) {
            uint8_t c = rx_get();
            USART0.write(c);        // echo
        }
        // do other work freely — no bytes are lost
    }
}
```

### Why power-of-two buffer size?

`(index + 1) & (BUF_SIZE - 1)` is a single AND instruction on AVR. Modulo with a non-power-of-two (`% N`) requires a division, which is slow on an 8-bit CPU with no hardware divider.

---

## 15. Interrupt-Driven TX (UDRE)

For transmitting a long string without blocking the main thread: arm the UDRE ISR, point a pointer at the string, then let the ISR feed one byte per interrupt while the main thread does other work. The ISR disables itself when done.

With PROGMEM strings, the ISR reads from flash using `pgm_read_byte()`:

```cpp
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <mikroduino/usart.hpp>
using namespace MikroDuino;

static const char LONG_MSG[] PROGMEM =
    "This entire paragraph is being transmitted one byte per UDRE interrupt "
    "while the main loop runs freely. No blocking write() calls involved.\r\n";

static const char* volatile udre_ptr  = nullptr;
static volatile bool        udre_done = false;

ISR(USART_UDRE_vect) {
    const char* p = udre_ptr;          // single volatile read
    uint8_t c = pgm_read_byte(p);
    if (c) {
        UDR0     = c;
        udre_ptr = p + 1;
    } else {
        USART0.disableUDREInterrupt(); // stop the ISR — string is done
        udre_ptr  = nullptr;
        udre_done = true;
    }
}

static void send_async(const char* PROGMEM msg) {
    udre_done = false;
    udre_ptr  = msg;                   // set pointer BEFORE enabling ISR
    USART0.enableUDREInterrupt();      // UDRIE0=1 — ISR fires immediately (UDR empty)
    sei();
}

int main() {
    USART0.begin(9600);

    send_async(LONG_MSG);

    while (!udre_done) {
        // main thread free — run sensors, update LEDs, etc.
    }
}
```

### Sending from SRAM (not PROGMEM)

For a dynamically-built string in SRAM:

```cpp
static char sram_msg[64];
static const char* volatile udre_ptr = nullptr;

ISR(USART_UDRE_vect) {
    const char* p = udre_ptr;
    uint8_t c = *p;                    // direct dereference — SRAM
    if (c) {
        UDR0     = static_cast<uint8_t>(c);
        udre_ptr = p + 1;
    } else {
        USART0.disableUDREInterrupt();
        udre_ptr  = nullptr;
    }
}
```

---

## 16. TXC Interrupt

The TX Complete interrupt fires after the very last stop bit has left the wire — the physical line is now idle.

This is stricter than `flush()`:
- `flush()` polls `TXC0` by spinning — it ties up the CPU.
- The TXC ISR is non-blocking; the main thread is free until the ISR fires.

Primary use: **RS-485 direction control**. In half-duplex RS-485, a transceiver's DE (Driver Enable) pin must be low before the remote can start transmitting. If you lower DE too early (before the last stop bit), the bus shorts. The TXC ISR gives you the exact moment the line goes idle:

```cpp
#define DE_PIN PD2

static volatile bool txc_ready = false;

ISR(USART_TX_vect) {
    GPIO::clear(DE_PIN);    // release RS-485 bus
    txc_ready = true;
    USART0.disableTxInterrupt();
}

void rs485_send(const char* msg) {
    GPIO::set(DE_PIN);              // assert driver enable
    txc_ready = false;
    USART0.enableTxInterrupt();
    sei();
    USART0.write_P(PSTR(msg));      // transmit
    while (!txc_ready) {}           // ISR will clear DE
}
```

---

## 17. ISR Vector Names by MCU

AVR-libc defines different vector names per MCU family. Use the correct name or you will get a `__vector_N` undefined reference.

| MCU | RX Complete | TX Complete | UDRE |
|-----|-------------|-------------|------|
| ATmega328P | `USART_RX_vect` | `USART_TX_vect` | `USART_UDRE_vect` |
| ATmega32, ATmega16 | `USART_RXC_vect` | `USART_TXC_vect` | `USART_UDRE_vect` |
| ATmega64 (USART0) | `USART0_RX_vect` | `USART0_TX_vect` | `USART0_UDRE_vect` |
| ATmega64 (USART1) | `USART1_RX_vect` | `USART1_TX_vect` | `USART1_UDRE_vect` |
| ATmega128 (USART0) | `USART0_RX_vect` | `USART0_TX_vect` | `USART0_UDRE_vect` |
| ATmega128 (USART1) | `USART1_RX_vect` | `USART1_TX_vect` | `USART1_UDRE_vect` |

---

## 18. Baud Rate Accuracy

UBRR is an integer, so baud rate has a rounding error. Most hosts tolerate ±2 %. Above that, framing errors appear.

### Normal speed (U2X=0), 16 MHz

| Baud rate | UBRR | Actual baud | Error |
|-----------|------|-------------|-------|
| 2400 | 416 | 2400 | 0.0 % |
| 4800 | 207 | 4808 | +0.2 % |
| 9600 | 103 | 9615 | +0.2 % |
| 14400 | 68 | 14493 | +0.6 % |
| 19200 | 51 | 19231 | +0.2 % |
| 28800 | 34 | 28571 | −0.8 % |
| 38400 | 25 | 38462 | +0.2 % |
| 57600 | 16 | 58824 | +2.1 % |
| **115200** | **8** | **111111** | **−3.5 %** ⚠ |
| 250000 | 3 | 250000 | 0.0 % |
| 500000 | 1 | 500000 | 0.0 % |
| 1000000 | 0 | 1000000 | 0.0 % |

**115200 at 16 MHz has −3.5 % error.** This exceeds the ±2 % tolerance of some hosts. Enable double speed:

```cpp
USARTConfig cfg;
cfg.baudRate    = 115200;
cfg.doubleSpeed = true;   // UBRR = 16 → +2.1 % error (within tolerance)
USART0.begin(cfg);
```

### Double speed (U2X=1), 16 MHz

| Baud rate | UBRR | Actual baud | Error |
|-----------|------|-------------|-------|
| 9600 | 207 | 9615 | +0.2 % |
| 57600 | 33 | 58824 | +2.1 % |
| **115200** | **16** | **117647** | **+2.1 %** ✓ |
| 250000 | 7 | 250000 | 0.0 % |

### Best baud rates for 16 MHz

Zero-error baud rates: **250000, 500000, 1000000**. Excellent when both sides are AVR-based.

### F_CPU matters

Changing the crystal changes all UBRR values. The SDK recalculates automatically from `F_CPU` at compile time.

---

## 19. Multi-USART Devices

ATmega64 and ATmega128 have two USART hardware units, exposed as `USART0` and `USART1`. They are fully independent — each has its own baud rate, frame format, and interrupt enables.

```cpp
#include <mikroduino/usart.hpp>
using namespace MikroDuino;

int main() {
    USART0.begin(9600);       // debug terminal / PC
    USART1.begin(115200);     // GPS, Bluetooth module, etc.

    while (1) {
        if (USART1.available()) {
            uint8_t c = USART1.read();
            USART0.write(c);          // forward USART1 → USART0
        }
    }
}
```

ATmega328P has only one USART. Referencing `USART1` on a 328P is a compile-time error because the `Regs<1>` template specialisation is not defined for that MCU.

---

## 20. Recipes

### Formatted float (no `printf`)

`USARTDriver::writeFloat()` (see §9) covers the common case without pulling in `<stdio.h>`'s `printf`/`dtostrf`, which adds ~2 KB flash:

```cpp
USART0.writeFloat(3.14f, 2);    // prints "3.14"
```

If you need full `printf` and have the flash:

```cpp
#include <stdio.h>
char buf[16];
dtostrf(3.14159f, 6, 2, buf);   // AVR-specific: width, decimal places
USART0.write(buf);               // "  3.14"
```

### Accumulate a line (newline-terminated)

```cpp
static char  line[64];
static uint8_t line_pos = 0;

void poll_line() {
    while (USART0.available()) {
        char c = (char)USART0.read();
        if (c == '\r') continue;
        if (c == '\n' || line_pos >= 63) {
            line[line_pos] = '\0';
            handle_line(line);       // your handler
            line_pos = 0;
        } else {
            line[line_pos++] = c;
        }
    }
}
```

Call `poll_line()` on every main loop iteration.

### Transmit-only mode

```cpp
USARTConfig cfg;
cfg.baudRate = 9600;
cfg.mode     = USARTMode::TX;    // RXEN0=0; PD0 is free GPIO
USART0.begin(cfg);
```

### Switching baud rates at runtime

```cpp
USART0.flush();            // drain TX shift register
USART0.begin(115200);      // apply new rate instantly
USART0.writeLine_P(PSTR("Now at 115200."));
```

No need to call `end()` between `begin()` calls. `begin()` reconfigures the hardware directly.

### Simple binary packet protocol

```cpp
// Sender: length-prefixed frame
void send_packet(const uint8_t* data, uint8_t len) {
    USART0.write(0xAA);          // start marker
    USART0.write(len);
    for (uint8_t i = 0; i < len; ++i) USART0.write(data[i]);
    USART0.write(0x55);          // end marker
}

// Receiver (polling, assumes well-formed packets)
bool recv_packet(uint8_t* out, uint8_t* out_len) {
    if (!USART0.available()) return false;
    if (USART0.read() != 0xAA) return false;    // expect start marker
    uint8_t len = USART0.read();
    for (uint8_t i = 0; i < len; ++i) out[i] = USART0.read();
    *out_len = len;
    return USART0.read() == 0x55;               // verify end marker
}
```

---

## 21. Common Mistakes

### Garbled output — baud rate mismatch

Random characters, boxes, or nothing. Confirm both sides use the same baud rate. At 16 MHz, 115200 has −3.5 % error without double speed — enable `doubleSpeed = true` (see §18).

### TX/RX crossed

TX of the MCU must connect to RX of the adapter and vice versa. TX→TX / RX→RX receives nothing — swap them.

### First byte corrupted

The USART takes a few bit-times to stabilise after `begin()`. Add a small delay before the first `write()`:

```cpp
USART0.begin(9600);
_delay_ms(5);
USART0.writeLine_P(PSTR("Ready."));
```

### `read()` hangs forever

`read()` blocks until `RXC0=1`. If nothing is transmitting to the MCU, it never returns. Always guard:

```cpp
if (USART0.available()) {
    uint8_t c = USART0.read();
}
```

### SRAM overflow from string literals

See §10. Replace every `write("text")` and `writeLine("text")` with `write_P(PSTR("text"))` and `writeLine_P(PSTR("text"))`. The symptom is a silent crash or watchdog reset on startup — the `.data` section was larger than SRAM before `main()` ran.

### Data overrun in polling mode

If your main loop takes more than one frame-time between `available()` checks (at 9600 baud: ~1 ms), the hardware 2-byte FIFO fills up and the next byte is dropped (`DOR0` is set). Switch to interrupt-driven RX with a ring buffer (§14).

### Calling `write()` inside an ISR

`write()` spin-waits on `UDRE0`. Inside an ISR, the UDRE flag can only be set by the hardware completing a prior transmission, which requires the main thread or timer to advance. You will deadlock. Buffer your output and send in the main loop, or use the UDRE ISR pattern (§15).

### Enabling UDRIE before setting the pointer

The UDRE ISR fires the instant UDRIE is enabled (because UDR is empty). If `udre_ptr` is not set to a valid string first, the ISR will transmit garbage or fault. Always set `udre_ptr` before calling `enableUDREInterrupt()`.

### `flush()` with stale TXC0

If `TXC0` was set by a previous transmission and not cleared, `flush()` returns immediately without waiting for the current transmission to finish. Clear it first:

```cpp
UCSR0A |= (1 << TXC0);    // write 1 to clear TXC0 (per datasheet §19.6.2)
USART0.flush();
```

This matters only when calling `flush()` immediately after a mode switch or when interrupts are disabled.

---

*See also: [`Examples/core/usart/`](../Examples/core/usart/) — numbered walkthrough series covering this API, from a basic hello-world print through a readline command console. [`docs/ADC.md`](ADC.md) — same format for the ADC library.*
