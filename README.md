# MikroDuino

MikroDuino is an AVR development environment for the ATmega family
(`ATmega328P`, `ATmega32`, `ATmega16`, `ATmega64`, `ATmega128`). It has two
parts: a desktop **IDE** (`apps/ide/`, Python/PyQt6) for writing, building,
and flashing firmware, and a C++17 **SDK** (`sdk/`) that wraps AVR hardware
registers into readable classes plus a set of reusable module drivers for
common peripherals (displays, sensors, motors, keypads, etc.).

This file is a consolidated index of every document under [`docs/`](docs/)
and [`sdk/docs/`](sdk/docs/), with a one-line summary of every library in the
SDK. For architecture, build commands, and contributor conventions, see
[`CLAUDE.md`](CLAUDE.md) and [`ARCHITECTURE.md`](ARCHITECTURE.md).

New to the project? Start with [`docs/getting-started.md`](docs/getting-started.md).

---

## Guide documents (`docs/`)

| Document | Covers |
|---|---|
| [`getting-started.md`](docs/getting-started.md) | Installing the IDE, first project, `.mdp` schema, build/flash flow, walkthroughs of GPIO/USART/ADC/PWM/Timers/Interrupts/LCD, common errors |
| [`arduino-mode.md`](docs/arduino-mode.md) | Tutorial-style intro to the Arduino compatibility shim (`ARDUINO_MAIN()`, pin numbering, `setup()`/`loop()`, full example walkthrough) |
| [`Arduino.md`](docs/Arduino.md) | Full reference for the Arduino shim — every function's hardware effect, timing, SRAM/PROGMEM notes, mixing rules with native MikroDuino APIs, common mistakes |
| [`registers.md`](docs/registers.md) | `registers.hpp` bit/mask/field macros, atomic blocks, compiler-hint attributes — the foundation every other module is built on |
| [`gpio.md`](docs/gpio.md) | `GPIO` class — pin constants, direction/output/input, port-wide batch operations |
| [`usart-reference.md`](docs/uart-reference.md) | `USARTDriver` — registers, config enums, transmit/receive API, PROGMEM strings, interrupt-driven RX/TX, baud accuracy tables |
| [`adc.md`](docs/ADC.md) | `ADCDriver` — registers, prescaler/timing tables, blocking/free-running/interrupt-driven reads, MCU differences |
| [`pwm.md`](docs/pwm.md) | `PWM1Driver` (Timer1) — Fast/Phase-Correct PWM theory, frequency/resolution tradeoffs, servo and LED-fade examples |
| [`timer.md`](docs/timer.md) | `Timer0Driver` / `Timer1Driver` — Normal/CTC/PWM modes, prescalers, `ticksForHz()`, ISR and flag-polling patterns |
| [`spi.md`](docs/spi.md) | `SPIDriver` — master/slave setup, clock/mode/bit-order enums, manual CS management, bus-sharing patterns |
| [`i2c.md`](docs/i2c.md) | `I2CDriver` (TWI) — master/slave, `writeRead()` repeated-start pattern, bus scan, common device address table |
| [`interrupts.md`](docs/interrupts.md) | `InterruptManager` — external INT0–INT7, sense modes, debouncing, ISR safety rules |
| [`eeprom.md`](docs/eeprom.md) | `EEPROMDriver` — write-cycle internals, `update()` for endurance, typed `get<T>()`/`put()`, interrupt-driven writes |

---

## SDK core hardware peripherals

Always available via `#include <mikroduino/mikroduino.hpp>` — no opt-in guard
needed. All live under `sdk/core/avr/include/mikroduino/`. This is the whole
SDK core in this release — there are no opt-in utility-library guards
(`MD_INCLUDE_*`) beyond what's listed here.

| Class | Header | Wraps |
|---|---|---|
| `GPIO` | `gpio.hpp` | Digital I/O, port-wide batch ops |
| `USART0` / `USART1` | `usart.hpp` | Serial (UART), PROGMEM strings, interrupts |
| `SPI` | `spi.hpp` | Master/slave SPI, manual CS |
| `I2C` | `i2c.hpp` | Master/slave TWI, bus scan |
| `ADC_Driver` | `adc.hpp` | 10-bit analog input, free-running/interrupt modes |
| `Timer0` / `Timer1` | `timer.hpp` | Normal/CTC/PWM timer modes |
| `PWM1` | `pwm.hpp` | 16-bit PWM (Timer1), percent or raw duty |
| `Interrupt` / `InterruptManager` | `interrupt.hpp` | External INT0–INT7 |
| `EEPROM` | `eeprom.hpp` | Non-volatile byte/block/typed storage |
| — | `registers.hpp` | `BITSET`/`BITCLEAR`/`FIELD_SET`/`ATOMIC_BLOCK_*` macros used by everything above |
| — | `platform.hpp` | MCU detection, capability flags, `F_CPU` guard |

### Arduino compatibility shim (`sdk/compat/`)

