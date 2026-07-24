/*
 * MAX72xx — Pixel Primitives and Shapes — MikroDuino Module SDK
 *
 * Project 2 of 6 in the examples/Modules/MAX72xx series. Project 1 only
 * ever called printText() — MatrixDisplay's font renderer. This project
 * drops one level lower, to the pixel and shape primitives every higher-
 * level feature (text, bitmaps, scrolling) is ultimately built from, and
 * cycles through each of them for a couple of seconds at a time.
 *
 * Hardware — identical wiring to project 1:
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ DIN     │ D11   │ MOSI                                        │
 *   │ CLK     │ D13   │ SCK                                          │
 *   │ CS      │ D10   │ Any free digital pin                        │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * MAX72xx concepts introduced:
 *   - Coordinate system: row 0 is the TOP row, col 0 is the LEFTMOST
 *     column of the FIRST chained module — every shape helper below
 *     shares this one coordinate space across all 4 modules (32 columns),
 *     not four separate 8x8 spaces.
 *   - setPixel(row, col, state) / getPixel(row, col) — the single-LED
 *     primitive. This project uses setPixel() to plot one lit pixel per
 *     module boundary (col 0, 8, 16, 24) so all four chained modules are
 *     visibly identifiable on screen.
 *   - fill(state) — every LED on (true) or off (false) at once.
 *   - drawHLine(row, startCol, endCol) / drawVLine(col, startRow, endRow)
 *     — straight lines; drawRect() and drawFilledRect() (project 6's
 *     bargraph uses this one) are built from exactly these two.
 *   - drawRect(row, col, height, width) — a HOLLOW rectangle outline.
 *   - drawFilledRect(row, col, height, width) — a SOLID rectangle.
 *   - clear() (used between each shape) resets every LED off in one call
 *     rather than fill(false) — both work, but clear() is what every
 *     later project in this series uses before redrawing a frame.
 */

#include <Arduino.h>
#include <SPI.h>
#include <MD_MAX72xx.h>
#include <MAX72xx.hpp>

using namespace MikroDuino;

ARDUINO_MAIN()

static constexpr uint8_t  CS_PIN      = 10;
static constexpr uint8_t  NUM_MODULES =  4;
static constexpr uint16_t HOLD_MS     = 1500;   // how long each shape stays on screen

MatrixDisplay mx(MatrixDisplay::FC16, CS_PIN, NUM_MODULES);

void setup() {
    mx.begin();
    mx.setBrightness(5);
}

void loop() {
    // 1) Single pixels — one dot at each module boundary.
    mx.clear();
    mx.setPixel(0, 0);
    mx.setPixel(0, 8);
    mx.setPixel(0, 16);
    mx.setPixel(0, 24);
    delay(HOLD_MS);

    // 2) Fill everything on, then off.
    mx.fill(true);
    delay(HOLD_MS);
    mx.fill(false);
    delay(300);

    // 3) A horizontal line across the middle row of all 4 modules.
    mx.clear();
    mx.drawHLine(3, 0, mx.columnCount() - 1);
    delay(HOLD_MS);

    // 4) A vertical line down the middle column of the first module.
    mx.clear();
    mx.drawVLine(4, 0, 7);
    delay(HOLD_MS);

    // 5) A hollow rectangle spanning the first two modules.
    mx.clear();
    mx.drawRect(0, 0, 8, 16);
    delay(HOLD_MS);

    // 6) A solid filled rectangle in the last module.
    mx.clear();
    mx.drawFilledRect(2, 24, 4, 6);
    delay(HOLD_MS);

    mx.clear();
    delay(300);
}
