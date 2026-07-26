# MikroDuino IDE — Architecture Document

Version 1.0 | Phase 1 Design

---

## 1. System Overview

MikroDuino is a complete embedded development ecosystem composed of a desktop IDE, build system, package manager, hardware SDK, and programmer integration. It targets AVR microcontrollers first, with an architecture designed for MCU family extensibility.

```
┌─────────────────────────────────────────────────────────────────────┐
│                        MikroDuino Ecosystem                         │
│                                                                     │
│  ┌──────────────────────────────────────────────────────────────┐   │
│  │                     MikroDuino IDE                           │   │
│  │  (Electron + TypeScript + React + Monaco Editor)             │   │
│  │                                                              │   │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────────┐   │   │
│  │  │ Project  │ │  Editor  │ │  Build   │ │   Serial     │   │   │
│  │  │ Explorer │ │  Tabs    │ │  Output  │ │   Monitor    │   │   │
│  │  └──────────┘ └──────────┘ └──────────┘ └──────────────┘   │   │
│  └──────────────────────────────────────────────────────────────┘   │
│          │              │              │              │               │
│          ▼              ▼              ▼              ▼               │
│  ┌────────────────────────────────────────────────────────────┐     │
│  │                    Node.js Backend Services                │     │
│  │                                                            │     │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌────────────┐   │     │
│  │  │ Project  │ │  Build   │ │ Package  │ │ Programmer │   │     │
│  │  │ Manager  │ │  System  │ │ Manager  │ │ Integration│   │     │
│  │  └──────────┘ └──────────┘ └──────────┘ └────────────┘   │     │
│  └────────────────────────────────────────────────────────────┘     │
│          │              │                                             │
│          ▼              ▼                                             │
│  ┌────────────────────────────────────────────────────────────┐     │
│  │              MikroDuino SDK (C++17 Hardware Libraries)     │     │
│  │                                                            │     │
│  │  GPIO │ USART │ SPI │ I2C │ ADC │ Timer │ PWM │ INT      │     │
│  └────────────────────────────────────────────────────────────┘     │
│          │                                                            │
│          ▼                                                            │
│  ┌────────────────────────────────────────────────────────────┐     │
│  │         AVR Toolchain (avr-gcc, avr-libc, avrdude)         │     │
│  └────────────────────────────────────────────────────────────┘     │
│          │                                                            │
│          ▼                                                            │
│  ┌────────────────────────────────────────────────────────────┐     │
│  │              AVR Microcontroller Hardware                   │     │
│  └────────────────────────────────────────────────────────────┘     │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 2. Layer Architecture

```
┌──────────────────────────────────────────────────┐
│              Application Layer                    │  User code
│  main.cpp, display.cpp, sensor.cpp               │
├──────────────────────────────────────────────────┤
│              Module Library Layer                 │  Reusable drivers
│  LCD, SSD1306, DS3231, DHT22, HCSR04            │
├──────────────────────────────────────────────────┤
│              Core Library Layer                   │  Hardware abstraction
│  GPIO, USART, SPI, I2C, ADC, Timer, PWM, INT    │
├──────────────────────────────────────────────────┤
│              Register Access Layer                │  Direct register macros
│  REG(), BITSET(), BITCLEAR(), BITTOGGLE()        │
├──────────────────────────────────────────────────┤
│              MCU Platform Layer                   │  Platform definitions
│  avr/io.h, avr/interrupt.h, avr/pgmspace.h      │
├──────────────────────────────────────────────────┤
│              AVR Hardware                         │  Physical silicon
│  ATmega328P / 32 / 16 / 64 / 128                │
└──────────────────────────────────────────────────┘

