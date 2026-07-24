# I2C (TWI) — MikroDuino SDK Reference Guide

The MikroDuino I2C library wraps the AVR hardware TWI (Two-Wire Interface) peripheral.
It exposes a single global instance `I2C` of type `I2CDriver` in the `MikroDuino`
namespace.  All operations are synchronous and polling-based except slave mode, which
optionally uses `ISR(TWI_vect)`.

---

## Include

```cpp
#include <mikroduino/i2c.hpp>
using namespace MikroDuino;
```

`I2C` is declared as a `static` instance in the header — no `.cpp` object file is
needed.  Just include the header and you have `I2C` ready to use.

---

## Hardware — TWI Pins by MCU

The TWI peripheral is mapped to fixed pins. These pins are **not** freely selectable
in software.

| MCU | SDA pin | SCL pin | Also used as |
|---|---|---|---|
| ATmega328P | **PC4** | **PC5** | ADC4, ADC5 |
| ATmega32 | **PC1** | **PC0** | — |
| ATmega16 | **PC1** | **PC0** | — |
| ATmega64 | **PD1** | **PD0** | RXD1 / TXD1 |
| ATmega128 | **PD1** | **PD0** | RXD1 / TXD1 |

> **Arduino Nano / Uno (ATmega328P):**  SDA = **A4**, SCL = **A5** on the pin header.

When TWI is active, the hardware takes ownership of those pins — do not use
`GPIO::output()` or `GPIO::write()` on them.

---

## Hardware — Pull-up Resistors

I2C is an open-drain bus.  Both SDA and SCL **must** have pull-up resistors to VCC.
The internal AVR pull-ups (~50 kΩ) are too weak — always use external resistors.

| Bus speed | Recommended Rp | Maximum Rp | Notes |
|---|---|---|---|
| 100 kHz (standard) | **4.7 kΩ** | 10 kΩ | Works up to ~400 pF bus capacitance |
| 400 kHz (fast) | **2.2 kΩ** | 3.9 kΩ | Rise time must be ≤ 300 ns |

**Rise time rule of thumb:**  `t_rise ≈ 0.85 × Rp × C_bus`

If you have many devices or long wires (high capacitance), lower Rp.  If voltage is
3.3 V instead of 5 V, lower Rp to maintain adequate drive current.

### Minimal wiring diagram

```
VCC ─┬── Rp ──┬── SDA (PC4/PC1/PD1 depending on MCU)
     │        │
     └── Rp ──┴── SCL (PC5/PC0/PD0 depending on MCU)

Each device: SDA and SCL wired in parallel on the same bus lines.
```

---

## Clock Speed — TWBR Formula

`beginMaster(clockHz)` computes TWBR automatically.  The SDK always uses prescaler = 1
(TWSR = 0x00).

```
TWBR = (F_CPU / clockHz - 16) / 2
```

| F_CPU | clockHz | TWBR | Actual frequency |
|---|---|---|---|
| 16 MHz | 100 000 | 72 | 100.0 kHz |
| 16 MHz | 400 000 | 12 | 400.0 kHz |
| 16 MHz | 50 000 | 152 | 50.0 kHz |
| 8 MHz | 100 000 | 32 | 100.0 kHz |
| 8 MHz | 400 000 | 2 | 400.0 kHz |

> **Minimum TWBR is 10** (AVR datasheet).  At 16 MHz this limits the maximum clock
> to ~444 kHz in practice, so 400 kHz (TWBR = 12) is the highest standard speed.

---

## I2CResult — Return Values

Every master-mode method returns `I2CResult`.  Always check it.

```cpp
enum class I2CResult : uint8_t {
    Ok,              // transaction completed successfully
    Timeout,         // TWINT never set within TIMEOUT_CYCLES (~10 000 loops)
    NackAddress,     // device did not ACK its own address (not present / wrong addr)
    NackData,        // address ACKed but a data byte was NACKed
    BusError,        // illegal START or STOP detected; TWSR == 0x00
    ArbitrationLost, // multi-master: another master won arbitration
};
```

