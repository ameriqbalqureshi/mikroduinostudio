# MikroDuino IDE — Getting Started Guide

A beginner's guide to writing AVR firmware with the MikroDuino IDE and SDK.

---

## Table of Contents

1. [What is MikroDuino?](#1-what-is-mikroduino)
2. [Installing and Running the IDE](#2-installing-and-running-the-ide)
3. [Tour of the IDE](#3-tour-of-the-ide)
4. [Creating Your First Project](#4-creating-your-first-project)
5. [Project Structure — the `.mdp` File](#5-project-structure--the-mdp-file)
6. [Adding Libraries — the Libraries Panel](#6-adding-libraries--the-libraries-panel)
7. [Building and Flashing](#7-building-and-flashing)
8. [Serial Monitor](#8-serial-monitor)
9. [Importing Arduino Sketches (.ino)](#9-importing-arduino-sketches-ino)
10. [Examples and Samples](#10-examples-and-samples)
11. [GPIO — Raw AVR Registers](#11-gpio--raw-avr-registers)
12. [GPIO — MikroDuino Library](#12-gpio--mikroduino-library)
13. [USART — Serial Communication](#13-usart--serial-communication)
14. [ADC — Reading Analog Values](#14-adc--reading-analog-values)
15. [PWM — Variable Output](#15-pwm--variable-output)
16. [Timers](#16-timers)
17. [External Interrupts](#17-external-interrupts)
18. [Character LCD](#18-character-lcd)
19. [Finding More Help](#19-finding-more-help)
20. [Common Errors](#20-common-errors)

---

## 1. What is MikroDuino?

MikroDuino is an AVR development environment built for the ATmega family of microcontrollers (`ATmega328P`, `ATmega32`, `ATmega16`, `ATmega64`, `ATmega128`). It has two parts:

- **The IDE** — a desktop application (Python 3 / PyQt6) where you write code, build it, and flash it to your chip. This is what this guide covers.
- **The SDK** (`sdk/`) — a C++17 header library that wraps AVR hardware registers into readable classes (`GPIO`, `USART0`, `ADC_Driver`, `PWM1`, `Timer1`, `CharLCD`, …), plus a growing set of opt-in module drivers.

You do **not** need to use the SDK. Sections 11–18 of this guide show both approaches for the core peripherals — raw AVR registers first, then the SDK equivalent — so you can choose the style that suits you. You can also write plain Arduino-style `setup()`/`loop()` sketches via the Arduino compatibility shim (see [`docs/arduino-mode.md`](arduino-mode.md)) or import an existing `.ino` sketch directly (section 9).

There is also a planned Electron/TypeScript IDE (`packages/`, architecture only, not yet scaffolded) — not relevant to this guide.

---

## 2. Installing and Running the IDE

### Option A — Windows installer (recommended for end users)

`installer/MikroDuinoIDE-Setup-<version>.exe` is a self-contained installer built from `packaging/installer.iss` — it bundles the IDE itself, the avr-gcc toolchain, and avrdude. Run it and launch **MikroDuino IDE** from the Start Menu; nothing else needs to be installed.

### Option B — Run from source (contributors, Linux/macOS, or to track `main`)

```
cd apps/ide
pip install -r requirements.txt   # PyQt6, pyserial
python main.py
```

Requirements:

| Tool | Notes |
|------|-------|
| Python 3.10+ | any recent CPython works |
| `PyQt6` / `pyserial` | installed by `pip install -r requirements.txt` |
| avr-gcc + avrdude | **Windows**: already vendored under `tools/toolchain/` in the repo — nothing to install. **Linux/macOS**: install `gcc-avr`/`avr-libc` and `avrdude` yourself (`sudo apt install gcc-avr avr-libc avrdude` on Debian/Ubuntu) and make sure they're on `PATH` — the vendored toolchain is Windows-only. |

The IDE looks for the bundled toolchain first and silently falls back to whatever `avr-gcc`/`avrdude` it finds on `PATH`, so both setups work without extra configuration.

---

## 3. Tour of the IDE

When it opens, the IDE shows a welcome screen until you open or create a project. The layout:

- **Menu bar** — File, Edit, View, Build, Tools, Help.
- **Toolbar** — New, Open, Save, Build, Flash, Properties, and the current project's name on the right.
- **EXPLORER** (left dock) — the current project's file tree. Right-click for New File/Folder, Rename, Move, Delete.
- **LIBRARIES** (right dock) — browse and insert Core Peripherals, Core Utilities, SDK Modules, and installed Arduino libraries. Covered in section 6.
- **Editor tabs** (centre) — C/C++ syntax highlighting and autocomplete (type 2+ characters, or press **Ctrl+Space** to force it; `.`/`->` triggers member completion).
- **OUTPUT** (bottom dock) — two tabs: **BUILD OUTPUT** (compiler/avrdude log) and **SERIAL MONITOR** (section 8).
- **View menu** — toggle any of the three docks (Explorer / Bottom Panel / Libraries) on or off.

### Menu reference

| Menu | Contains |
|---|---|
| **File** | New/Open Project, **Projects Folder** (one-click list of `.mdp` files under your configured sketchbook folder), **Examples** (section 10), **Import Sketch (.ino)…** (section 9), Save / Save All, Project Properties, Exit |
| **Edit** | Undo/Redo, Cut/Copy/Paste, Select All, **Preferences…** (`Ctrl+,`) — editor font/size, tab size, word wrap, UI font size, output font size, and the Projects Folder path |
| **View** | Toggle Explorer / Bottom Panel / Libraries docks |
| **Build** | Build Project (`F7`), Rebuild All (`Ctrl+F7`), Clean, Flash to Device (`F8`), Project Properties |
| **Tools** | Library Manager… (`Ctrl+L`), Project Properties |
| **Help** | **Documentation…** (`F1`) — an in-IDE browser for every guide, SDK library reference, and module README (this file included); About |

---

## 4. Creating Your First Project

1. **File → New Project…**.
2. Choose a parent folder (defaults to your configured Projects Folder — see Preferences) and enter a project name, e.g. `BlinkLED`.
3. The IDE creates this layout and opens it:

```
BlinkLED/
  BlinkLED.mdp      ← project configuration
  src/
    main.cpp        ← starter blink example, ready to build
```

4. Open **Build → Project Properties…** (or click the gear icon in the toolbar) to set:
   - **MCU** — e.g. `ATmega328P`
   - **Clock** — MHz
   - **Programmer** — `ARDUINO` for a bootloader on an Uno-style board, `USBASP`/`AVRISP`/`STK500`/`DRAGON`/`JTAG` for a standalone ISP programmer
   - **Port** — e.g. `COM3` on Windows or `/dev/ttyUSB0` on Linux (a dropdown lists currently detected ports)
   - **Optimisation / Warnings / C++ standard**
   - **Include Directories** — add/remove extra include paths (used by section 6's Libraries panel automatically; you rarely need to touch this by hand)
5. Click **OK** — settings are saved into the `.mdp` file immediately.

---

## 5. Project Structure — the `.mdp` File

The project file is plain JSON. You can edit it by hand, though Project Properties (section 4) and the Libraries panel (section 6) cover almost everything you'd normally need:

```json
{
  "projectName": "BlinkLED",
  "version": "1.0.0",
  "target": { "mcu": "ATmega328P", "clock": 16000000 },
  "programmer": { "type": "ARDUINO", "port": "COM3", "baudRate": 115200 },
  "build": {
    "optimization": "O2",
    "warnings": "all",
    "cppStandard": "c++17",
    "includeDirs": [],
    "extraSources": []
  },
  "sourceFiles": ["src/main.cpp"]
}
```

- **`sourceFiles`** — every `.c`/`.cpp` the builder compiles. If you omit this field entirely, the builder auto-discovers all C/C++ files in the project folder (legacy `.mdp` support).
- **`build.includeDirs`** — extra `-I` paths, relative to the project folder (or absolute).
- **`build.extraSources`** — extra `.c`/`.cpp` files to compile alongside `sourceFiles` (e.g. a module driver's implementation file), relative to the project folder or absolute.

### The SDK core (Layer 1) needs no configuration

`sdk/core/avr/include/` and `sdk/core/avr/include/mikroduino/` are **always** on the include path for every project — you can `#include <mikroduino/gpio.hpp>` (or any other Layer-1 header, or the umbrella `<mikroduino/mikroduino.hpp>`) with zero changes to `includeDirs`. This is what sections 12, 14–17 rely on.

### Module drivers and Arduino libraries need wiring

Everything else — `sdk/modules/*` drivers (LCD, sensors, motors, …) and third-party Arduino libraries — needs an `includeDirs`/`extraSources` entry before you can use it. **Don't hand-edit this** — use the Libraries panel described next; it does the correct thing for whichever kind of library you pick.

---

## 6. Adding Libraries — the Libraries Panel

The **LIBRARIES** dock (right side) is a single place to browse everything available to a project and insert it correctly — no manual JSON editing needed. It has four sections, and double-clicking (or pressing Enter on) any leaf item inserts the right `#include` at your cursor and wires up the project file if that's required:

| Section | What double-click does |
|---|---|
| **Core Peripherals** | `GPIO`, `USART`, `SPI`, `I2C`, `ADC`, `Timer`, `PWM`, `Interrupt`, `EEPROM`, `Registers`, `Platform` — just inserts `#include <mikroduino/X.hpp>`. No project-file wiring needed (see section 5) and no project even needs to be open, just an editor tab. |
| **Core Utilities** | Auto-discovered opt-in headers under `sdk/core/avr/include/mikroduino/<group>/`, grouped and inserted the same way as Core Peripherals plus a `#define MD_INCLUDE_<GUARD>` before the umbrella include. This release ships no such headers, so the section shows empty — it exists for forward compatibility if a future release adds one. |
| **SDK Modules** | Every driver under `sdk/modules/` (LCD, DS3231, DHT22, HCSR04, MAX72xx, SSD1306, ST7735, SevenSeg, SevenSegShift, Servo, Stepper, DCMotor, IRSensor, IRArray, IRRemote, MatrixKeypad, Button, RotaryEncoder, Pulse). Adds the module's `include/` dir to `build.includeDirs` and its `.cpp` (if it has one) to `build.extraSources`, saves the `.mdp`, then inserts `#include <ModuleName.hpp>`. Requires an open project. |
| **Arduino Libraries** | Third-party libraries installed via the Library Manager (below) or dropped into a project's `libraries/` folder or the global `~/.mikroduino/libraries/`. Just inserts the primary header's `#include` — no project-file wiring at all, because the builder scans every source file's `#include`s at build time and automatically compiles/links whatever installed library matches (transitively, one level deep). |

Use the filter box above the tree to search by name, and the refresh button to pick up libraries installed while the IDE is running.

### Library Manager (Tools → Library Manager…, `Ctrl+L`, or the panel's "Manage Arduino Libraries…" button)

Installs go to `~/.mikroduino/libraries/`, shared by every project. Two install paths:
- **Install from .ZIP** — pick a local `.zip` (Arduino-library layout).
- **Install from GitHub / URL** — paste a repo URL or a direct `.zip` link (e.g. a Releases asset); it downloads and installs automatically.

Remove installed libraries from the same dialog's left-hand list.

---

## 7. Building and Flashing

Press **F7** to build. The BUILD OUTPUT tab shows each step:

```
=== Building BlinkLED  [atmega328p  16 MHz] ===
    Build dir: C:\...\BlinkLED\build

  Compiling  src/main.cpp
  $ avr-g++ -mmcu=atmega328p -DF_CPU=16000000UL -O2 -std=c++17 -Wall -c ...

  Linking…
  Generating HEX…

Device: atmega328p
Program:     178 bytes (0.5% Full)
...
=== Build successful ===
```

**Ctrl+F7** (Rebuild All) deletes `build/` first. **Clean** (Build menu) just deletes `build/` without rebuilding.

Press **F8** to flash. The board must be connected with the correct **Port** and **Programmer** set in Project Properties. **Close the Serial Monitor first** — avrdude and the Serial Monitor cannot share the same COM port; the IDE does not disconnect it for you automatically.

---

## 8. Serial Monitor

The **SERIAL MONITOR** tab (bottom dock, next to BUILD OUTPUT) is a standalone terminal — independent of the project's configured programmer port, so you can point it at any detected port:

- **Port** dropdown (editable — type a path directly if it's not listed) + refresh button
- **Baud** dropdown: 9600 / 19200 / 38400 / 57600 / 115200 / 230400
- **Connect/Disconnect** toggle, **Clear** button
- Text field at the bottom — type and press Enter (or click **Send**) to transmit

Switching to this tab automatically refreshes the port list. Use it to see `USART0.writeLine(...)`/`Serial.println(...)` output without leaving the IDE — but remember to disconnect before flashing (section 7).

---

## 9. Importing Arduino Sketches (.ino)

**File → Import Sketch (.ino)…** converts an existing Arduino sketch into a MikroDuino project source file:

- Adds `#include <Arduino.h>` if the sketch doesn't already have it.
- If it detects both `setup()` and `loop()`, prepends `ARDUINO_MAIN()` — a macro from the [Arduino compatibility shim](Arduino.md) that expands to a correct `int main()` calling your `setup()`/`loop()`.
- Sibling `.ino` files in the same folder (multi-tab sketches) are converted the same way, without a second `main()`.
- Sibling `.h`/`.hpp` files are copied over unchanged.

The build system detects `#include <Arduino.h>` (and `<SPI.h>`, `<Wire.h>`, `<WString.h>`, `<Print.h>`) automatically and compiles the matching compat source files for you — no `includeDirs`/`extraSources` wiring needed for Arduino-mode code, same as core peripherals. See [`docs/arduino-mode.md`](arduino-mode.md) for a full tutorial and [`docs/Arduino.md`](Arduino.md) for the complete shim reference.

---

## 10. Examples and Samples

**File → Examples** has three submenus:

- **Arduino Library Examples** — examples bundled with each installed Arduino library.
- **Examples with IDE** — native `.mdp` example projects under `Examples/core/` — numbered walkthroughs per peripheral (`gpio`, `usart`, `adc`, `pwm`, `timer`, `spi`, `i2c`, `interrupts`, `eeprom`).
- **Samples** — native `.mdp` projects under `Examples/Modules/`, one numbered series per module driver (`Examples/Modules/<Name>`).

Selecting one copies it into your Projects Folder and opens it — the original under `Examples/` is never modified.

---

## 11. GPIO — Raw AVR Registers

This approach uses the AVR hardware registers directly — exactly what compilers and debuggers understand, with zero overhead.

### Concepts

Each I/O port has three registers:

| Register | Purpose |
|----------|---------|
| `DDRx` | Data Direction — 1 = output, 0 = input |
| `PORTx` | Output value (when output) or pull-up enable (when input) |
| `PINx` | Read current logic level on the pin |

`x` is the port letter: `B`, `C`, `D` on ATmega328P. Each register is 8 bits wide — one bit per pin.

### Example — Blink LED on PB5

```cpp
#include <avr/io.h>
#include <util/delay.h>

int main() {
    // Set PB5 as output
    DDRB |= (1 << DDB5);

    while (1) {
        PORTB |=  (1 << PORTB5);   // HIGH — LED on
        _delay_ms(500);
        PORTB &= ~(1 << PORTB5);   // LOW  — LED off
        _delay_ms(500);
    }
}
```

The `(1 << DDB5)` idiom shifts the value 1 left by 5 positions, producing the bitmask `0b00100000`. `|=` sets that bit; `&= ~(...)` clears it.

### Example — Button input on PD2

```cpp
#include <avr/io.h>

int main() {
    DDRB  |=  (1 << DDB5);    // PB5 output (LED)
    DDRD  &= ~(1 << DDD2);    // PD2 input  (button)
    PORTD |=  (1 << PORTD2);  // enable internal pull-up

    while (1) {
        if (!(PIND & (1 << PIND2))) {
            // Button pressed — pin pulled LOW through the button
            PORTB |=  (1 << PORTB5);  // LED on
        } else {
            PORTB &= ~(1 << PORTB5);  // LED off
        }
    }
}
```

The button connects the pin to GND. The internal pull-up holds it HIGH when the button is open; pressing it drives it LOW — hence the `!` (not) check.

### Example — Toggle with `PINB`

Writing a 1 to a bit in `PINx` **toggles** the corresponding `PORTx` bit. This is the fastest way to flip a pin:

```cpp
#include <avr/io.h>
#include <util/delay.h>

int main() {
    DDRB |= (1 << DDB5);

    while (1) {
        PINB = (1 << PINB5);  // toggle PB5
        _delay_ms(500);
    }
}
```

---

## 12. GPIO — MikroDuino Library

The SDK's `GPIO` class provides named functions for the same operations. The key difference is **pin encoding**: instead of separate port and bit arguments, every pin is a single `uint8_t` constant that encodes both:

```
MikroDuino pin = (port_index << 3) | bit_number
```

So `PD5` = `(3 << 3) | 5` = `29`. You never calculate this yourself — you just use the constants (`PB0`…`PD7` etc.) defined in `gpio.hpp`. No `includeDirs` setup is needed (see section 5) — just `#include`, or double-click **GPIO** in the Libraries panel (section 6).

```cpp
#include <mikroduino/gpio.hpp>
#include <util/delay.h>

using namespace MikroDuino;
```

### API summary

```cpp
GPIO::output(pin);          // set pin as output
GPIO::input(pin);           // set pin as input (no pull-up)
GPIO::inputPullup(pin);     // set pin as input + enable pull-up

GPIO::set(pin);             // drive HIGH
GPIO::clear(pin);           // drive LOW
GPIO::toggle(pin);          // flip output state
GPIO::write(pin, true);     // set or clear based on bool

bool level = GPIO::read(pin);  // read current input level
```

### Example — Blink LED on PB5

```cpp
#include <mikroduino/gpio.hpp>
#include <util/delay.h>

using namespace MikroDuino;

int main() {
    GPIO::output(PB5);

    while (1) {
        GPIO::set(PB5);
        _delay_ms(500);
        GPIO::clear(PB5);
        _delay_ms(500);
    }
}
```

### Example — Button on PD2 controls LED on PB5

```cpp
#include <mikroduino/gpio.hpp>
using namespace MikroDuino;

int main() {
    GPIO::output(PB5);
    GPIO::inputPullup(PD2);   // internal pull-up, button connects to GND

    while (1) {
        // read() returns false when button pressed (pin pulled LOW)
        GPIO::write(PB5, !GPIO::read(PD2));
    }
}
```

### Pin constants by port (ATmega328P)

| Port B | Port C | Port D |
|--------|--------|--------|
| PB0–PB7 | PC0–PC6 | PD0–PD7 |

---

## 13. USART — Serial Communication

USART lets the chip talk to a PC over a USB-serial adapter, or to other devices. On ATmega328P, USART0 uses:
- **TX** — PD1 (transmit, connect to RX of adapter)
- **RX** — PD0 (receive, connect to TX of adapter)

### Raw AVR approach

```cpp
#include <avr/io.h>

#define BAUD     9600UL
#define UBRR_VAL (F_CPU / 16 / BAUD - 1)

static void uart_init() {
    UBRR0H = (uint8_t)(UBRR_VAL >> 8);
    UBRR0L = (uint8_t)(UBRR_VAL);
    UCSR0B = (1 << TXEN0) | (1 << RXEN0);   // enable TX and RX
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00); // 8 data bits, 1 stop, no parity
}

static void uart_putc(char c) {
    while (!(UCSR0A & (1 << UDRE0)));  // wait for TX buffer empty
    UDR0 = c;
}

static void uart_puts(const char *s) {
    while (*s) uart_putc(*s++);
}

static char uart_getc() {
    while (!(UCSR0A & (1 << RXC0)));   // wait for received byte
    return UDR0;
}

int main() {
    uart_init();
    uart_puts("Hello!\r\n");

    while (1) {
        char c = uart_getc();   // echo back
        uart_putc(c);
    }
}
```

### MikroDuino SDK approach

```cpp
#include <mikroduino/usart.hpp>
using namespace MikroDuino;

int main() {
    USART0.begin(9600);         // 9600-8-N-1
    USART0.writeLine("Hello!");

    while (1) {
        if (USART0.available()) {
            uint8_t c = USART0.read();
            USART0.write(c);    // echo
        }
    }
}
```

### Advanced configuration

```cpp
USARTConfig cfg;
cfg.baudRate  = 115200;
cfg.dataBits  = USARTDataBits::Eight;
cfg.parity    = USARTParity::None;
cfg.stopBits  = USARTStopBits::One;
cfg.mode      = USARTMode::TX_RX;
USART0.begin(cfg);
```

### Sending numbers

```cpp
USART0.write("Value: ");
USART0.writeInt(42);         // decimal
USART0.writeLine("");        // newline

USART0.writeHex(0xDEAD);    // prints "0x0000DEAD"
```

### Serial Monitor

See section 8 — the IDE's built-in Serial Monitor connects to any port you choose and shows your `USART0.writeLine(...)` output without leaving the IDE.

---

## 14. ADC — Reading Analog Values

The ADC converts a voltage on an analog input pin to a 10-bit number (0–1023). On ATmega328P the analog channels are on `PC0`–`PC5` (ADC0–ADC5).

### Raw AVR approach

```cpp
#include <avr/io.h>

static void adc_init() {
    ADMUX  = (1 << REFS0);   // AVcc reference, channel 0
    ADCSRA = (1 << ADEN)     // enable ADC
           | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); // prescaler /128
}

static uint16_t adc_read(uint8_t channel) {
    ADMUX  = (ADMUX & 0xF0) | (channel & 0x0F); // select channel
    ADCSRA |= (1 << ADSC);                       // start conversion
    while (ADCSRA & (1 << ADSC));                // wait for completion
    return ADC;                                   // 10-bit result
}
```

### MikroDuino SDK approach

```cpp
#include <mikroduino/adc.hpp>
using namespace MikroDuino;

int main() {
    ADC_Driver.begin();          // AVcc reference, /128 prescaler — good for 16 MHz

    while (1) {
        uint16_t raw = ADC_Driver.read(0);   // read ADC channel 0 (PC0)

        // Convert to millivolts (5000 mV reference, 10-bit = 1024 steps)
        uint16_t mV = (uint32_t)raw * 5000 / 1023;
    }
}
```

### Reference options

```cpp
ADC_Driver.begin(ADCRef::AVCC);      // use VCC (most common)
ADC_Driver.begin(ADCRef::Internal);  // 1.1 V internal reference
ADC_Driver.begin(ADCRef::AREF);      // external voltage on AREF pin
```

### Full example — read potentiometer, send over UART

```cpp
#include <mikroduino/mikroduino.hpp>
#include <stdio.h>
using namespace MikroDuino;

int main() {
    USART0.begin(9600);
    ADC_Driver.begin();

    char buf[32];
    while (1) {
        uint16_t raw = ADC_Driver.read(0);
        uint16_t mV  = (uint32_t)raw * 5000 / 1023;

        // sprintf needs <stdio.h>
        sprintf(buf, "ADC=%4u  mV=%4u\r\n", raw, mV);
        USART0.write(buf);

        // simple delay using raw register (or _delay_ms from <util/delay.h>)
        for (volatile uint32_t i = 0; i < 200000UL; ++i);
    }
}
```

---

## 15. PWM — Variable Output

PWM (Pulse Width Modulation) rapidly switches a pin between HIGH and LOW. By changing the **duty cycle** (the fraction of time spent HIGH) you can control LED brightness, motor speed, or generate audio tones.

On ATmega328P the hardware PWM outputs are:

| Channel | Pin | Timer |
|---------|-----|-------|
| OC0A | PD6 | Timer0 |
| OC0B | PD5 | Timer0 |
| OC1A | PB1 | Timer1 (16-bit) |
| OC1B | PB2 | Timer1 (16-bit) |
| OC2A | PB3 | Timer2 |
| OC2B | PD3 | Timer2 |

### Raw AVR approach (Timer1, OC1A on PB1)

```cpp
#include <avr/io.h>
#include <util/delay.h>

static void pwm_init() {
    DDRB  |= (1 << DDB1);   // OC1A as output

    // Fast PWM, ICR1 as TOP, non-inverting on OC1A
    // Frequency = F_CPU / (prescaler * (ICR1 + 1))
    // With /8 prescaler and ICR1=19999: ~100 Hz
    ICR1   = 19999;
    TCCR1A = (1 << COM1A1) | (1 << WGM11);
    TCCR1B = (1 << WGM13)  | (1 << WGM12) | (1 << CS11); // prescaler /8
}

static void pwm_duty(uint8_t percent) {
    OCR1A = (uint16_t)(20000UL * percent / 100);
}

int main() {
    pwm_init();

    while (1) {
        // Fade up
        for (uint8_t d = 0; d <= 100; ++d) {
            pwm_duty(d);
            _delay_ms(20);
        }
        // Fade down
        for (uint8_t d = 100; d > 0; --d) {
            pwm_duty(d);
            _delay_ms(20);
        }
    }
}
```

### MikroDuino SDK approach

`PWM1` uses Timer1 and controls OC1A (PB1) and OC1B (PB2). It auto-selects the prescaler so the output frequency is as accurate as possible.

```cpp
#include <mikroduino/pwm.hpp>
#include <util/delay.h>
using namespace MikroDuino;

int main() {
    PWM1.begin(1000);       // 1 kHz PWM frequency

    while (1) {
        // Fade up
        for (uint8_t d = 0; d <= 100; ++d) {
            PWM1.dutyA(d);  // 0–100 %
            _delay_ms(20);
        }
        // Fade down
        for (uint8_t d = 100; d > 0; --d) {
            PWM1.dutyA(d);
            _delay_ms(20);
        }
    }
}
```

Control both channels independently:

```cpp
PWM1.begin(500);       // 500 Hz
PWM1.dutyA(75);        // PB1 at 75 %
PWM1.dutyB(25);        // PB2 at 25 %
PWM1.stopA();          // turn off PB1 (reverts pin to GPIO)
```

---

## 16. Timers

Timers count clock ticks independently of your main code. Use them to fire events at precise intervals — blinking LEDs without `_delay_ms`, measuring elapsed time, generating PWM, and more.

ATmega328P has:
- **Timer0** — 8-bit, counts 0–255
- **Timer1** — 16-bit, counts 0–65535 (best for timing)
- **Timer2** — 8-bit async, can use an external 32.768 kHz crystal

### CTC mode (Clear Timer on Compare)

In CTC mode the timer counts from 0 to a value you set in `OCRnA`, then resets to 0. You can trigger an interrupt at each reset to get a precise periodic callback.

### Raw AVR approach — 1 Hz LED toggle using Timer1 CTC

```cpp
#include <avr/io.h>
#include <avr/interrupt.h>

// Timer1 CTC: compare A = F_CPU / (prescaler * frequency) - 1
// 16 MHz / (256 * 1 Hz) - 1 = 62499
ISR(TIMER1_COMPA_vect) {
    PINB = (1 << PINB5);    // toggle PB5 (LED)
}

int main() {
    DDRB  |= (1 << DDB5);

    OCR1A  = 62499;
    TCCR1B = (1 << WGM12) | (1 << CS12);  // CTC, prescaler /256
    TIMSK1 = (1 << OCIE1A);               // enable compare A interrupt
    sei();                                 // enable global interrupts

    while (1) { /* ISR does the work */ }
}
```

### MikroDuino SDK approach

```cpp
#include <mikroduino/timer.hpp>
#include <avr/interrupt.h>
using namespace MikroDuino;

ISR(TIMER1_COMPA_vect) {
    GPIO::toggle(PB5);
}

int main() {
    GPIO::output(PB5);

    Timer1.mode(TimerMode::CTC);
    Timer1.prescaler(TimerPrescaler::DIV256);
    Timer1.compareA(Timer1.ticksForHz(1));  // calculates 62499 for 1 Hz at /256
    Timer1.enableInterruptA();
    Timer1.start();

    InterruptManager::enableGlobal();

    while (1) {}
}
```

`ticksForHz(n)` calculates the correct compare value based on the prescaler you chose, so you never do the arithmetic by hand.

---

## 17. External Interrupts

External interrupts (INT0, INT1) let you react to a pin change instantly, without polling. On ATmega328P: **INT0 = PD2**, **INT1 = PD3**.

### Raw AVR approach

```cpp
#include <avr/io.h>
#include <avr/interrupt.h>

ISR(INT0_vect) {
    // button pressed on PD2 — toggle LED
    PINB = (1 << PINB5);
}

int main() {
    DDRB  |=  (1 << DDB5);    // LED output
    DDRD  &= ~(1 << DDD2);    // PD2 input
    PORTD |=  (1 << PORTD2);  // pull-up

    EICRA |= (1 << ISC01);    // trigger on falling edge
    EIMSK |= (1 << INT0);     // enable INT0
    sei();

    while (1) {}
}
```

### MikroDuino SDK approach

```cpp
#include <mikroduino/mikroduino.hpp>
using namespace MikroDuino;

static void onButton() {
    GPIO::toggle(PB5);
}

int main() {
    GPIO::output(PB5);
    GPIO::inputPullup(PD2);

    InterruptManager::attach(IntSource::INT0, onButton, IntSense::Falling);
    InterruptManager::enableGlobal();

    while (1) {}
}
```

`InterruptManager::attach()` stores your function pointer and registers the interrupt in one call. Detach with `InterruptManager::detach(IntSource::INT0)`.

### Sense options

| `IntSense` | Triggers when |
|------------|--------------|
| `Low` | Pin is LOW (fires continuously while held) |
| `Change` | Pin changes state (either edge) |
| `Falling` | Pin goes HIGH→LOW |
| `Rising` | Pin goes LOW→HIGH |

---

## 18. Character LCD

The MikroDuino SDK includes a driver (`CharLCD`, in `sdk/modules/LCD/`) for HD44780-compatible character LCDs (16×2, 20×4, …) in 4-bit parallel mode.

### Wiring (16×2 LCD to ATmega328P)

```
LCD pin  | AVR pin | Notes
---------|---------|-----------------------------
VSS      | GND     |
VDD      | 5 V     |
V0       | pot     | 10 kΩ contrast pot wiper
RS       | PD2     | Register Select
RW       | GND     | Tie to ground — always write
EN       | PD3     | Enable
D0–D3   | —       | Not connected (4-bit mode)
D4       | PD4     |
D5       | PD5     |
D6       | PD6     |
D7       | PD7     |
A (LED+) | 5 V     | Backlight (220 Ω series if needed)
K (LED-) | GND     |
```

### Adding it to your project

Double-click **LCD** under **SDK Modules** in the Libraries panel (section 6) — it wires up `include_dirs`/`extraSources` and inserts `#include <LCD.hpp>` for you. (Equivalent by hand: add `sdk/modules/LCD/include` to `build.includeDirs` and `sdk/modules/LCD/src/LCD.cpp` to `build.extraSources` in the `.mdp`.)

### API

```cpp
CharLCD lcd(RS, EN, D4, D5, D6, D7, cols, rows);

lcd.begin();                    // initialise — must be called first
lcd.clear();                    // clear display, cursor to (0,0)
lcd.home();                     // cursor to (0,0) without clearing
lcd.setCursor(col, row);        // move cursor; col and row are 0-based
lcd.print("hello");             // write a string
lcd.print(int32_t value);       // write a decimal integer
lcd.printHex(uint8_t value);    // write "AB" style hex for one byte
lcd.writeChar(uint8_t c);       // write a single character (or CGRAM index)

lcd.display(bool on);           // turn display on/off
lcd.cursor(bool on);            // show/hide underscore cursor
lcd.blink(bool on);             // blinking block cursor on/off
lcd.scrollLeft();               // shift all text one position left
lcd.scrollRight();              // shift all text one position right
lcd.createChar(index, bitmap);  // define custom 5×8 character (index 0–7)
```

### Example — Hello World

```cpp
#include <LCD.hpp>
#include <util/delay.h>
using namespace MikroDuino;

static CharLCD lcd(PD2, PD3, PD4, PD5, PD6, PD7);

int main() {
    lcd.begin();
    lcd.print("Hello, World!");
    lcd.setCursor(0, 1);   // second line
    lcd.print("MikroDuino");

    while (1) {}
}
```

### Example — Counter with custom character

```cpp
#include <LCD.hpp>
#include <util/delay.h>
using namespace MikroDuino;

static CharLCD lcd(PD2, PD3, PD4, PD5, PD6, PD7);

// 5×8 heart glyph stored in CGRAM slot 0
static const uint8_t HEART[8] = {
    0b00000,
    0b01010,
    0b11111,
    0b11111,
    0b01110,
    0b00100,
    0b00000,
    0b00000,
};

int main() {
    lcd.begin();
    lcd.createChar(0, HEART);   // load into CGRAM

    lcd.setCursor(0, 0);
    lcd.writeChar(0);           // print the heart
    lcd.print(" Heartbeat");

    int32_t count = 0;
    while (1) {
        lcd.setCursor(0, 1);
        lcd.print(count++);
        lcd.print("      ");    // overwrite leftover digits
        _delay_ms(500);
    }
}
```

---

## 19. Finding More Help

This guide covers the essentials; deeper reference material is one keystroke away:

- **Help → Documentation… (`F1`)** — an in-IDE browser covering this guide, the Arduino compatibility reference, per-peripheral deep dives (registers, GPIO, USART, ADC, PWM, timers, SPI, I2C, interrupts, EEPROM), the full module-driver reference (`sdk/docs/core-libraries.md`), and one page per SDK module (`sdk/modules/<Name>/README.md`). It's searchable and stays up to date automatically as those files are added to or edited.
- **`sdk/docs/core-libraries.md`** — worked examples for every module driver (`sdk/modules/`).
- **[`docs/Arduino.md`](Arduino.md)** and **[`docs/arduino-mode.md`](arduino-mode.md)** — full Arduino-compatibility-shim reference and tutorial.
- **[`CLAUDE.md`](../CLAUDE.md)** / **[`ARCHITECTURE.md`](../ARCHITECTURE.md)** (repo root) — codebase architecture, build system, and contributor conventions.

---

## 20. Common Errors

### "Not found: avr-gcc"

The AVR toolchain is not on your PATH, and you're not running the Windows build with the vendored toolchain under `tools/toolchain/`. See section 2, Option B.

### "Source file not found"

A path in `sourceFiles` does not exist. Check that the relative path is correct from the project folder.

### "No C/C++ source files found"

The project folder has no `.c` or `.cpp` files outside of `build/`. Create a `src/main.cpp`.

### "avrdude: can't open device"

- Wrong COM port — check Device Manager (Windows) or `ls /dev/ttyUSB*` (Linux).
- Serial Monitor is still open — close it before flashing (section 7).
- Wrong programmer type — `ARDUINO` for bootloader, `USBASP`/`AVRISP`/`STK500`/`DRAGON`/`JTAG` for a standalone ISP programmer.

### Build succeeds but chip does not run

- Fuse bits: ensure the chip is configured for the same clock source as `F_CPU`. A chip set for an external crystal that is absent will not run.
- The HEX was not written to the correct address — check avrdude output for errors.

### LCD shows garbage or nothing

- Contrast too high or too low — adjust the V0 potentiometer.
- RW is not tied to GND — the LCD will fight the bus.
- Initialisation called before power is stable — `lcd.begin()` waits 50 ms, but make sure your power supply settled before `main()` runs.
- Wrong pin mapping — verify your wiring against the `CharLCD` constructor arguments.

---

*Numbered example projects live under `Examples/core/` (`gpio/`, `usart/`, `adc/`, `pwm/`, `timer/`, `spi/`, `i2c/`, `interrupts/`, `eeprom/`) and `Examples/Modules/` (one numbered series per module driver). Browse them from inside the IDE via **File → Examples**.*
