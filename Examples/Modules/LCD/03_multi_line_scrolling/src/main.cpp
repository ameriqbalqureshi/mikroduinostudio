/*
 * LCD Multi-Row Addressing & Scrolling — MikroDuino Module SDK
 *
 * Project 3 of 6 in the examples/Modules/LCD series. Explains a fact
 * about the HD44780 that trips up nearly everyone the first time: each
 * display ROW has 40 characters of DDRAM behind it, even on a 16-column
 * display — only the first 16 are ever VISIBLE at once. scrollLeft() /
 * scrollRight() don't move text around per se; they move which 16-wide
 * WINDOW into that 40-character row is currently shown. This project
 * writes a message longer than 16 characters and scrolls it into view,
 * building a classic marquee/ticker effect.
 *
 * Hardware: identical to projects 1-2 — RS=PB0, EN=PB1, D4-D7=PB2-PB5.
 *
 * LCD concepts introduced:
 *   - scrollLeft() / scrollRight() — shift the visible window one
 *     column at a time. Critically, this is a hardware-level operation
 *     that shifts EVERY row at once — there is no way to scroll one row
 *     independently of another on real HD44780 hardware. This project
 *     puts short text on row 1 specifically to make that fact
 *     unmistakable: watch it slide out of view right along with row 0's
 *     longer message, even though row 1's own text easily fits within
 *     16 columns on its own.
 *   - setCursor(col, row) reused across BOTH of a 16x2's rows — and,
 *     per CharLCD's ROW_OFFSETS table, the exact same call pattern
 *     extends to row 2 and row 3 without any other code changes on a
 *     20x4 module (just change the constructor's cols/rows arguments;
 *     everything else in this file is unaffected).
 *
 * LCD concepts reused from projects 1-2:
 *   - CharLCD(...), begin(), clear(), print(const char*).
 */

#include <util/delay.h>
#include <LCD.hpp>

using namespace MikroDuino;

CharLCD lcd(PB0, PB1, PB2, PB3, PB4, PB5);

static void delay_ms(uint16_t ms) { while (ms--) _delay_ms(1); }

int main() {
    lcd.begin();

    // Row 0: a message longer than the visible 16 columns — the extra
    // characters land in DDRAM columns 16-31, off-screen until scrolled
    // into view.
    lcd.setCursor(0, 0);
    lcd.print("MikroDuino LCD marquee demo!");

    // Row 1: short text that fits entirely within the visible 16
    // columns on its own. It will still slide out of sight during the
    // scroll below — proof that scrolling is a whole-display operation,
    // not a per-row one.
    lcd.setCursor(0, 1);
    lcd.print("(whole screen scrolls)");

    delay_ms(1500);

    static constexpr uint8_t SCROLL_STEPS = 16;   // enough to fully reveal row 0's overflow

    while (true) {
        for (uint8_t i = 0; i < SCROLL_STEPS; ++i) {
            lcd.scrollLeft();
            delay_ms(300);
        }
        delay_ms(800);

        for (uint8_t i = 0; i < SCROLL_STEPS; ++i) {
            lcd.scrollRight();
            delay_ms(300);
        }
        delay_ms(800);
    }
}