**Pattern:**

```cpp
I2CResult r = I2C.write(addr, buf, len);
if (r != I2CResult::Ok) {
    // handle error — r tells you what went wrong
}
```

`Timeout` usually means no pull-ups, wrong MCU frequency (`F_CPU`), or a stuck-low
bus (a device holding SDA low after a partial transaction).

---

## TWIStatus — Bus Status Codes

`status()` returns `TWSR & 0xF8` — the 5-bit status field with prescaler bits masked.
The SDK uses these constants internally to interpret bus state after each TWI step.
You only need them directly when writing a custom slave ISR or debugging raw bus behavior.

```cpp
namespace TWIStatus {
    // Master transmitter
    constexpr uint8_t StartSent        = 0x08;  // START sent
    constexpr uint8_t RepStartSent     = 0x10;  // repeated START sent
    constexpr uint8_t SLAWAck         = 0x18;  // SLA+W sent, ACK received
    constexpr uint8_t SLAWNack        = 0x20;  // SLA+W sent, NACK received
    constexpr uint8_t DataWriteAck    = 0x28;  // data byte sent, ACK received
    constexpr uint8_t DataWriteNack   = 0x30;  // data byte sent, NACK received

    // Master receiver
    constexpr uint8_t SLARAAck        = 0x40;  // SLA+R sent, ACK received
    constexpr uint8_t SLARANack       = 0x48;  // SLA+R sent, NACK received
    constexpr uint8_t DataReadAck     = 0x50;  // data byte received, ACK returned
    constexpr uint8_t DataReadNack    = 0x58;  // data byte received, NACK returned

    // Slave receiver
    constexpr uint8_t SlaveRxSLAW     = 0x60;  // own SLA+W addressed
    constexpr uint8_t SlaveRxData     = 0x80;  // data byte received as slave

    // Slave transmitter
    constexpr uint8_t SlaveTxSLAR    = 0xA8;  // own SLA+R addressed
    constexpr uint8_t SlaveTxData    = 0xB8;  // data byte sent as slave

    // Error
    constexpr uint8_t BusError       = 0x00;  // illegal condition on bus
}
```

---

## API Reference

### `beginMaster(clockHz)`

```cpp
void beginMaster(uint32_t clockHz = 100000UL);
```

Initialises the TWI peripheral in **master mode**.  Sets TWBR and enables the TWEN
bit in TWCR.  Call once before any `write()`, `read()`, `writeRead()`, or `scan()`.

```cpp
I2C.beginMaster();          // 100 kHz (default)
I2C.beginMaster(100000);    // 100 kHz explicit
I2C.beginMaster(400000);    // 400 kHz fast mode
```

Call it again at any time to change speed.  No need to re-call after `scan()` or
regular read/write operations — the peripheral stays configured.

---

### `write(addr, data, len, stop)`

```cpp
I2CResult write(uint8_t address,
                const uint8_t* data,
                uint8_t len,
                bool stop = true);
```

Sends a master-write transaction:
`START → SLA+W → data[0] … data[len-1] → STOP`

| Parameter | Description |
|---|---|
| `address` | 7-bit device address (not shifted — SDK shifts it internally) |
| `data` | pointer to bytes to send |
| `len` | number of bytes |
| `stop` | `true` (default) sends STOP; `false` holds the bus for a repeated START |

```cpp
// Write a single register value to a device
uint8_t buf[2] = { 0x01, 0xFF };   // register addr + value
I2CResult r = I2C.write(0x3C, buf, 2);

// Write without STOP — keeps bus busy for a following read()
I2CResult r = I2C.write(0x50, reg, 2, false);
```

`stop = false` is used when you want to manually issue a repeated START followed by
`read()`.  `writeRead()` does this automatically — prefer it unless you need explicit
control.

---

### `read(addr, buf, len, stop)`