Opt-in, non-invasive layer providing `pinMode`, `digitalWrite`, `analogRead`,
`analogWrite`, `Serial`, `millis`/`micros`, `delay`, `map`/`constrain`,
`random`, `pulseIn`, and Arduino-style pin numbering (`A0`–`A5`) on top of
the core peripherals above. Requires `Arduino::beginTimekeeping()` before
using `millis()`/`micros()` (takes ownership of Timer0). See
[`docs/Arduino.md`](docs/Arduino.md).

---

## Module drivers (`sdk/modules/`)

Not pulled in by `mikroduino.hpp` — add each module's `include/` directory to
the build's include path and compile its `.cpp` file where noted. Full
write-ups (wiring, API, examples) live in
[`sdk/docs/core-libraries.md`](sdk/docs/core-libraries.md); every module also
has its own `README.md` (linked below) that either points at that write-up
or is a stub waiting for one. All of this is also browsable from inside the
IDE via **Help → Documentation…**.

| Module | Header | Summary |
|---|---|---|
| [`LCD`](sdk/modules/LCD/README.md) | `LCD.hpp` | HD44780-compatible character LCD, 4-bit mode |
| [`DS3231`](sdk/modules/DS3231/README.md) | `DS3231.hpp` | I2C real-time clock |
| [`DHT22`](sdk/modules/DHT22/README.md) | `DHT22.hpp` | 1-Wire temperature/humidity sensor |
| [`HCSR04`](sdk/modules/HCSR04/README.md) | `HCSR04.hpp` | Ultrasonic distance sensor (trigger/echo) |
| [`MAX72xx`](sdk/modules/MAX72xx/README.md) | `MAX72xx.hpp` | SPI dot-matrix LED driver |
| [`SSD1306`](sdk/modules/SSD1306/README.md) | `SSD1306.hpp` | 128×64 I2C OLED — 1024-byte framebuffer, text/graphics/bitmap/scroll |
| [`ST7735`](sdk/modules/ST7735/README.md) | `ST7735.hpp` | SPI color TFT (128×160 / 128×128 / 160×80, RGB565, no framebuffer) |
| [`SevenSeg`](sdk/modules/SevenSeg/README.md) | `SevenSeg.hpp` (+ `.cpp`) | Single 7-segment digit, Timer2 auto-multiplex or manual |
| [`SevenSegShift`](sdk/modules/SevenSegShift/README.md) | `SevenSegShift.hpp` (+ `.cpp`) | Shift-register-driven multiplexed 7-segment display (2–8 digits) |
| [`Servo`](sdk/modules/Servo/README.md) | `Servo.hpp` | Hardware PWM (Timer1) hobby servo driver, 50 Hz / 0.5 µs resolution |
| [`Stepper`](sdk/modules/Stepper/README.md) | `Stepper.hpp` | STEP/DIR driver for A4988, DRV8825, TMC2208/2209, etc. |
| [`DCMotor`](sdk/modules/DCMotor/README.md) | `DCMotor.hpp` | Two-pin direction control + optional PWM speed |
| [`IRSensor`](sdk/modules/IRSensor/README.md) | `IRSensor.hpp` | Single IR reflectance/proximity sensor, digital or analog |
| [`IRArray`](sdk/modules/IRArray/README.md) | `IRArray.hpp` | N-sensor IR line array with weighted centre-of-mass line position |
| [`IRRemote`](sdk/modules/IRRemote/README.md) | `IRRemote.hpp` (+ `.cpp`) | NEC-protocol IR remote receiver (Timer2 + INT0/INT1) |
| [`MatrixKeypad`](sdk/modules/MatrixKeypad/README.md) | `MatrixKeypad.hpp` | Header-only `MatrixKeypad<ROWS,COLS>` row/column scanner with ghost-key rejection |
| [`Button`](sdk/modules/Button/README.md) | `Button.hpp` | Header-only debounced button with long-press and double-click detection |
| [`RotaryEncoder`](sdk/modules/RotaryEncoder/README.md) | `RotaryEncoder.hpp` (+ `.cpp`) | ISR-driven quadrature rotary encoder, self-counting |
| [`Pulse`](sdk/modules/Pulse/README.md) | `Pulse.hpp` (+ `.cpp`) | `Stopwatch` and `PulseMeter` time measurement classes (Timer1) |

---

## Supported MCUs

`ATmega328P`, `ATmega32`, `ATmega16`, `ATmega64`, `ATmega128`.
Definitions: `packages/shared/src/constants/mcu-definitions.ts` (TypeScript
packages) and `builder.py::MCU_MAP` (Python IDE).

---

## Where to go next

- **Build/run the IDE, project file schema, build & flash flow:** [`CLAUDE.md`](CLAUDE.md)
- **System design, module boundaries, future Electron IDE:** [`ARCHITECTURE.md`](ARCHITECTURE.md)
- **Worked examples:** [`Examples/`](Examples/) — one `.mdp` project per topic, all built around the core peripherals and module drivers above. `Examples/core/` has numbered walkthroughs per peripheral (`gpio`, `usart`, `adc`, `pwm`, `timer`, `spi`, `i2c`, `interrupts`, `eeprom`); `Examples/Modules/` has a numbered series per module driver. Browse them from inside the IDE via **File → Examples**.
