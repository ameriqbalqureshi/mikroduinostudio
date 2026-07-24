/*
 * LCD Custom Characters — createChar() Battery Animation —
 * MikroDuino Module SDK
 *
 * Project 4 of 6 in the examples/Modules/LCD series. The HD44780 has 8
 * slots of CGRAM (Character Generator RAM) — user-definable 5x8-pixel
 * glyphs, addressed as ordinary character codes 0-7. This project
 * defines four battery-level icons and animates them into a simple
 * "charging" indicator, the same trick used for progress bars, custom
 * bullet points, degree/arrow symbols, and any other glyph the built-in
 * ROM font doesn't have.
 *
 * Hardware: identical to projects 1-3 — RS=PB0, EN=PB1, D4-D7=PB2-PB5.
 *
 * LCD concepts introduced:
 *   - createChar(index, bitmap) — index is 0-7 (CharLCD masks it with
 *     0x07 internally, so passing 8 silently wraps to slot 0 — treat
 *     0-7 as the real usable range). bitmap is 8 bytes, one per pixel
 *     ROW of the glyph's 5x8 cell; only the low 5 bits of each byte are
 *     meaningful (bit 4 = leftmost column, bit 0 = rightmost). Uploading
 *     a glyph moves the LCD's internal address pointer into CGRAM
 *     territory — begin()'s font-mode setting (5x8 dots) is what makes
 *     the CGRAM cells exactly 8 rows tall; a 5x10-dot font (not used by
 *     this driver) would need 11-byte bitmaps and only 4 usable slots
 *     instead of 8.
 *   - writeChar(0-7) to actually DISPLAY a custom glyph — since CGRAM
 *     slots are addressed as character codes 0-7, printing byte value 3
 *     shows whatever createChar(3, ...) uploaded, exactly the way
 *     printing byte value 'A' (65) shows the ROM font's capital A.
 *     print() would work too (a string containing byte value 3 embedded
 *     in it), but writeChar() is clearer when the "character" isn't
 *     printable ASCII text.
 *
 * LCD concepts reused from projects 1-3:
 *   - CharLCD(...), begin(), clear(), setCursor(), print(const char*),
 *     print(int32_t).
 */

#include <util/delay.h>
#include <LCD.hpp>

using namespace MikroDuino;

CharLCD lcd(PB0, PB1, PB2, PB3, PB4, PB5);

static void delay_ms(uint16_t ms) { while (ms--) _delay_ms(1); }

// Four battery glyphs, empty to full, each 8 bytes (one per pixel row).
// Only bits 4-0 of each byte matter (5 pixels wide); the outline (top
// nub, side walls, bottom cap) is identical in every frame, only the
// fill rows change, which is what makes the animation read as "filling
// up" rather than four unrelated icons.
static const uint8_t BATTERY_EMPTY[8] = {
    0b01110,
    0b11111,
    0b10001,
    0b10001,
    0b10001,
    0b10001,
    0b10001,
    0b11111,
};
static const uint8_t BATTERY_LOW[8] = {
    0b01110,
    0b11111,
    0b10001,
    0b10001,
    0b10001,
    0b10001,
    0b11111,
    0b11111,
};
static const uint8_t BATTERY_MID[8] = {
    0b01110,
    0b11111,
    0b10001,
    0b10001,
    0b11111,
    0b11111,
    0b11111,
    0b11111,
};
static const uint8_t BATTERY_FULL[8] = {
    0b01110,
    0b11111,
    0b11111,
    0b11111,
    0b11111,
    0b11111,
    0b11111,
    0b11111,
};

static constexpr uint8_t FRAME_COUNT = 4;
static const uint8_t PERCENT[FRAME_COUNT] = { 0, 33, 66, 100 };

int main() {
    lcd.begin();

    // Upload all four glyphs into CGRAM slots 0-3, once, at startup —
    // they stay resident until the LCD is powered off or re-initialised,
    // so there's no need to re-upload them inside the animation loop.
    lcd.createChar(0, BATTERY_EMPTY);
    lcd.createChar(1, BATTERY_LOW);
    lcd.createChar(2, BATTERY_MID);
    lcd.createChar(3, BATTERY_FULL);

    lcd.setCursor(0, 0);
    lcd.print("Charging...");

    while (true) {
        for (uint8_t frame = 0; frame < FRAME_COUNT; ++frame) {
            lcd.setCursor(0, 1);
            lcd.writeChar(frame);   // draws whichever CGRAM glyph slot 0-3 holds
            lcd.print(" ");
            lcd.print(static_cast<int32_t>(PERCENT[frame]));
            lcd.print("%   ");      // pad over a shorter previous percentage

            delay_ms(600);
        }
        delay_ms(400);   // brief pause on "100%" before looping back to empty
    }
}