```cpp
I2CResult read(uint8_t address,
               uint8_t* buf,
               uint8_t len,
               bool stop = true);
```

Sends a master-read transaction:
`START → SLA+R → buf[0] … buf[len-2] (ACK) → buf[len-1] (NACK) → STOP`

The last byte always receives a NACK to signal the device to release the bus.

```cpp
uint8_t data[6];
I2CResult r = I2C.read(0x68, data, 6);
if (r == I2CResult::Ok) {
    // data[0..5] contain the received bytes
}
```

`stop = false` is valid here too — it holds the bus after reading.

---

### `writeRead(addr, wBuf, wLen, rBuf, rLen)`

```cpp
I2CResult writeRead(uint8_t addr,
                    const uint8_t* wBuf, uint8_t wLen,
                    uint8_t* rBuf,       uint8_t rLen);
```

Combined write-then-read with a **repeated START** between them.  This is the
standard pattern for reading a register from a device — no other master can steal
the bus between the write and read phases.

Internal sequence:
```
START → SLA+W → wBuf[0..wLen-1] → rep-START → SLA+R → rBuf[0..rLen-1] → STOP
```

This is equivalent to `write(addr, wBuf, wLen, false)` followed by
`read(addr, rBuf, rLen, true)`.

```cpp
// Read 6 bytes from DS3231 RTC starting at register 0x00
uint8_t reg  = 0x00;
uint8_t time[6];
I2CResult r = I2C.writeRead(0x68, &reg, 1, time, 6);

// Read 8 bytes from AT24C32 EEPROM at memory address 0x0010
uint8_t addr[2] = { 0x00, 0x10 };
uint8_t data[8];
I2CResult r = I2C.writeRead(0x50, addr, 2, data, 8);
```

---

### `scan(callback)`

```cpp
template<typename Callback>
void scan(Callback cb);
```

Probes every 7-bit address from 1 to 126.  For each address that ACKs the SLA+W
frame, `cb(address)` is called.  Devices that do not respond are silently skipped.

`cb` can be a function pointer, a lambda, or any callable.  Use a lambda to capture
context:

```cpp
I2C.scan([](uint8_t addr) {
    // called for each found device
    // print addr, build a device list, etc.
});

// Lambda capturing a found-count variable
uint8_t count = 0;
I2C.scan([&](uint8_t addr) {
    ++count;
});
```

`scan` calls `beginMaster` internally — no need to call it separately before scanning.
However, if you change speed before scanning, call `beginMaster(speed)` first.

---

### `beginSlave(address, generalCall)`

```cpp
void beginSlave(uint8_t address, bool generalCall = false);
```

Switches the TWI peripheral to **slave mode**.  The hardware listens for its own
address on the bus and triggers `ISR(TWI_vect)` on each bus event (when
`enableInterrupt()` is also called).

| Parameter | Description |
|---|---|
| `address` | 7-bit slave address this MCU will respond to |
| `generalCall` | `false` (default) — ignore address 0x00; `true` — also respond to general-call broadcasts |

```cpp
I2C.beginSlave(0x42);          // respond to 0x42 only
I2C.beginSlave(0x42, true);    // respond to 0x42 + general-call (0x00)
```

After `beginSlave()`, call `enableInterrupt()` and then `sei()` to activate the ISR.
To resume master mode, call `beginMaster()`.

---

### `enableInterrupt()` / `disableInterrupt()`

```cpp
void enableInterrupt();    // sets   TWIE in TWCR
void disableInterrupt();   // clears TWIE in TWCR
```

Controls the TWI interrupt enable bit (TWIE).  When set, `ISR(TWI_vect)` fires on
every TWI event — required for slave mode to work.

```cpp
I2C.beginSlave(0x42);
I2C.enableInterrupt();
sei();   // global interrupt enable (avr/interrupt.h)

// ... later, to stop:
cli();
I2C.disableInterrupt();
I2C.beginMaster(100000);   // switch back to master
```

These are also useful in master mode to selectively mask TWI interrupts without
affecting other peripherals.

