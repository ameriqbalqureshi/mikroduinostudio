#pragma once
/*
 * MikroDuino SSD1306 Module
 * 128×64 OLED display driver over I2C.
 *
 * Features:
 *   - 1024-byte framebuffer (128×64 ÷ 8 bits); all drawing goes to RAM first,
 *     display() pushes the buffer to the OLED in a single I2C burst.
 *   - Pixel, line (H/V/diagonal), rectangle (outline+fill), circle (outline+fill).
 *   - Bitmap rendering from PROGMEM (native SSD1306 page-column format).
 *   - 5×7 ASCII text, cursor positioning, integer print.
 *   - Hardware scrolling (right, left, diagonal).
 *   - Contrast, invert, on/off, flip (mirror X and/or Y).
 *
 * RAM cost: 1024 B (framebuffer) + ~10 B (object state).
 * On ATmega328P (2 KB RAM) this leaves ~1 KB for the rest of the program.
 * Prefer ATmega64/128 for applications with heavy stack/data usage.
 *
 * Wiring (I2C):
 *   SDA → PC4 (ATmega328P), SCL → PC5.
 *   Typical I2C address: 0x3C (SA0 low) or 0x3D (SA0 high).
 *
 * Usage:
 *   I2C.beginMaster(400000UL);          // 400 kHz for fast framebuffer push
 *   SSD1306 oled(I2C);
 *   oled.begin();
 *   oled.clear();
 *   oled.setCursor(0, 0);
 *   oled.print("Hello!");
 *   oled.display();
 *
 * Requires: SSD1306.cpp compiled with the project.
 */

#include <mikroduino/i2c.hpp>
#include <stdint.h>
#include <string.h>
#include <avr/pgmspace.h>

namespace MikroDuino {

class SSD1306 {
public:

    static constexpr uint8_t WIDTH  = 128;
    static constexpr uint8_t HEIGHT = 64;
    static constexpr uint8_t PAGES  = HEIGHT / 8;  // 8

    // addr: I2C address (0x3C or 0x3D)
    explicit SSD1306(I2CDriver& i2c, uint8_t addr = 0x3C)
        : _i2c(i2c), _addr(addr), _curX(0), _curY(0) {}

    // ================================================================
    // Initialisation
    // ================================================================

    // Initialise the display. Call once after I2C.beginMaster().
    void begin();

    // ================================================================
    // Framebuffer control
    // ================================================================

    // Push the framebuffer to the display over I2C. Call after any drawing.
    void display();

    // Clear the framebuffer (all pixels off). Call display() to apply.
    void clear() { memset(_buf, 0x00, sizeof(_buf)); }

    // Fill the entire framebuffer with a pattern (default 0xFF = all pixels on).
    void fill(uint8_t pattern = 0xFF) { memset(_buf, pattern, sizeof(_buf)); }

    // ================================================================
    // Display control
    // ================================================================

    // Turn the display on or off (does not affect framebuffer).
    void on(bool enable);

    // Invert pixel polarity (swap lit/unlit).
    void invert(bool inv);

    // Set contrast 0–255 (default ~0xCF).
    void contrast(uint8_t level);

    // Mirror the display horizontally (flipH) and/or vertically (flipV).
    // Call before begin() or call begin() again after changing.
    void flip(bool flipH, bool flipV);

    // ================================================================
    // Pixel operations
    // ================================================================

    // Set or clear one pixel in the framebuffer.
    void drawPixel(uint8_t x, uint8_t y, bool on = true);

    // Read one pixel from the framebuffer.
    bool getPixel(uint8_t x, uint8_t y) const;

    // ================================================================
    // Primitives (all operate on the framebuffer)
    // ================================================================

    void drawHLine(uint8_t x, uint8_t y, uint8_t w, bool on = true);
    void drawVLine(uint8_t x, uint8_t y, uint8_t h, bool on = true);
    void drawLine(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, bool on = true);

    void drawRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, bool on = true);
    void fillRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, bool on = true);

    void drawCircle(uint8_t cx, uint8_t cy, uint8_t r, bool on = true);
    void fillCircle(uint8_t cx, uint8_t cy, uint8_t r, bool on = true);

    // ================================================================
    // Bitmap (PROGMEM, native SSD1306 column-page format)
    // Data layout: (h/8) pages × w columns, byte = 8 vertical pixels,
    // bit 0 = top pixel of each page row.
    // h must be a multiple of 8.
    // ================================================================
    void drawBitmap(uint8_t x, uint8_t y, uint8_t w, uint8_t h,
                    const uint8_t* pgmData, bool on = true);

    // ================================================================
    // Text (5×7 font, each character cell is 6×8: 5 cols + 1 spacing)
    // Cursor X is in pixels. Cursor Y is the page (0–7), i.e. row × 8 pixels.
    // ================================================================

    // Set text cursor position. y is the pixel row (should be multiple of 8).
    void setCursor(uint8_t x, uint8_t y);

    // Print one character. Newline ('\n') advances to the next row.
    void print(char c);

    // Print a NUL-terminated string.
    void print(const char* str);

    // Print a PROGMEM string.
    void print_P(const char* pgmStr);

    // Print a signed decimal integer.
    void print(int32_t value);

    // Print an unsigned integer in the given base (10 or 16).
    void printU(uint32_t value, uint8_t base = 10);

    // ================================================================
    // Hardware scrolling (moves GDDRAM content, not the framebuffer)
    // ================================================================

    // Scroll right continuously. startPage/endPage: 0–7.
    void scrollRight(uint8_t startPage = 0, uint8_t endPage = 7);

    // Scroll left continuously.
    void scrollLeft(uint8_t startPage = 0, uint8_t endPage = 7);

    // Scroll diagonally up-right.
    void scrollDiagRight(uint8_t startPage = 0, uint8_t endPage = 7);

    // Scroll diagonally up-left.
    void scrollDiagLeft(uint8_t startPage = 0, uint8_t endPage = 7);

    // Stop all scrolling.
    void stopScroll();

private:

    I2CDriver& _i2c;
    uint8_t    _addr;
    uint8_t    _buf[WIDTH * PAGES];  // 1024-byte framebuffer
    uint8_t    _curX;
    uint8_t    _curY;  // pixel row (page × 8)

    // Send one or more SSD1306 commands.
    void _cmd(uint8_t c);
    void _cmd2(uint8_t c, uint8_t a);
    void _cmd3(uint8_t c, uint8_t a0, uint8_t a1);

    // Raw byte from PROGMEM font table for character c, column col.
    static uint8_t _fontByte(char c, uint8_t col);
};

} // namespace MikroDuino