Optional layer (separate, non-invasive):
┌──────────────────────────────────────────────────┐
│         Arduino Compatibility Layer               │
│  pinMode(), digitalWrite(), analogRead(), Serial │
│  (maps to MikroDuino Core internally)            │
└──────────────────────────────────────────────────┘
```

---

## 3. Monorepo Structure

```
MikroDuino_IDE/
├── ARCHITECTURE.md
├── package.json                    # npm workspaces root
├── tsconfig.base.json
├── .eslintrc.json
├── .gitignore
│
├── apps/
│   ├── ide/                        # Electron desktop IDE
│   │   ├── package.json
│   │   ├── tsconfig.json
│   │   ├── webpack.main.config.js
│   │   ├── webpack.renderer.config.js
│   │   └── src/
│   │       ├── main/               # Electron main process
│   │       │   ├── index.ts
│   │       │   ├── WindowManager.ts
│   │       │   ├── IPCHandler.ts
│   │       │   └── MenuBuilder.ts
│   │       ├── preload/
│   │       │   └── index.ts        # Context bridge API
│   │       └── renderer/           # React frontend
│   │           ├── index.tsx
│   │           ├── App.tsx
│   │           ├── components/
│   │           │   ├── Editor/     # Monaco editor wrapper
│   │           │   ├── Explorer/   # Project file tree
│   │           │   ├── Output/     # Build output panel
│   │           │   ├── Toolbar/    # Top toolbar
│   │           │   ├── StatusBar/  # Bottom status bar
│   │           │   ├── SerialMonitor/
│   │           │   └── DeviceInfo/
│   │           ├── panels/
│   │           ├── store/          # Redux state
│   │           └── hooks/
│   └── cli/                        # CLI tool (future)
│
├── packages/
│   ├── shared/                     # Shared types across all packages
│   ├── build-system/               # Build orchestration
│   ├── project-manager/            # .mdp project file management
│   ├── package-manager/            # Library package management
│   ├── serial-monitor/             # Serial port communication
│   └── programmer/                 # avrdude integration
│
├── sdk/
│   ├── core/
│   │   └── avr/
│   │       ├── include/mikroduino/ # C++17 hardware library headers
│   │       └── src/               # Implementation files
│   ├── compat/                     # Arduino compatibility layer
│   └── modules/                    # Reusable peripheral drivers
│       ├── LCD/
│       ├── SSD1306/
│       ├── DS3231/
│       ├── DHT22/
│       └── HCSR04/
│
├── docs/
│   ├── api/
│   └── guides/
│
├── tests/
└── tools/
    ├── toolchain/                  # Toolchain detection/management
    └── scripts/
