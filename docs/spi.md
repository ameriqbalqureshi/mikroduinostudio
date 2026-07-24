# MikroDuino SPI Reference

## Overview

`MikroDuino::SPIDriver` gives full access to the ATmega328P hardware SPI peripheral
in both master and slave mode. Chip-select (CS) is intentionally **not managed** by
the driver — the caller asserts and releases every CS line manually. This makes it
straightforward to share the bus across multiple devices with different CS pins.

Include:
```cpp
#include <mikroduino/spi.hpp>
// — or —
#include <mikroduino/mikroduino.hpp>   // entire SDK
using namespace MikroDuino;
```

`SPI` is a pre-instantiated global. Never create your own `SPIDriver`.

---

## SPI Pins (ATmega328P)

| Signal | Pin | Arduino Uno | Direction (master) |
|--------|-----|-------------|-------------------|
| MOSI | PB3 | 11 | Output |
| MISO | PB4 | 12 | Input |
| SCK  | PB5 | 13 | Output |
| SS   | PB2 | 10 | Output (must stay output to remain master) |

`beginMaster()` configures all four pins automatically. In slave mode `beginSlave()`
swaps the directions: MISO becomes an output, the rest become inputs.

---

## Enumerations

### `SPIClockDiv`

Controls the SCK frequency relative to `F_CPU`. The driver picks the correct
combination of `SPCR.SPR` bits and the `SPSR.SPI2X` doubler bit automatically.

| Value | Divisor | SCK @ 16 MHz |
|-------|---------|-------------|
| `SPIClockDiv::DIV2` | ÷2 | 8 MHz |
| `SPIClockDiv::DIV4` | ÷4 | 4 MHz |
| `SPIClockDiv::DIV8` | ÷8 | 2 MHz |
| `SPIClockDiv::DIV16` | ÷16 | 1 MHz *(default)* |
| `SPIClockDiv::DIV32` | ÷32 | 500 kHz |
| `SPIClockDiv::DIV64` | ÷64 | 250 kHz |
| `SPIClockDiv::DIV128` | ÷128 | 125 kHz |

Choose the fastest clock your device datasheet allows. Slower speeds add latency
and rarely improve reliability unless the wiring is very long or noisy.

---

### `SPIMode`

SPI defines four bus modes by combining clock polarity (CPOL) and clock phase (CPHA).
Check your device datasheet — it will say "SPI Mode 0" or show a timing diagram with
the clock idle state and the sample edge.

| Value | CPOL | CPHA | Clock idle | Sample edge |
|-------|------|------|-----------|------------|
| `SPIMode::Mode0` | 0 | 0 | LOW | Rising *(most common)* |
| `SPIMode::Mode1` | 0 | 1 | LOW | Falling |
| `SPIMode::Mode2` | 1 | 0 | HIGH | Falling |
| `SPIMode::Mode3` | 1 | 1 | HIGH | Rising |

Mode0 is the default and works with the majority of SPI peripherals (74HC595,
SD cards, most ADCs and DACs). Mode3 is functionally equivalent to Mode0 for many
devices. Mode1 and Mode2 are less common but required by some sensors and displays.

---

### `SPIBitOrder`

| Value | Description |
|-------|-------------|
| `SPIBitOrder::MSBFirst` | Bit 7 transmitted first *(default, most devices)* |
| `SPIBitOrder::LSBFirst` | Bit 0 transmitted first |

Most SPI devices expect MSB first. Check the datasheet — some LED controllers and
certain RF modules use LSB first.

---

## API Reference

### `beginMaster(clockDiv, mode, order)`

```cpp
void beginMaster(
    SPIClockDiv clockDiv = SPIClockDiv::DIV16,
    SPIMode     mode     = SPIMode::Mode0,
    SPIBitOrder order    = SPIBitOrder::MSBFirst);
```

Configure the SPI peripheral as a **master** and enable it. Sets MOSI, SCK, and SS
as outputs; MISO as input. Must be called before any transfer.

All three parameters are optional and default to the most common settings.

