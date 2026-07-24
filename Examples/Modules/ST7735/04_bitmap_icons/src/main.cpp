/*
 * ST7735 PROGMEM Bitmaps — drawBitmap565() + drawBitmap1() — MikroDuino
 * Module SDK
 *
 * Project 4 of 6 in the examples/Modules/ST7735 series. This driver has
 * TWO distinct bitmap formats, covered side by side here: a straight
 * 16-bit-per-pixel color dump, and a 1-bit-per-pixel icon format that
 * can be drawn either opaque (two colors, one fast burst) or transparent
 * (only the "on" pixels are drawn, letting the icon sit on top of
 * whatever is already on screen).
 *
 * Hardware: identical to projects 1-3 (ATmega328P @ 16 MHz, SPI color TFT).
 *
 * ST7735 concepts introduced:
 *   - drawBitmap565(x, y, w, h, pgmData) — a row-major array of raw
 *     RGB565 uint16_t values, one per pixel, straight from PROGMEM to
 *     GRAM in one continuous burst. This is what an image converter
 *     that exports "RGB565 array" (rather than a 1bpp icon format)
 *     produces; RAINBOW_BMP below builds one by hand from six of the
 *     named color constants instead, to keep this project
 *     self-contained. Must be placed fully on-screen — see the
 *     header's doc comment on why a straight PROGMEM streamer can't
 *     clip a partially offscreen bitmap.
 *   - drawBitmap1(x, y, w, h, pgmData, fg, bg, opaque) — row-major,
 *     MSB-first, each row padded to a byte boundary: the format every
 *     "image to Arduino code" converter targeting Adafruit_GFX-style
 *     libraries (image2cpp and similar) produces for a monochrome icon.
 *     DIAMOND_BMP below is a plain filled diamond, hand-built from a
 *     symmetric per-row width formula rather than traced from a picture,
 *     so its bit pattern is easy to verify by eye against the shape it
 *     draws.
 *   - opaque=true vs. opaque=false on the SAME 1bpp bitmap: the first
 *     draws every pixel (fg for "on" bits, bg for "off" bits) in one
 *     burst; the second only draws the fg pixels and leaves "off" bits
 *     showing whatever was already there — visibly different against a
 *     non-solid-color background, which this project demonstrates by
 *     drawing the same diamond over a colored band both ways.
 */

#include <util/delay.h>
#include <avr/pgmspace.h>
#include <mikroduino/spi.hpp>
#include <ST7735.hpp>

using namespace MikroDuino;

ST7735 tft(PB2, PB1, PB0);

// 8x6 color swatch, one solid-color row at a time — six of the driver's
// own named constants, so there's no hand-computed RGB565 hex to get
// wrong. Row-major: 8 entries per row, 6 rows.
static const uint16_t RAINBOW_BMP[8 * 6] PROGMEM = {
    ST7735::RED,    ST7735::RED,    ST7735::RED,    ST7735::RED,
    ST7735::RED,    ST7735::RED,    ST7735::RED,    ST7735::RED,
    ST7735::ORANGE, ST7735::ORANGE, ST7735::ORANGE, ST7735::ORANGE,
    ST7735::ORANGE, ST7735::ORANGE, ST7735::ORANGE, ST7735::ORANGE,
    ST7735::YELLOW, ST7735::YELLOW, ST7735::YELLOW, ST7735::YELLOW,
    ST7735::YELLOW, ST7735::YELLOW, ST7735::YELLOW, ST7735::YELLOW,
    ST7735::GREEN,  ST7735::GREEN,  ST7735::GREEN,  ST7735::GREEN,
    ST7735::GREEN,  ST7735::GREEN,  ST7735::GREEN,  ST7735::GREEN,
    ST7735::CYAN,   ST7735::CYAN,   ST7735::CYAN,   ST7735::CYAN,
    ST7735::CYAN,   ST7735::CYAN,   ST7735::CYAN,   ST7735::CYAN,
    ST7735::BLUE,   ST7735::BLUE,   ST7735::BLUE,   ST7735::BLUE,
    ST7735::BLUE,   ST7735::BLUE,   ST7735::BLUE,   ST7735::BLUE,
};

// 16x16 filled diamond, row-major MSB-first, 2 bytes/row. Each row's
// width grows by 2px until the middle (rows 7-8, fully filled) then
// shrinks by 2px back down — a plain geometric shape, not traced art,
// so the bit pattern below is easy to check against the shape it draws.
static const uint8_t DIAMOND_BMP[16 * 2] PROGMEM = {
    0x01, 0x80,   0x03, 0xC0,   0x07, 0xE0,   0x0F, 0xF0,
    0x1F, 0xF8,   0x3F, 0xFC,   0x7F, 0xFE,   0xFF, 0xFF,
    0xFF, 0xFF,   0x7F, 0xFE,   0x3F, 0xFC,   0x1F, 0xF8,
    0x0F, 0xF0,   0x07, 0xE0,   0x03, 0xC0,   0x01, 0x80,
};

int main() {
    SPI.beginMaster(SPIClockDiv::DIV2, SPIMode::Mode0);
    tft.begin(ST7735::Variant::BlackTab);

    tft.fillScreen(ST7735::BLACK);
    tft.setTextColor(ST7735::WHITE, ST7735::BLACK);

    // ---- drawBitmap565: the rainbow swatch, scaled up by drawing it
    // three times side by side (the driver has no bitmap scaling of its
    // own, so "bigger" just means "draw the same small source more than
    // once") ----
    tft.setCursor(4, 4);
    tft.print("drawBitmap565:");
    tft.drawBitmap565(4,  16, 8, 6, RAINBOW_BMP);
    tft.drawBitmap565(16, 16, 8, 6, RAINBOW_BMP);
    tft.drawBitmap565(28, 16, 8, 6, RAINBOW_BMP);

    // ---- drawBitmap1, opaque: fg+bg both drawn, one fast burst ----
    tft.setCursor(4, 40);
    tft.print("opaque:");
    tft.fillRect(4, 52, 40, 20, ST7735::BLUE);   // background band to draw over
    tft.drawBitmap1(14, 54, 16, 16, DIAMOND_BMP, ST7735::YELLOW, ST7735::BLUE, true);

    // ---- drawBitmap1, transparent: only "on" pixels drawn, the blue
    // band shows through around the diamond's edges either way, but this
    // makes it obvious the "off" pixels were never touched ----
    tft.setCursor(4, 80);
    tft.print("transparent:");
    tft.fillRect(4, 92, 40, 20, ST7735::BLUE);
    tft.drawBitmap1(14, 94, 16, 16, DIAMOND_BMP, ST7735::YELLOW, ST7735::BLUE, false);

    // Static scene: nothing to update once drawn.
    while (true) {}
}
