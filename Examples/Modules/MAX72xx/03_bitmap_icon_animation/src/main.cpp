/*
 * MAX72xx — PROGMEM Bitmap Icons and a 2-Frame Animation — MikroDuino
 * Module SDK
 *
 * Project 3 of 6 in the examples/Modules/MAX72xx series. Projects 1-2
 * used the library's own font (printText) and its own shape math
 * (drawRect/drawFilledRect/...). This project draws a hand-authored 8x8
 * icon from a raw byte array instead — the technique any custom logo,
 * sprite, or icon set needs — then flips between two such icons on a
 * timer to build a simple up/down "select" indicator animation.
 *
 * Hardware — identical wiring to project 1 (only the first module is lit
 * by this project; modules 2-4 stay dark):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ DIN     │ D11   │ MOSI                                        │
 *   │ CLK     │ D13   │ SCK                                          │
 *   │ CS      │ D10   │ Any free digital pin                        │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * MAX72xx concepts introduced:
 *   - Column-major bitmap format: an 8x8 icon is 8 bytes, one byte per
 *     COLUMN (not per row, unlike most font/glyph formats). Within a
 *     byte, bit 0 is the TOP row and bit 7 is the BOTTOM row. The two
 *     icons below were hand-derived from an 8x8 grid this way — see the
 *     bit-by-bit table in the comment above each array.
 *   - PROGMEM + drawBitmapP(col, data_P, width) — the icon bytes live in
 *     flash, not RAM, since a whole icon set would otherwise compete with
 *     the ~2 KB of SRAM an ATmega328P has for everything else the sketch
 *     needs. drawBitmapP() reads through pgm_read_byte() internally so
 *     the caller never touches flash-vs-RAM addressing directly.
 *     drawBitmap() (no P suffix, not used by this project) is the RAM
 *     equivalent, for bitmaps generated or modified at runtime.
 *   - A plain delay()-driven toggle between two static frames is the
 *     simplest possible "animation": no scrolling, no shifting, just
 *     redrawing a different bitmap over the same columns on a timer.
 *     Project 5 replaces this delay() polling with MatrixDisplay's own
 *     non-blocking scroll engine for actual horizontal motion.
 */

#include <Arduino.h>
#include <SPI.h>
#include <MD_MAX72xx.h>
#include <MAX72xx.hpp>

using namespace MikroDuino;

ARDUINO_MAIN()

static constexpr uint8_t  CS_PIN      = 10;
static constexpr uint8_t  NUM_MODULES =  4;
static constexpr uint16_t FRAME_MS    = 400;   // time each animation frame stays on screen

MatrixDisplay mx(MatrixDisplay::FC16, CS_PIN, NUM_MODULES);

// 8x8 up-arrow, drawn on paper as (row 0 = top, col 0 = left, X = lit):
//   col:     0 1 2 3 4 5 6 7
//   row0     . . . X . . . .
//   row1     . . X X X . . .
//   row2     . X X X X X . .
//   row3     . . X X X . . .
//   row4     . . X X X . . .
//   row5     . . X X X . . .
//   row6     . . X X X . . .
//   row7     . . . . . . . .
// Transposed to one byte per column, bit 0 = row 0 ... bit 7 = row 7:
static const uint8_t UP_ARROW[8] PROGMEM = {
    0x00, 0x04, 0x7E, 0x7F, 0x7E, 0x04, 0x00, 0x00
};

// Same shape mirrored top-to-bottom (row 0 <-> row 7, bit r <-> bit 7-r).
static const uint8_t DOWN_ARROW[8] PROGMEM = {
    0x00, 0x20, 0x7E, 0xFE, 0x7E, 0x20, 0x00, 0x00
};

void setup() {
    mx.begin();
    mx.setBrightness(5);

    // Static icon first, held for a moment before the animation starts.
    mx.clear();
    mx.drawBitmapP(0, UP_ARROW, 8);
    delay(1000);
}

void loop() {
    mx.clear();
    mx.drawBitmapP(0, UP_ARROW, 8);
    delay(FRAME_MS);

    mx.clear();
    mx.drawBitmapP(0, DOWN_ARROW, 8);
    delay(FRAME_MS);
}