```cpp
SPI.beginMaster();                                           // 1 MHz, Mode0, MSB first
SPI.beginMaster(SPIClockDiv::DIV2);                         // 8 MHz, Mode0, MSB first
SPI.beginMaster(SPIClockDiv::DIV8, SPIMode::Mode3);         // 2 MHz, Mode3, MSB first
SPI.beginMaster(SPIClockDiv::DIV16, SPIMode::Mode0,
                SPIBitOrder::LSBFirst);                      // 1 MHz, Mode0, LSB first
```

Calling `beginMaster()` a second time safely reconfigures the peripheral at runtime —
useful for driving multiple devices on the same bus that need different speeds or modes.
Call it before asserting each device's CS if their settings differ.

---

### `beginSlave(mode, order)`

```cpp
void beginSlave(
    SPIMode     mode  = SPIMode::Mode0,
    SPIBitOrder order = SPIBitOrder::MSBFirst);
```

Configure the SPI peripheral as a **slave**. Reconfigures pins: MISO (PB4) becomes
an output, MOSI/SCK/SS (PB3/PB5/PB2) become inputs. An external master controls SCK
and SS.

```cpp
SPI.beginSlave();                              // Mode0, MSB first
SPI.beginSlave(SPIMode::Mode3, SPIBitOrder::LSBFirst);
```

In slave mode, an incoming transfer triggers the `SPI_STC_vect` interrupt (if enabled).
The received byte is read from `SPDR`; any byte written to `SPDR` before or during the
transfer is clocked out on MISO to the master.

---

### `end()`

```cpp
void end();
```

Disable the SPI peripheral by clearing the `SPE` bit in `SPCR`. The pins remain in
their current direction. Re-enable with `beginMaster()` or `beginSlave()`.

```cpp
SPI.end();   // shut down — connected output-latching devices hold their last value
```

---

### `transfer(data)` — single byte

```cpp
uint8_t transfer(uint8_t data);
```

Transmit `data` on MOSI and simultaneously receive a byte on MISO. Blocks until the
8-bit transfer is complete (SPIF flag set), then returns the received byte.

```cpp
uint8_t rx = SPI.transfer(0xAB);   // send 0xAB, receive whatever the device returns
SPI.transfer(0x00);                 // dummy byte — receive only (discard TX)
```

CS is **not** asserted by this function. Assert it before calling and release it after:

```cpp
GPIO::clear(CS);
SPI.transfer(0xAB);
GPIO::set(CS);
```

---

### `transfer(txBuf, rxBuf, len)` — buffer

```cpp
void transfer(const uint8_t* txBuf, uint8_t* rxBuf, uint16_t len);
```

Transfer `len` bytes, storing received bytes into `rxBuf`. Equivalent to calling
the single-byte `transfer()` in a loop but cleaner for multi-byte frames.

```cpp
const uint8_t cmd[]  = { 0x03, 0x00, 0x10, 0x00 };   // read command + address
uint8_t       resp[4];

GPIO::clear(CS);
SPI.transfer(cmd, resp, 4);   // send 4 bytes, receive 4 bytes simultaneously
GPIO::set(CS);

// resp[0] is received during cmd[0] transmission, etc.
```

TX and RX happen simultaneously (full-duplex). If you only need to transmit, pass
a scratch buffer for `rxBuf`. If you only need to receive, fill `txBuf` with `0x00`
dummy bytes.

---

### `transferComplete()`

```cpp
bool transferComplete() const;
```

Returns `true` if the SPIF flag is set — a transfer has completed and `SPDR` holds
the received byte. Returns `false` while a transfer is in progress.

Use this when you write to `SPDR` directly (without going through `transfer()`) and
want to poll for completion without blocking:

```cpp
GPIO::clear(CS);
SPDR = 0xF0;                          // kick off a transfer — non-blocking
// ... do other work here ...
while (!SPI.transferComplete()) {}    // poll until done
uint8_t result = SPDR;                // reading SPDR also clears SPIF
GPIO::set(CS);
```

