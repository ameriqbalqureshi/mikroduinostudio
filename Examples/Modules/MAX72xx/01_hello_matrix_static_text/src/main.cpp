/*
 * MAX72xx Basics — Static Text on a Chained LED Matrix — MikroDuino
 * Module SDK
 *
 * The simplest possible use of the MAX72xx module: four chained 8x8
 * MAX7219 dot-matrix modules showing one fixed word. This is project 1 of
 * 6 in the examples/Modules/MAX72xx series, which walks MatrixDisplay
 * from a single static printText() up to a capstone scrolling sensor
 * dashboard combining non-blocking scroll, a potentiometer, and a button.
 *
 * Unlike every other module series in this SDK, MAX72xx is NOT a plain
 * MikroDuino-native module — it wraps the third-party Arduino MD_MAX72XX
 * library, which expects Arduino-style pin numbers, SPI.h, and (for the
 * non-blocking scroll projects later in this series) Arduino's millis().
 * Every project in this series therefore uses the SDK's Arduino
 * compatibility layer (sdk/compat — see CLAUDE.md) instead of the raw
 * GPIO/registers style every other module series in this SDK uses:
 * #include <Arduino.h>, ARDUINO_MAIN() instead of a hand-written main(),
 * and setup()/loop() instead of a while(true) loop.
 *
 * Build requirement: the MD_MAX72XX Arduino library must be installed at
 * %USERPROFILE%\.mikroduino\libraries\MD_MAX72XX (see MAX72xx.hpp's own
 * header comment). The MikroDuino IDE auto-detects <MD_MAX72xx.h> /
 * <SPI.h> / <Arduino.h> includes and resolves their library/compat source
 * files automatically — nothing beyond this project's own two include
 * dirs needs to be listed in the .mdp file.
 *
 * Hardware (ATmega328P @ 16 MHz, 4x chained FC-16 style 8x8 MAX7219
 * modules, hardware SPI):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ DIN     │ D11   │ MOSI                                        │
 *   │ CLK     │ D13   │ SCK                                          │
 *   │ CS      │ D10   │ Any free digital pin                        │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * MAX72xx concepts introduced:
 *   - MatrixDisplay(mod, csPin, numDevices) — the hardware-SPI
 *     constructor. mod selects the module wiring variant (FC16 for the
 *     common blue eBay boards this series assumes); numDevices=4 chains
 *     four 8x8 modules into one 32-column-wide display.
 *   - begin() — must be called once in setup() before any other method.
 *   - setBrightness(level) — 0 (dimmest) to 15 (full). Set once here;
 *     project 5 changes it at runtime from a button.
 *   - printText(text, startCol) — renders static text starting at the
 *     given absolute column and returns the first free column after it.
 *     This project ignores the return value; project 6's capstone uses
 *     it to place a second piece of text right after the first.
 */

#include <Arduino.h>    // pinMode()/delay(), and signals the builder to compile Arduino compat
#include <SPI.h>        // signals the builder to compile SPI compat
#include <MD_MAX72xx.h> // signals the builder to auto-find and compile MD_MAX72XX
#include <MAX72xx.hpp>  // MikroDuino MatrixDisplay wrapper

using namespace MikroDuino;

ARDUINO_MAIN()

static constexpr uint8_t  CS_PIN      = 10;
static constexpr uint8_t  NUM_MODULES =  4;   // 4 x 8x8 = 32 columns

MatrixDisplay mx(MatrixDisplay::FC16, CS_PIN, NUM_MODULES);

void setup() {
    mx.begin();
    mx.setBrightness(5);   // comfortable indoor brightness (0 dim ... 15 full)
    mx.printText("HI");
}

void loop() {
    // Static display: nothing to update once printText() has drawn it.
}