```

---

## 4. SDK Core Class Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                    MikroDuino Namespace                      │
│                                                             │
│  ┌──────────────┐    ┌─────────────────────────────────┐   │
│  │  GPIO        │    │  Pin                            │   │
│  │  ──────────  │    │  ─────────────────────────────  │   │
│  │  +output()   │    │  encoding: port<<3 | bit        │   │
│  │  +input()    │    │  PA0..PA7, PB0..PB7, etc.       │   │
│  │  +set()      │    └─────────────────────────────────┘   │
│  │  +clear()    │                                           │
│  │  +toggle()   │    ┌─────────────────────────────────┐   │
│  │  +read()     │    │  Registers (macros)             │   │
│  └──────────────┘    │  ────────────────────────────── │   │
│                       │  REG(r)                         │   │
│  ┌──────────────┐    │  BITSET(r,b)                    │   │
│  │  USART<N>    │    │  BITCLEAR(r,b)                  │   │
│  │  ──────────  │    │  BITTOGGLE(r,b)                 │   │
│  │  +begin()    │    │  BITREAD(r,b)                   │   │
│  │  +write()    │    └─────────────────────────────────┘   │
│  │  +read()     │                                           │
│  │  +available()│    ┌─────────────────────────────────┐   │
│  │  +enableIRQ()│    │  Timer<N>                       │   │
│  └──────────────┘    │  ─────────────────────────────  │   │
│                       │  +mode(TimerMode)               │   │
│  ┌──────────────┐    │  +prescaler(uint16_t)           │   │
│  │  SPI         │    │  +compareA(uint16_t)            │   │
│  │  ──────────  │    │  +compareB(uint16_t)            │   │
│  │  +beginMaster│    │  +enableInterrupt()             │   │
│  │  +beginSlave │    │  +start() / stop()              │   │
│  │  +transfer() │    └─────────────────────────────────┘   │
│  └──────────────┘                                           │
│                       ┌─────────────────────────────────┐   │
│  ┌──────────────┐    │  ADC                            │   │
│  │  I2C         │    │  ─────────────────────────────  │   │
│  │  ──────────  │    │  +begin(ref, prescaler)         │   │
│  │  +beginMaster│    │  +read(channel)                 │   │
│  │  +beginSlave │    │  +readFreeRunning()             │   │
│  │  +write()    │    │  +enableInterrupt()             │   │
│  │  +read()     │    └─────────────────────────────────┘   │
│  │  +scan()     │                                           │
│  └──────────────┘    ┌─────────────────────────────────┐   │
│                       │  Interrupt                      │   │
│  ┌──────────────┐    │  ─────────────────────────────  │   │
│  │  PWM<N>      │    │  +attach(source, handler)       │   │
│  │  ──────────  │    │  +detach(source)                │   │
│  │  +begin()    │    │  +enable() / disable()          │   │
│  │  +duty()     │    └─────────────────────────────────┘   │
│  │  +frequency()│                                           │
│  └──────────────┘                                           │
└─────────────────────────────────────────────────────────────┘
```

---

## 5. Build Flow

```
User clicks "Build"
        │
        ▼
┌───────────────────┐
│  BuildSystem.ts   │
│  validateProject()│
└─────────┬─────────┘
          │
          ▼
┌───────────────────┐
│MakefileGenerator  │
│generateMakefile() │
│  - MCU flags      │
│  - F_CPU define   │
│  - Source files   │
│  - Include paths  │
│  - Lib paths      │
└─────────┬─────────┘
          │
          ▼
┌───────────────────┐     ┌─────────────────────────┐
│  ToolchainManager │────▶│  avr-gcc / avr-g++      │
│  invokeMake()     │     │  compile each .c/.cpp    │
└─────────┬─────────┘     └─────────────────────────┘
          │
          ▼
┌───────────────────┐     ┌─────────────────────────┐
│  avr-ld (linker)  │────▶│  avr-objcopy             │
│  produces .elf    │     │  produces .hex           │
└─────────┬─────────┘     └─────────────────────────┘
          │
          ▼
┌───────────────────┐
│  avr-size         │
│  Flash / RAM /    │
│  EEPROM usage     │
└─────────┬─────────┘
          │
          ▼
┌───────────────────┐
│  BuildParser.ts   │
│  parse errors,    │
│  warnings,        │
│  file:line refs   │
└─────────┬─────────┘
          │
          ▼
┌───────────────────┐
│  IDE Output Panel │
│  clickable errors │
│  memory usage bar │
└───────────────────┘
```

---

## 6. IPC Architecture (Electron)

```
┌────────────────────────────────────────────────────────┐
│                   Renderer Process                      │
│   React Components ──▶ Redux Store ──▶ IPC Calls      │
│                         (window.mikroduino.*)          │
└────────────────────────────┬───────────────────────────┘
                             │  contextBridge (preload)
                             │  Secure channel only
┌────────────────────────────▼───────────────────────────┐
│                    Main Process                         │
│   IPCHandler ──▶ ProjectManager / BuildSystem          │
│               ──▶ SerialConnection / Programmer        │
│               ──▶ PackageManager                       │
└────────────────────────────────────────────────────────┘
```

---

## 7. Project File Schema (project.mdp)

