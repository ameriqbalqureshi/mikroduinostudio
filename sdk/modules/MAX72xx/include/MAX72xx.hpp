#pragma once
/*
 * MikroDuino MAX72xx Wrapper
 * High-level driver for MAX7219/MAX7221 LED dot-matrix displays.
 * Wraps the MD_MAX72XX library with a simpler MikroDuino-style API.
 *
 * Build requirement: add the MD_MAX72XX/src directory to avr-g++ include paths.
 * In Settings → Build Flags: -I%USERPROFILE%\.mikroduino\libraries\MD_MAX72XX\src
 *
 * Typical wiring (hardware SPI on ATmega328P):
 *   DIN → D11 (MOSI), CLK → D13 (SCK), CS → any free pin (e.g. D10)
 *
 * Usage:
 *   #include <MAX72xx.hpp>
 *   using namespace MikroDuino;
 *
 *   MatrixDisplay mx(MatrixDisplay::FC16, 10, 4); // FC16 modules, CS=D10, 4 devices
 *
 *   mx.begin();
 *   mx.setBrightness(4);
 *   mx.scrollText("Hello!");
 */

#include <MD_MAX72xx.h>
#include <stdint.h>

namespace MikroDuino {

class MatrixDisplay {
public:
    // Module type aliases — pick the one matching your hardware
    using ModuleType = MD_MAX72XX::moduleType_t;
    static constexpr ModuleType GENERIC   = MD_MAX72XX::GENERIC_HW;   // common eBay modules
    static constexpr ModuleType FC16      = MD_MAX72XX::FC16_HW;      // FC-16 blue modules
    static constexpr ModuleType PAROLA    = MD_MAX72XX::PAROLA_HW;    // Parola boards
    static constexpr ModuleType ICSTATION = MD_MAX72XX::ICSTATION_HW; // ICStation boards

    // Hardware SPI constructor — uses AVR SPI peripheral (fastest)
    // ATmega328P: DIN=D11 (MOSI), CLK=D13 (SCK), CS=csPin
    MatrixDisplay(ModuleType mod, uint8_t csPin, uint8_t numDevices = 1);

    // Software (bit-bang) SPI constructor — any three digital pins
    MatrixDisplay(ModuleType mod, uint8_t dataPin, uint8_t clkPin,
                  uint8_t csPin, uint8_t numDevices = 1);

    // Must be called once in setup() before any other method
    bool begin();

    // ── Display control ──────────────────────────────────────────────────────
    void clear();
    void setBrightness(uint8_t level);  // 0 = dimmest … 15 = full brightness
    void setEnabled(bool on);           // false = low-power shutdown mode

    // ── Pixel primitives ────────────────────────────────────────────────────
    // Coordinate system: row 0 = top, col 0 = leftmost column of first module
    void setPixel(uint8_t row, uint16_t col, bool state = true);
    bool getPixel(uint8_t row, uint16_t col);

    // Fill entire display on (true) or off (false)
    void fill(bool state = true);

    // Horizontal line across a row
    void drawHLine(uint8_t row, uint16_t startCol, uint16_t endCol, bool state = true);
    // Vertical line down a column
    void drawVLine(uint16_t col, uint8_t startRow, uint8_t endRow, bool state = true);
    // Hollow rectangle: top-left corner at (row, col), given height and width in pixels
    void drawRect(uint8_t row, uint16_t col, uint8_t height, uint8_t width, bool state = true);
    // Solid filled rectangle
    void drawFilledRect(uint8_t row, uint16_t col, uint8_t height, uint8_t width, bool state = true);

    // ── Bitmap drawing ───────────────────────────────────────────────────────
    // Column-major format: each byte represents 8 LEDs in one column (bit 0 = row 0).
    // Draw from a RAM buffer
    void drawBitmap(uint16_t col, const uint8_t* data, uint8_t width);
    // Draw from PROGMEM (declare your array with PROGMEM attribute)
    void drawBitmapP(uint16_t col, const uint8_t* data_P, uint8_t width);

    // ── Text ────────────────────────────────────────────────────────────────
    // Render static text at absolute column startCol.
    // Returns the first free column after the last character (useful for multi-print).
    uint16_t printText(const char* text, uint16_t startCol = 0);

    // Blocking marquee: scrolls text left across all modules then returns.
    // delayMs controls speed (milliseconds between each one-pixel shift).
    // Does NOT require timekeeping — uses _delay_ms() internally.
    void scrollText(const char* text, uint16_t delayMs = 50);

    // Non-blocking marquee: call beginScroll() once, then updateScroll() every loop.
    // updateScroll() returns true while still scrolling, false when the animation ends.
    // REQUIRES millis() — call Arduino::beginTimekeeping() in setup() before use.
    void beginScroll(const char* text, uint16_t delayMs = 50);
    bool updateScroll();

    // ── Info & escape hatch ─────────────────────────────────────────────────
    uint16_t    columnCount() { return _mx.getColumnCount(); }
    uint8_t     deviceCount() { return _mx.getDeviceCount(); }
    MD_MAX72XX& raw()         { return _mx; }  // direct access for advanced use

private:
    MD_MAX72XX _mx;

    // Non-blocking scroll state
    struct ScrollState {
        const char* p;           // pointer into the text string at current character
        uint8_t     charBuf[8];  // pixel columns for the current character
        uint8_t     charWidth;   // width of current character in columns
        uint8_t     colIdx;      // next column index to push from charBuf
        bool        inGap;       // true while emitting the 1-column inter-character gap
        uint32_t    lastMs;      // millis() timestamp of last pixel shift
        uint16_t    delayMs;     // requested delay between shifts
        bool        active;      // false when scroll is finished
        uint16_t    tailCols;    // remaining blank columns to push after text ends
    } _scroll;

    void _scrollAdvance();  // shift one pixel and feed next column data
};

} // namespace MikroDuino
