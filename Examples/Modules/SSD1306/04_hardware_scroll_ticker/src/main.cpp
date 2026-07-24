/*
 * SSD1306 Hardware Scrolling Ticker — MikroDuino Module SDK
 *
 * Project 4 of 6 in the examples/Modules/SSD1306 series. The SSD1306
 * controller can scroll its own GDDRAM content continuously, entirely
 * on its own, once told to start — no CPU time and no further I2C
 * traffic is spent per scrolled frame, unlike a "software" marquee
 * that would have to redraw and re-push the framebuffer every step.
 * This project fills the screen with text once, then lets the display
 * controller do all the work of scrolling it.
 *
 * Hardware: identical to project 1 (ATmega328P @ 16 MHz, I2C OLED):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ SDA     │ PC4   │ I2C data, address 0x3C                     │
 *   │ SCL     │ PC5   │ I2C clock                                  │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * SSD1306 concepts introduced:
 *   - scrollRight(startPage, endPage) / scrollLeft(...) — continuous
 *     horizontal scroll of the GDDRAM rows spanned by [startPage,
 *     endPage] (each page is 8 pixel rows tall, 0-7 for the full
 *     64-pixel height). Once started, the controller keeps scrolling
 *     by itself until stopScroll() is called — the MCU is completely
 *     free in the meantime.
 *   - scrollDiagRight(...) / scrollDiagLeft(...) — the same idea, but
 *     also scrolling vertically, so content wraps both across and up
 *     the panel at once.
 *   - stopScroll() — halts whichever scroll is active. Each scroll*()
 *     call also calls stopScroll() first internally, so switching
 *     directly between scroll modes without an explicit stop in
 *     between is safe.
 *
 * IMPORTANT: hardware scrolling moves the CONTROLLER's own GDDRAM, not
 * this class's RAM framebuffer — the two go out of sync the moment a
 * scroll starts. Calling clear()/setCursor()/print()/display() again
 * while scrolling would silently overwrite the scrolled content with a
 * stale buffer. Always stopScroll() before drawing anything new.
 */

#include <util/delay.h>
#include <avr/pgmspace.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/i2c.hpp>
#include <SSD1306.hpp>

using namespace MikroDuino;

SSD1306 oled(I2C, 0x3C);

static void fillTickerRows() {
    oled.clear();
    for (uint8_t row = 0; row < 8; ++row) {
        oled.setCursor(0, static_cast<uint8_t>(row * 8));
        oled.print("Row ");
        oled.print(static_cast<int32_t>(row));
        oled.print(" -- MikroDuino SSD1306 ticker");
    }
    oled.display();
}

int main() {
    GPIO::inputPullup(PC4);
    GPIO::inputPullup(PC5);

    I2C.beginMaster(400000UL);
    oled.begin();

    while (true) {
        fillTickerRows();
        _delay_ms(500);

        // Full-height scroll right, then left
        oled.scrollRight(0, 7);
        _delay_ms(3000);
        oled.stopScroll();
        _delay_ms(300);

        oled.scrollLeft(0, 7);
        _delay_ms(3000);
        oled.stopScroll();
        _delay_ms(300);

        // Diagonal scroll over just the middle four pages (rows 16-47)
        oled.scrollDiagRight(2, 5);
        _delay_ms(3000);
        oled.stopScroll();
        _delay_ms(300);

        oled.scrollDiagLeft(2, 5);
        _delay_ms(3000);
        oled.stopScroll();
        _delay_ms(500);

        // Redraw a fresh frame before the next lap — display() pushes
        // this class's RAM framebuffer wholesale over GDDRAM, so it's
        // what actually resyncs the two after scrolling (stopScroll()
        // above only halted the controller's own autonomous scrolling;
        // it does not touch GDDRAM content on its own).
        oled.clear();
        oled.setCursor(20, 28);
        oled.print("Lap complete");
        oled.display();
        _delay_ms(1500);
    }
}