> **Note**: the single-byte `transfer()` already waits for SPIF internally — calling
> `transferComplete()` immediately after `transfer()` will always return `true`.
> `transferComplete()` is only useful when you start the transfer yourself by writing
> `SPDR` directly.

---

### `enableInterrupt()` / `disableInterrupt()`

```cpp
void enableInterrupt();
void disableInterrupt();
```

Enable or disable the SPI Transfer Complete interrupt (`SPIE` bit in `SPCR`). When
enabled, `SPI_STC_vect` fires each time a transfer completes (SPIF set). Global
interrupts must be enabled via `sei()` for the ISR to execute.

```cpp
volatile bool g_done = false;

ISR(SPI_STC_vect) {
    g_done = true;         // transfer is complete; SPDR holds received byte
}

// In main:
sei();
SPI.enableInterrupt();

g_done = false;
GPIO::clear(CS);
SPDR = 0xA5;               // start transfer
while (!g_done) {}         // yield until ISR fires
uint8_t rx = SPDR;
GPIO::set(CS);

SPI.disableInterrupt();
```

In slave mode, `SPI_STC_vect` fires each time the master completes an 8-bit transfer.
Read the received byte immediately in the ISR before the next transfer overwrites it.

---

## CS Management

The driver provides **no chip-select management**. This is a deliberate design choice:
the right CS strategy varies too much between devices (active-low vs active-high,
pulse vs hold, timing requirements) to be hidden in a generic driver.

The recommended pattern is a thin per-device wrapper:

```cpp
static constexpr uint8_t CS_EEPROM  = PB2;
static constexpr uint8_t CS_DISPLAY = PD7;

static void eeprom_select()   { GPIO::clear(CS_EEPROM); }
static void eeprom_release()  { GPIO::set(CS_EEPROM);   }
```

For multi-device buses, keep all CS pins high (deasserted) on startup:

```cpp
GPIO::output(CS_EEPROM);   GPIO::set(CS_EEPROM);
GPIO::output(CS_DISPLAY);  GPIO::set(CS_DISPLAY);
SPI.beginMaster();
```

---

## Sharing the Bus

Multiple SPI devices can share MOSI, MISO, and SCK as long as each has its own CS
pin. If devices need different modes or speeds, reconfigure with `beginMaster()`
before each transaction:

```cpp
// Talk to a 74HC595 (Mode0, 8 MHz)
SPI.beginMaster(SPIClockDiv::DIV2, SPIMode::Mode0);
GPIO::clear(CS_595);
SPI.transfer(data);
GPIO::set(CS_595);

// Talk to an MCP3204 ADC (Mode0, 2 MHz — slower device)
SPI.beginMaster(SPIClockDiv::DIV8, SPIMode::Mode0);
GPIO::clear(CS_ADC);
SPI.transfer(cmd, rx, 3);
GPIO::set(CS_ADC);
```

---

## Examples

### Write to a 74HC595 shift register

74HC595 is a classic SPI output expander: 8 serial bits in → 8 parallel outputs.
RCLK (latch) is the CS line; a rising edge copies the shift register to the outputs.

```cpp
#include <mikroduino/spi.hpp>
#include <mikroduino/gpio.hpp>
#include <util/delay.h>
using namespace MikroDuino;

static constexpr uint8_t LATCH = PB2;

int main() {
    SPI.beginMaster();          // Mode0, MSBFirst, 1 MHz
    GPIO::set(LATCH);           // latch idle high

    while (true) {
        for (uint8_t i = 0; i < 8; i++) {
            GPIO::clear(LATCH);
            SPI.transfer(static_cast<uint8_t>(1 << i));
            GPIO::set(LATCH);   // rising edge latches to QA–QH
            _delay_ms(100);
        }
    }
}
```

---

### Read from a 25LC256 SPI EEPROM