---

### `status()`

```cpp
uint8_t status() const;
```

Returns `TWSR & 0xF8` — the current TWI status code with the two prescaler bits
(TWPS1:0) masked out.  Compare against `TWIStatus::*` constants.

```cpp
uint8_t s = I2C.status();
if (s == TWIStatus::SLAWAck) {
    // address was ACKed
}
```

**When to call it:**
- Inside `ISR(TWI_vect)` to decide the next bus action (slave mode).
- After a failed `I2CResult` to inspect the raw hardware state.
- During debugging to trace the exact bus step where a transaction failed.

**When NOT to call it:**
- After `sendStop()` — the status register may hold a stale value.
  Read `status()` before the STOP is issued for a reliable result.

---

## Usage Patterns

### 1 — Write a register (single byte)

```cpp
I2C.beginMaster(100000);

uint8_t msg[2] = { 0x01, 0x80 };   // register address, value
I2CResult r = I2C.write(0x3C, msg, 2);
```

### 2 — Read a register (standard repeated-start)

```cpp
uint8_t reg  = 0x00;
uint8_t data[2];

I2CResult r = I2C.writeRead(0x68, &reg, 1, data, 2);
```

### 3 — Raw sequential read (device with auto-increment pointer)

Some devices (PCF8574, PCF8591, DS1307) maintain an internal address counter.
After a `writeRead` sets the pointer, subsequent `read()` calls continue from there:

```cpp
// Set pointer to register 0x00
uint8_t reg = 0x00;
I2C.writeRead(0x68, &reg, 1, buf, 1);

// Now read the next 6 bytes without re-sending register address
uint8_t more[6];
I2C.read(0x68, more, 6);
```

### 4 — EEPROM page write (AT24Cxx)

```cpp
// AT24C32: 2-byte memory address + up to 32 data bytes per page
uint8_t payload[10];
payload[0] = 0x00;                            // memory address high byte
payload[1] = 0x00;                            // memory address low byte
for (uint8_t i = 0; i < 8; ++i)
    payload[2 + i] = i;

I2C.write(0x50, payload, sizeof(payload));
delay_ms(10);   // AT24Cxx needs ≥5 ms write-cycle time before next access
```

### 5 — EEPROM random read

```cpp
uint8_t addr[2] = { 0x00, 0x00 };
uint8_t data[8];
I2C.writeRead(0x50, addr, 2, data, 8);
```

### 6 — Bus scan

```cpp
I2C.beginMaster(100000);
I2C.scan([](uint8_t addr) {
    // addr found on bus
});
```

### 7 — Slave mode with ISR

```cpp
#include <avr/interrupt.h>
#include <mikroduino/i2c.hpp>
using namespace MikroDuino;

static volatile uint8_t rx_buf[8];
static volatile uint8_t rx_len = 0;

ISR(TWI_vect) {
    uint8_t s = I2C.status();

    if (s == TWIStatus::SlaveRxSLAW) {
        rx_len = 0;
        TWCR = (1 << TWINT) | (1 << TWEA) | (1 << TWEN) | (1 << TWIE);

    } else if (s == TWIStatus::SlaveRxData) {
        if (rx_len < sizeof(rx_buf))
            rx_buf[rx_len++] = TWDR;
        TWCR = (1 << TWINT) | (1 << TWEA) | (1 << TWEN) | (1 << TWIE);

    } else if (s == TWIStatus::SlaveTxSLAR) {
        TWDR = 0x42;   // first byte to send to master
        TWCR = (1 << TWINT) | (1 << TWEA) | (1 << TWEN) | (1 << TWIE);

    } else {
        TWCR = (1 << TWINT) | (1 << TWEA) | (1 << TWEN) | (1 << TWIE);
    }
}

int main() {
    I2C.beginSlave(0x42);
    I2C.enableInterrupt();
    sei();

    while (true) {
        // main loop — ISR captures bus events
    }
}
```

