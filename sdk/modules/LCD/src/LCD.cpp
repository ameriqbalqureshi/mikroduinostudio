#include "../include/LCD.hpp"
#include <stdlib.h>

namespace MikroDuino {

const uint8_t CharLCD::ROW_OFFSETS[4] = { 0x00, 0x40, 0x14, 0x54 };

void CharLCD::begin() {
    // Set all pins as output
    GPIO::output(_rs);
    GPIO::output(_en);
    for (uint8_t i = 0; i < 4; ++i) GPIO::output(_d[i]);

    // Wait 50 ms for LCD power-on
    _delay_ms(50);

    GPIO::clear(_rs);
    GPIO::clear(_en);

    // 4-bit init sequence (see HD44780 datasheet)
    write4bits(0x03); _delay_ms(5);
    write4bits(0x03); _delay_ms(1);
    write4bits(0x03); _delay_us(150);
    write4bits(0x02); // set 4-bit mode

    // Function set: 4-bit, 2-line, 5x8
    command(LCD_FUNCTIONSET | LCD_4BITMODE | LCD_2LINE | LCD_5x8DOTS);
    // Display control: on, no cursor, no blink
    command(LCD_DISPLAYCONTROL | LCD_DISPLAYON);
    // Clear
    clear();
    // Entry mode: left to right, no shift
    command(LCD_ENTRYMODESET | LCD_ENTRYLEFT);
}

void CharLCD::clear() {
    command(LCD_CLEARDISPLAY);
    _delay_ms(2);
}

void CharLCD::home() {
    command(LCD_RETURNHOME);
    _delay_ms(2);
}

void CharLCD::setCursor(uint8_t col, uint8_t row) {
    if (row >= 4) row = 3;
    command(LCD_SETDDRAMADDR | (col + ROW_OFFSETS[row]));
}

void CharLCD::print(const char* str) {
    while (*str) writeChar(static_cast<uint8_t>(*str++));
}

void CharLCD::print(int32_t value) {
    char buf[12];
    ltoa(value, buf, 10);
    print(buf);
}

void CharLCD::printHex(uint8_t value) {
    const char hex[] = "0123456789ABCDEF";
    writeChar(hex[value >> 4]);
    writeChar(hex[value & 0x0F]);
}

void CharLCD::display(bool on) {
    if (on) _displayControl |=  LCD_DISPLAYON;
    else    _displayControl &= ~LCD_DISPLAYON;
    command(LCD_DISPLAYCONTROL | _displayControl);
}

void CharLCD::cursor(bool on) {
    if (on) _displayControl |=  LCD_CURSORON;
    else    _displayControl &= ~LCD_CURSORON;
    command(LCD_DISPLAYCONTROL | _displayControl);
}

void CharLCD::blink(bool on) {
    if (on) _displayControl |=  LCD_BLINKON;
    else    _displayControl &= ~LCD_BLINKON;
    command(LCD_DISPLAYCONTROL | _displayControl);
}

void CharLCD::scrollLeft()  { command(LCD_CURSORSHIFT | 0x08); }
void CharLCD::scrollRight() { command(LCD_CURSORSHIFT | 0x0C); }

void CharLCD::createChar(uint8_t index, const uint8_t bitmap[8]) {
    command(LCD_SETCGRAMADDR | ((index & 0x07) << 3));
    for (uint8_t i = 0; i < 8; ++i) writeChar(bitmap[i]);
}

void CharLCD::writeChar(uint8_t c) {
    send(c, true);
}

// ── Private ──────────────────────────────────────────────────────────────────

void CharLCD::command(uint8_t cmd) { send(cmd, false); }

void CharLCD::send(uint8_t value, bool isData) {
    GPIO::write(_rs, isData);
    write4bits(value >> 4);
    write4bits(value & 0x0F);
}

void CharLCD::write4bits(uint8_t value) {
    for (uint8_t i = 0; i < 4; ++i)
        GPIO::write(_d[i], (value >> i) & 0x01);
    pulse();
}

void CharLCD::pulse() {
    GPIO::clear(_en);
    _delay_us(1);
    GPIO::set(_en);
    _delay_us(1);
    GPIO::clear(_en);
    _delay_us(50);
}

} // namespace MikroDuino
