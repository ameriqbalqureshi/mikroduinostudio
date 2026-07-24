/*
 * LCD Live Sensor Dashboard — ADC + Custom-Character Bar Graph —
 * MikroDuino Module SDK
 *
 * Project 5 of 6 in the examples/Modules/LCD series. Combines the LCD
 * module with the core ADC peripheral to build a genuinely useful
 * pattern: a smooth, high-resolution bar graph made of custom
 * characters, reading a potentiometer live.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ RS      │ PB0   │ LCD (same wiring as projects 1-4)          │
 *   │ EN      │ PB1   │                                            │
 *   │ D4-D7   │ PB2-5 │                                            │
 *   │ ADC0    │ PC0   │ Potentiometer wiper — outer legs to 5V/GND │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * Why a bar graph needs custom characters at all: each LCD character
 * CELL is an all-or-nothing unit — a single cell can show "empty" or
 * "full" using ordinary text, but nothing in between, which limits a
 * naive bar graph to a resolution of one bar-graph "notch" per 8 pixels.
 * By defining 5 CGRAM glyphs — 1/5, 2/5, 3/5, 4/5, and 5/5 (full) of a
 * cell's width filled — a bar graph spanning N cells gets 5xN resolution
 * steps instead of just N. This project uses 10 cells, giving 50 visible
 * fill levels across the row from a 10-bit (0-1023) ADC reading.
 *
 * LCD concepts reused from projects 1-4:
 *   - CharLCD(...), begin(), createChar() (project 4) — this time with
 *     FIVE glyphs instead of four, filling CGRAM slots 0-4 and leaving
 *     slots 5-7 unused.
 *   - setCursor(), print(const char*), print(int32_t), writeChar()
 *     (writeChar() is what actually draws each bar-graph CELL, exactly
 *     as it drew each battery-animation frame in project 4).
 *
 * Non-LCD concept introduced:
 *   - ADCDriver (mikroduino/adc.hpp) — ADC_Driver.begin() and
 *     ADC_Driver.read(channel), the same blocking single-conversion API
 *     examples/pwm/05_servo_angle_control used to read a potentiometer.
 *     This project reads channel 0 (ADC0/PC0) once per refresh and maps
 *     the 0-1023 result onto the 50-step bar graph and a plain-text
 *     percentage, entirely with integer arithmetic (no floating point
 *     needed anywhere in this file).
 */

#include <util/delay.h>
#include <mikroduino/adc.hpp>
#include <LCD.hpp>

using namespace MikroDuino;

CharLCD lcd(PB0, PB1, PB2, PB3, PB4, PB5);

static void delay_ms(uint16_t ms) { while (ms--) _delay_ms(1); }

static constexpr uint8_t BAR_CELLS = 10;   // bar graph spans 10 LCD character cells

// Five fill levels, 1/5 through 5/5 (full) of a cell's 5-pixel width,
// uploaded into CGRAM slots 0-4. Slot 4 (full) could instead reuse a
// solid-block character from the LCD's built-in ROM font, but which
// code point that is varies between HD44780 ROM variants (A00 vs A02) —
// defining it ourselves keeps this project's appearance identical on
// any HD44780-compatible module, at the cost of one extra CGRAM slot.
static const uint8_t FILL_1_5[8] = { 0b10000,0b10000,0b10000,0b10000,0b10000,0b10000,0b10000,0b10000 };
static const uint8_t FILL_2_5[8] = { 0b11000,0b11000,0b11000,0b11000,0b11000,0b11000,0b11000,0b11000 };
static const uint8_t FILL_3_5[8] = { 0b11100,0b11100,0b11100,0b11100,0b11100,0b11100,0b11100,0b11100 };
static const uint8_t FILL_4_5[8] = { 0b11110,0b11110,0b11110,0b11110,0b11110,0b11110,0b11110,0b11110 };
static const uint8_t FILL_5_5[8] = { 0b11111,0b11111,0b11111,0b11111,0b11111,0b11111,0b11111,0b11111 };

// Draw the bar graph on the given row for a 0-1023 ADC reading.
static void drawBar(uint8_t row, uint16_t adcValue) {
    // fillUnits: 0-50, one unit per 1/5th of a single cell.
    uint16_t fillUnits = static_cast<uint16_t>((static_cast<uint32_t>(adcValue) * BAR_CELLS * 5) / 1023u);
    uint8_t fullCells      = static_cast<uint8_t>(fillUnits / 5);
    uint8_t remainderLevel = static_cast<uint8_t>(fillUnits % 5);   // 0 = none, 1-4 = partial glyph index+1

    lcd.setCursor(0, row);
    for (uint8_t cell = 0; cell < BAR_CELLS; ++cell) {
        if (cell < fullCells) {
            lcd.writeChar(4);   // FILL_5_5 — fully filled cell
        } else if (cell == fullCells && remainderLevel > 0) {
            lcd.writeChar(static_cast<uint8_t>(remainderLevel - 1));   // FILL_1_5 .. FILL_4_5
        } else {
            lcd.print(" ");     // empty cell
        }
    }
}

int main() {
    lcd.begin();
    lcd.createChar(0, FILL_1_5);
    lcd.createChar(1, FILL_2_5);
    lcd.createChar(2, FILL_3_5);
    lcd.createChar(3, FILL_4_5);
    lcd.createChar(4, FILL_5_5);

    ADC_Driver.begin();   // defaults: AVCC reference, DIV128 prescaler

    while (true) {
        uint16_t raw = ADC_Driver.read(0);
        uint16_t percent = static_cast<uint16_t>((static_cast<uint32_t>(raw) * 100u) / 1023u);

        // "ADC " + up to 4 digits + " " + up to 3 digits + "%  " = 15
        // characters max ("ADC 1023 100%  ") — stays within a 16-column
        // display's visible width; see project 3 for what happens to
        // text that doesn't.
        lcd.setCursor(0, 0);
        lcd.print("ADC ");
        lcd.print(static_cast<int32_t>(raw));
        lcd.print(" ");
        lcd.print(static_cast<int32_t>(percent));
        lcd.print("%  ");

        drawBar(1, raw);

        delay_ms(150);
    }
}
