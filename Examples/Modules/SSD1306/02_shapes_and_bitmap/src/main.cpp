/*
 * SSD1306 Drawing Primitives + PROGMEM Bitmap — MikroDuino Module SDK
 *
 * Project 2 of 6 in the examples/Modules/SSD1306 series. Composes a
 * single static scene using every graphics primitive the framebuffer
 * exposes, to show how they combine — text, lines, rectangles, circles,
 * and a small PROGMEM bitmap logo — all landing in the same 1024-byte
 * buffer before one display() push makes the whole scene appear at
 * once, flicker-free.
 *
 * Hardware: identical to project 1 (ATmega328P @ 16 MHz, I2C OLED):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ SDA     │ PC4   │ I2C data, address 0x3C                     │
 *   │ SCL     │ PC5   │ I2C clock                                  │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * SSD1306 concepts introduced:
 *   - drawPixel(x, y, on) / getPixel(x, y) — the primitive everything
 *     else in the driver is built on; getPixel() reads the framebuffer
 *     back rather than the physical screen, so it reflects whatever was
 *     last drawn even before display() has pushed it out.
 *   - drawHLine / drawVLine / drawLine — straight lines; drawLine()
 *     handles any slope, drawHLine/drawVLine are cheaper special cases
 *     for the axis-aligned ones.
 *   - drawRect / fillRect — outline vs. filled rectangles, both taking
 *     an on/off polygon-fill flag rather than assuming "on".
 *   - drawCircle / fillCircle — outline vs. filled circles from a
 *     centre point and radius.
 *   - drawBitmap(x, y, w, h, pgmData) — blits a PROGMEM image straight
 *     into the framebuffer. Data must already be in the SSD1306's
 *     native page-column layout (h/8 pages x w columns, one byte per
 *     column per page, bit 0 = that page's top pixel) — exactly the
 *     format tools like image2cpp/LCD-image-converter export for this
 *     controller, and h must be a multiple of 8.
 */

#include <util/delay.h>
#include <avr/pgmspace.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/i2c.hpp>
#include <SSD1306.hpp>

using namespace MikroDuino;

SSD1306 oled(I2C, 0x3C);

// 16x16 MikroDuino "M" logo bitmap (2 pages x 16 columns, bit0=top).
static const uint8_t LOGO_BMP[2 * 16] PROGMEM = {
    // Page 0 — top half
    0xFF, 0x07, 0x1E, 0x78, 0x78, 0x1E, 0x07, 0x07,
    0x07, 0x1E, 0x78, 0x78, 0x1E, 0x07, 0xFF, 0x00,
    // Page 1 — bottom half
    0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00,
};

int main() {
    GPIO::inputPullup(PC4);
    GPIO::inputPullup(PC5);

    I2C.beginMaster(400000UL);
    oled.begin();

    while (true) {
        oled.clear();

        // ---- Title + horizontal rule ----
        oled.setCursor(0, 0);
        oled.print("Shapes + Bitmap");
        oled.drawHLine(0, 9, 128);

        // ---- Nested rectangle outlines, top-left ----
        oled.drawRect(0,  14, 40, 40);
        oled.drawRect(4,  18, 32, 32);
        oled.drawRect(8,  22, 24, 24);
        oled.drawRect(12, 26, 16, 16);

        // ---- Filled + hollow rectangles, top-middle ----
        oled.fillRect(46, 14, 18, 18, true);
        oled.fillRect(50, 18, 10, 10, false);   // hollow centre
        oled.fillRect(46, 36, 18, 18, true);

        // ---- Circles, top-right ----
        oled.drawCircle(96, 23, 9);
        oled.fillCircle(118, 23, 9);
        oled.drawCircle(107, 45, 9);
        oled.drawPixel(107, 45);   // centre dot on the outline circle

        // ---- Diagonal line fan, bottom-left ----
        for (uint8_t i = 0; i < 5; ++i)
            oled.drawLine(0, 63, static_cast<uint8_t>(14 * i + 2), 40);
        oled.drawVLine(70, 40, 24);

        // ---- Bitmap logo, bottom-right, tiled ----
        oled.drawBitmap(78, 44, 16, 16, LOGO_BMP);
        oled.drawBitmap(96, 44, 16, 16, LOGO_BMP);
        oled.drawBitmap(112, 44, 16, 16, LOGO_BMP);

        oled.display();
        _delay_ms(3000);

        // ---- getPixel() readback: prove the buffer holds what was drawn ----
        // Read BEFORE clear() — getPixel() reflects the RAM framebuffer, not
        // the physical screen, so it must be sampled while the scene above
        // is still sitting in the buffer.
        bool onRectEdge = oled.getPixel(12, 26);   // inside the innermost rect outline
        bool onBlank     = oled.getPixel(60, 60);   // empty area, should read false

        oled.clear();
        oled.setCursor(0, 0);
        oled.print("getPixel(12,26)=");
        oled.print(static_cast<int32_t>(onRectEdge ? 1 : 0));
        oled.setCursor(0, 8);
        oled.print("getPixel(60,60)=");
        oled.print(static_cast<int32_t>(onBlank ? 1 : 0));
        oled.display();
        _delay_ms(2000);
    }
}
