#pragma once
/*
 * MikroDuino LCD Module
 * 4-bit parallel interface driver for HD44780-compatible character LCD.
 * Supports 16x2, 20x4, etc.
 */

#include <mikroduino/gpio.hpp>
#include <stdint.h>
#include <util/delay.h>

namespace MikroDuino {

class CharLCD {
public:
    // pins: RS, EN, D4, D5, D6, D7
    constexpr CharLCD(
        uint8_t rs, uint8_t en,
        uint8_t d4, uint8_t d5, uint8_t d6, uint8_t d7,
        uint8_t cols = 16, uint8_t rows = 2
    ) : _rs(rs), _en(en),
        _d{d4, d5, d6, d7},
        _cols(cols), _rows(rows) {}

    void begin();
    void clear();
    void home();
    void setCursor(uint8_t col, uint8_t row);
    void print(const char* str);
    void print(int32_t value);
    void printHex(uint8_t value);

    void display(bool on);
    void cursor(bool on);
    void blink(bool on);
    void scrollLeft();
    void scrollRight();
    void createChar(uint8_t index, const uint8_t bitmap[8]);
    void writeChar(uint8_t c);

private:
    uint8_t _rs, _en;
    uint8_t _d[4];
    uint8_t _cols, _rows;
    uint8_t _displayControl = 0x0C;  // display on, cursor off, blink off

    void command(uint8_t cmd);
    void send(uint8_t value, bool isData);
    void write4bits(uint8_t value);
    void pulse();

    static constexpr uint8_t LCD_CLEARDISPLAY   = 0x01;
    static constexpr uint8_t LCD_RETURNHOME     = 0x02;
    static constexpr uint8_t LCD_ENTRYMODESET   = 0x04;
    static constexpr uint8_t LCD_DISPLAYCONTROL = 0x08;
    static constexpr uint8_t LCD_CURSORSHIFT    = 0x10;
    static constexpr uint8_t LCD_FUNCTIONSET    = 0x20;
    static constexpr uint8_t LCD_SETCGRAMADDR   = 0x40;
    static constexpr uint8_t LCD_SETDDRAMADDR   = 0x80;
    static constexpr uint8_t LCD_ENTRYLEFT      = 0x02;
    static constexpr uint8_t LCD_DISPLAYON      = 0x04;
    static constexpr uint8_t LCD_CURSORON       = 0x02;
    static constexpr uint8_t LCD_BLINKON        = 0x01;
    static constexpr uint8_t LCD_4BITMODE       = 0x00;
    static constexpr uint8_t LCD_2LINE          = 0x08;
    static constexpr uint8_t LCD_5x8DOTS        = 0x00;

    static const uint8_t ROW_OFFSETS[4];
};

} // namespace MikroDuino
