/*
 * SSD1306 Number Formatting + Hardware Display Control — MikroDuino
 * Module SDK
 *
 * Project 3 of 6 in the examples/Modules/SSD1306 series. Two unrelated
 * but frequently-needed things: printing numbers in decimal/hex without
 * hand-rolling a conversion, and the controller-level (not framebuffer-
 * level) commands that change how the WHOLE screen is presented —
 * contrast, inversion, mirroring, and power state.
 *
 * Hardware: identical to project 1 (ATmega328P @ 16 MHz, I2C OLED):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ SDA     │ PC4   │ I2C data, address 0x3C                     │
 *   │ SCL     │ PC5   │ I2C clock                                  │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * SSD1306 concepts introduced:
 *   - print(int32_t) — formats a signed decimal integer, sign included.
 *   - printU(uint32_t, base) — formats an unsigned value in decimal
 *     (base=10) or hex (base=16, no "0x" prefix added automatically).
 *   - contrast(0-255) — one SSD1306 command byte, no framebuffer
 *     change; swept below to show the full brightness range on the
 *     SAME pixels rather than needing to redraw anything.
 *   - invert(bool) — swaps which polarity (lit/unlit) each framebuffer
 *     bit is displayed as, again without touching the buffer itself —
 *     drawing more shapes while inverted would still add "on" pixels
 *     the normal way, they'd just render dark-on-light until invert()
 *     is called again.
 *   - flip(flipH, flipV) — mirrors the whole panel horizontally and/or
 *     vertically at the controller level (segment remap + COM scan
 *     direction), useful for boards where the OLED module ends up
 *     mounted upside-down or reversed in the final enclosure.
 *   - on(bool) — cuts or restores the panel's own power stage; the
 *     framebuffer and all other settings are preserved while off, so
 *     on(true) resumes exactly where it left off.
 */

#include <util/delay.h>
#include <avr/pgmspace.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/i2c.hpp>
#include <SSD1306.hpp>

using namespace MikroDuino;

SSD1306 oled(I2C, 0x3C);

static void showNumbers() {
    oled.clear();
    oled.setCursor(0, 0);
    oled.print("Number formatting:");

    oled.setCursor(0, 16);
    oled.print("Dec: ");
    oled.print(static_cast<int32_t>(-12345));

    oled.setCursor(0, 24);
    oled.print("Hex: 0x");
    oled.printU(0xDEAD, 16);

    oled.setCursor(0, 32);
    oled.print("Pos: ");
    oled.print(static_cast<int32_t>(32767));

    oled.setCursor(0, 40);
    oled.print("Zero: ");
    oled.print(static_cast<int32_t>(0));

    oled.display();
    _delay_ms(2500);
}

static void sweepContrast() {
    oled.clear();
    oled.setCursor(0, 0);
    oled.print("Contrast sweep");
    oled.fillRect(0, 16, 128, 16, true);   // solid reference band
    oled.display();

    for (uint8_t c = 0; c < 250; c += 5) { oled.contrast(c); _delay_ms(15); }
    for (uint8_t c = 250; c > 10; c -= 5) { oled.contrast(c); _delay_ms(15); }
    oled.contrast(0xCF);   // restore default
}

static void demoInvert() {
    oled.clear();
    oled.setCursor(20, 24);
    oled.print("invert(true)");
    oled.drawRect(0, 0, 128, 64);
    oled.display();
    _delay_ms(600);
    oled.invert(true);
    _delay_ms(1500);
    oled.invert(false);
    _delay_ms(400);
}

static void demoFlip() {
    oled.clear();
    oled.setCursor(0, 0);
    oled.print("Flip test: ABC");
    oled.drawLine(0, 10, 60, 63);
    oled.fillCircle(100, 40, 15);
    oled.display();
    _delay_ms(1000);

    oled.flip(true, false);   // mirror horizontal
    _delay_ms(800);
    oled.flip(false, true);   // mirror vertical
    _delay_ms(800);
    oled.flip(true, true);    // both
    _delay_ms(800);
    oled.flip(false, false);  // restore
    _delay_ms(400);
}

static void demoOnOff() {
    oled.clear();
    oled.setCursor(16, 28);
    oled.print("on(false/true)");
    oled.display();
    _delay_ms(500);
    oled.on(false);
    _delay_ms(700);
    oled.on(true);
    _delay_ms(500);
    oled.on(false);
    _delay_ms(400);
    oled.on(true);
}

int main() {
    GPIO::inputPullup(PC4);
    GPIO::inputPullup(PC5);

    I2C.beginMaster(400000UL);
    oled.begin();

    while (true) {
        showNumbers();
        sweepContrast();
        demoInvert();
        demoFlip();
        demoOnOff();
    }
}
