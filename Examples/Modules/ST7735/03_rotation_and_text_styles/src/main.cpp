/*
 * ST7735 Rotation + Text Sizing/Transparency — MikroDuino Module SDK
 *
 * Project 3 of 6 in the examples/Modules/ST7735 series. Two related
 * "how text and the coordinate system actually behave" topics: cycling
 * through all four screen rotations (watching width()/height() swap for
 * the landscape ones), and the difference between transparent and
 * opaque text at several sizes.
 *
 * Hardware: identical to projects 1-2 (ATmega328P @ 16 MHz, SPI color TFT).
 *
 * ST7735 concepts introduced:
 *   - setRotation(rotation) — Deg0/Deg180 are portrait (width() x
 *     height() stays whatever the panel's native size is); Deg90/Deg270
 *     are landscape (width() and height() SWAP). Every coordinate this
 *     project draws is computed FROM tft.width()/tft.height() rather
 *     than hardcoded, so the same drawing code produces a correctly
 *     centered frame at all four rotations without an if/else per case.
 *   - setTextSize(n) — scales the 5x7 font by an integer factor; a
 *     size-3 character is 18x24 pixels (6x8 unscaled cell x 3).
 *   - setTextColor(fg) — the ONE-argument, TRANSPARENT form: only the
 *     glyph's own pixels are drawn, so text can be laid over whatever
 *     is already on screen (the colored background bars below) without
 *     punching an opaque rectangle out of it first. This costs more per
 *     character than the opaque form project 1 used (see ST7735.hpp's
 *     class-level performance note — transparent text can't use one
 *     continuous burst per glyph the way opaque text does), which is
 *     an acceptable trade for text that only changes a few times a
 *     second, as here.
 *   - print(const char*) — reused from project 1, now at three
 *     different setTextSize() values in a row for direct comparison.
 */

#include <util/delay.h>
#include <mikroduino/spi.hpp>
#include <ST7735.hpp>

using namespace MikroDuino;

ST7735 tft(PB2, PB1, PB0);

static void drawFrame(ST7735::Rotation rotation, const char* label) {
    tft.setRotation(rotation);
    int16_t w = static_cast<int16_t>(tft.width());
    int16_t h = static_cast<int16_t>(tft.height());

    tft.fillScreen(ST7735::BLACK);
    tft.drawRect(0, 0, w, h, ST7735::WHITE);

    // Opaque label, top-left — dimensions come from width()/height(),
    // not the panel's native portrait size, so this lands correctly in
    // every rotation.
    tft.setTextColor(ST7735::BLACK, ST7735::WHITE);
    tft.setCursor(4, 4);
    tft.print(label);
    tft.print(" ");
    tft.print(static_cast<int32_t>(w));
    tft.print("x");
    tft.print(static_cast<int32_t>(h));

    // A colored band across the middle, with transparent text laid
    // directly over it at three sizes stacked below one another.
    int16_t bandY = static_cast<int16_t>(h / 2 - 24);
    tft.fillRect(0, bandY, w, 48, ST7735::BLUE);

    tft.setTextColor(ST7735::WHITE);   // transparent — draws over the blue band
    tft.setTextSize(1);
    tft.setCursor(4, static_cast<int16_t>(bandY + 4));
    tft.print("size 1");

    tft.setTextSize(2);
    tft.setCursor(4, static_cast<int16_t>(bandY + 16));
    tft.print("size 2");

    tft.setTextSize(3);
    tft.setCursor(4, static_cast<int16_t>(bandY + 32));
    if (w >= 4 + 18 * 6) tft.print("sz 3");   // only if it fits this rotation's width

    tft.setTextSize(1);   // restore for the next frame's label text
}

int main() {
    SPI.beginMaster(SPIClockDiv::DIV2, SPIMode::Mode0);
    tft.begin(ST7735::Variant::BlackTab);

    while (true) {
        drawFrame(ST7735::Rotation::Deg0,   "0deg");
        _delay_ms(2000);
        drawFrame(ST7735::Rotation::Deg90,  "90deg");
        _delay_ms(2000);
        drawFrame(ST7735::Rotation::Deg180, "180deg");
        _delay_ms(2000);
        drawFrame(ST7735::Rotation::Deg270, "270deg");
        _delay_ms(2000);
    }
}