**ISR TWCR write rule:**  After every TWI event you must write TWCR with
`TWINT | TWEN | TWIE` (and optionally `TWEA` to ACK the next byte) to release the
bus and re-arm the interrupt.  Forgetting this stalls the bus.

### 8 — Switching between master and slave

```cpp
// Start as master
I2C.beginMaster(100000);
I2C.write(0x50, buf, 4);

// Switch to slave for a window
I2C.beginSlave(0x42);
I2C.enableInterrupt();
sei();
// ... handle bus events in ISR ...
cli();
I2C.disableInterrupt();

// Back to master
I2C.beginMaster(100000);
```

---

## Common Devices — Address and Register Reference

| Device | Default address | Notes |
|---|---|---|
| AT24C32 EEPROM | 0x50 | A0–A2 pins selectable → 0x50–0x57 |
| AT24C64 EEPROM | 0x50 | same as AT24C32 |
| DS3231 RTC | 0x68 | fixed, not configurable |
| DS1307 RTC | 0x68 | fixed |
| PCF8574 I/O expander | 0x20–0x27 | A0–A2 selectable |
| PCF8574A I/O expander | 0x38–0x3F | A0–A2 selectable |
| SSD1306 OLED | 0x3C or 0x3D | SA0 pin selects |
| MPU-6050 IMU | 0x68 or 0x69 | AD0 pin selects |
| BMP280 sensor | 0x76 or 0x77 | SDO pin selects |
| PCF8591 ADC/DAC | 0x48–0x4F | A0–A2 selectable |

---

## Pitfalls and Common Mistakes

### Missing pull-up resistors
The TWI bus will appear stuck or return `Timeout` / `BusError` immediately.
Always add 4.7 kΩ (100 kHz) or 2.2 kΩ (400 kHz) from both SDA and SCL to VCC.

### Wrong F_CPU
TWBR is computed from `F_CPU`.  If `F_CPU` does not match the actual clock, the
generated I2C frequency will be wrong.  Devices rated for 100 kHz may not respond
if the actual clock is much higher.  Confirm `F_CPU` in your `.mdp` file.

### EEPROM write-cycle guard time
After writing to an AT24Cxx EEPROM, the device is busy for up to 5 ms completing
the internal write.  Any `write()` or `writeRead()` during this window returns
`NackAddress`.  Wait at least 5 ms (10 ms is safe) before the next access.

### Page boundary on EEPROM writes
AT24C32 has 32-byte pages.  A write that crosses a page boundary wraps around
within the page — bytes beyond the boundary overwrite the start of the same page.
Always keep writes within one page or split them.

### status() after sendStop is stale
The TWI hardware clears TWINT and updates TWSR only when a new bus event completes.
After `sendStop()` there is no new event, so `status()` still shows the code from
the last step before the STOP.  This is usually `DataWriteAck` (0x28) or
`DataReadNack` (0x58).

### Forgetting sei() in slave mode
`enableInterrupt()` sets TWIE, but ISR(TWI_vect) cannot fire unless the global
interrupt flag (I bit in SREG) is set.  Call `sei()` after `enableInterrupt()`.

### Shared bus between master and slave on same MCU
An ATmega cannot simultaneously be master and slave on the same bus in hardware.
`beginSlave()` overwrites the TWCR configuration from `beginMaster()`.  Switch
explicitly with `beginMaster()` / `beginSlave()` as shown in pattern 8.

### Address 0 (general call)
Do not probe address 0 with `write()` — it is the I2C general-call address and
will trigger all devices that have general-call enabled.  The `scan()` function
correctly starts from address 1 to avoid this.

---

## Example Projects

Worked `.mdp` projects covering this API live under
[`Examples/core/i2c/`](../Examples/core/i2c/) — a numbered walkthrough series
from a basic bus scanner through EEPROM read/write, RTC time set/get, and a
combined EEPROM+RTC datalogger. Open any `.mdp` in the MikroDuino IDE, build,
and flash.