```json
{
  "projectName": "WeatherStation",
  "version": "1.0.0",
  "target": {
    "mcu": "ATmega328P",
    "clock": 16000000,
    "bootloader": "optiboot"
  },
  "build": {
    "optimization": "O2",
    "warnings": "all",
    "defines": ["F_CPU=16000000UL"],
    "extraFlags": []
  },
  "programmer": {
    "type": "USBASP",
    "port": "",
    "baudRate": 115200
  },
  "libraries": ["LCD", "DS3231"],
  "sources": {
    "srcDir": "src",
    "includeDir": "include",
    "libDir": "lib"
  }
}
```

---

## 8. Supported MCU Definitions

| MCU         | Flash  | RAM   | EEPROM | USART | SPI | I2C | ADC  | Timers |
|-------------|--------|-------|--------|-------|-----|-----|------|--------|
| ATmega328P  | 32 KB  | 2 KB  | 1 KB   | 1     | 1   | 1   | 6ch  | 3      |
| ATmega32    | 32 KB  | 2 KB  | 1 KB   | 1     | 1   | 1   | 8ch  | 3      |
| ATmega16    | 16 KB  | 1 KB  | 512 B  | 1     | 1   | 1   | 8ch  | 3      |
| ATmega64    | 64 KB  | 4 KB  | 2 KB   | 2     | 1   | 1   | 8ch  | 4      |
| ATmega128   | 128 KB | 4 KB  | 4 KB   | 2     | 1   | 1   | 8ch  | 4      |
| ATmega2560  | 256 KB | 8 KB  | 4 KB   | 4     | 1   | 1   | 16ch | 6      |

---

## 9. Package Format

Each MikroDuino package is a directory with:

```
PackageName/
├── package.json        # Metadata
├── include/            # Public headers
│   └── PackageName.hpp
├── src/                # Implementation
│   └── PackageName.cpp
├── examples/           # Usage examples
│   └── basic/
│       └── main.cpp
└── docs/
    └── README.md
```

`package.json` format:
```json
{
  "name": "SSD1306",
  "version": "1.0.0",
  "author": "Amer Iqbal",
  "description": "SSD1306 OLED display driver",
  "license": "MIT",
  "dependencies": [],
  "targets": ["avr"],
  "keywords": ["oled", "display", "i2c"]
}
```

---

## 10. Phase 1 Implementation Plan

### Milestone 1.1 — SDK Core
- [x] Register access macros (registers.hpp)
- [x] Platform definitions (platform.hpp)
- [x] GPIO library (gpio.hpp / gpio.cpp)
- [x] USART library (usart.hpp / usart.cpp)
- [x] Interrupt library (interrupt.hpp / interrupt.cpp)
- [x] Arduino compat layer (Arduino.h / Arduino.cpp)

### Milestone 1.2 — Build System
- [x] MCU definitions database
- [x] Project file parser and validator
- [x] Makefile generator
- [x] Toolchain manager (avr-gcc/avrdude detection)
- [x] Build output parser (error extraction)
- [x] Build system orchestrator

### Milestone 1.3 — Project Manager
- [x] Project scaffolding (create new project)
- [x] Project file CRUD
- [x] File system watcher
- [x] Project validation

### Milestone 1.4 — IDE Skeleton
- [x] Electron main process + window management
- [x] Secure IPC bridge (preload)
- [x] React app scaffold
- [x] Monaco editor integration
- [x] Project explorer file tree
- [x] Build output panel
- [x] Toolbar (Build / Upload / New / Open)
- [x] Status bar (MCU / Clock / Programmer)
- [x] Redux state management

### Phase 2 (next)
- SPI, I2C, ADC, Timer, PWM libraries
- Package manager
- Serial monitor with hex/binary view

### Phase 3
- Library repository
- Programmer integration (avrdude UI)
- Fuse bit manager

### Phase 4
- Arduino compatibility layer
- In-IDE documentation viewer

### Phase 5
- RTOS scaffold
- Debugger interfaces
- Simulator framework