```cpp
#include <mikroduino/spi.hpp>
#include <mikroduino/gpio.hpp>
using namespace MikroDuino;

static constexpr uint8_t CS = PB2;

uint8_t eeprom_read(uint16_t addr) {
    uint8_t tx[4] = { 0x03,                       // READ command
                      static_cast<uint8_t>(addr >> 8),
                      static_cast<uint8_t>(addr),
                      0x00 };                       // dummy byte (data comes here)
    uint8_t rx[4];

    GPIO::clear(CS);
    SPI.transfer(tx, rx, 4);
    GPIO::set(CS);

    return rx[3];   // received during the dummy byte
}

int main() {
    SPI.beginMaster(SPIClockDiv::DIV4, SPIMode::Mode0);
    GPIO::output(CS);
    GPIO::set(CS);

    uint8_t val = eeprom_read(0x0010);
    (void)val;

    while (true) {}
}
```

---

### LSBFirst — driving a device with reversed bit order

```cpp
SPI.beginMaster(SPIClockDiv::DIV16, SPIMode::Mode0, SPIBitOrder::LSBFirst);

GPIO::clear(CS);
SPI.transfer(0b00000001);   // bit 0 goes out first → device sees LSB on first clock
GPIO::set(CS);
```

---

### Interrupt-driven slave receiver

```cpp
#include <mikroduino/spi.hpp>
#include <avr/interrupt.h>
using namespace MikroDuino;

volatile uint8_t g_rx_byte = 0;
volatile bool    g_rx_ready = false;

ISR(SPI_STC_vect) {
    g_rx_byte  = SPDR;
    g_rx_ready = true;
}

int main() {
    SPI.beginSlave();
    SPI.enableInterrupt();
    sei();

    while (true) {
        if (g_rx_ready) {
            g_rx_ready = false;
            // process g_rx_byte
        }
    }
}
```

---

## Notes

**SS must stay output in master mode.**
If SS (PB2) is configured as an input and pulled low by external hardware, the AVR
SPI hardware automatically switches to slave mode — even if `beginMaster()` was called.
`beginMaster()` sets PB2 as an output to prevent this. Never call `GPIO::input(PB2)`
while operating as an SPI master.

**CS is your responsibility.**
`transfer()` does not touch any CS pin. If you forget to assert CS before a transfer,
the device ignores the bytes. If you forget to release CS after, the device stays
selected and will misinterpret the next transaction's bytes as a continuation.

**`SPDR` read clears SPIF.**
Reading `SPDR` (or calling `transfer()` which reads it internally) clears the SPIF
flag. Do not read `SPDR` more than once per transfer — the second read returns
undefined data and you will miss the next transfer-complete event.

**SPI conflicts with `timer.hpp` — none.**
The SPI peripheral (SPCR/SPSR/SPDR) is entirely independent of the timer registers.
Using `SPI` alongside `PWM1` or `Timer0` is safe.

**No `transfer()` inside `SPI_STC_vect`.**
The blocking `transfer()` calls `WAIT_BITSET(SPSR, SPIF)`. Calling it inside the
SPI ISR will spin forever because SPIF was already cleared when the ISR was entered.
In slave ISRs, write the next response byte directly to `SPDR`.

---

## Quick Reference

| Task | Call |
|------|------|
| Start as master (defaults) | `SPI.beginMaster()` |
| Start as master, 8 MHz | `SPI.beginMaster(SPIClockDiv::DIV2)` |
| Start as master, Mode3 | `SPI.beginMaster(SPIClockDiv::DIV16, SPIMode::Mode3)` |
| Start as master, LSB first | `SPI.beginMaster(SPIClockDiv::DIV16, SPIMode::Mode0, SPIBitOrder::LSBFirst)` |
| Start as slave | `SPI.beginSlave()` |
| Disable SPI | `SPI.end()` |
| Send/receive one byte | `uint8_t rx = SPI.transfer(tx)` |
| Send/receive a buffer | `SPI.transfer(txBuf, rxBuf, len)` |
| Poll for completion | `bool done = SPI.transferComplete()` |
| Enable transfer interrupt | `SPI.enableInterrupt()` |
| Disable transfer interrupt | `SPI.disableInterrupt()` |
| Assert CS (active-low) | `GPIO::clear(CS_PIN)` |
| Release CS | `GPIO::set(CS_PIN)` |
