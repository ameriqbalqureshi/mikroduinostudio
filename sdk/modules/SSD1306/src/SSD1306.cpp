#include "../include/SSD1306.hpp"

namespace MikroDuino {

// ================================================================
// 5×7 ASCII font — 95 characters (0x20 space … 0x7E tilde)
// Each character: 5 bytes (columns left→right), bit 0 = top pixel.
// PROGMEM: 475 bytes in flash, 0 in RAM.
// ================================================================
static const uint8_t FONT5X7[95][5] PROGMEM = {
    {0x00,0x00,0x00,0x00,0x00}, // ' '
    {0x00,0x00,0x5F,0x00,0x00}, // '!'
    {0x00,0x07,0x00,0x07,0x00}, // '"'
    {0x14,0x7F,0x14,0x7F,0x14}, // '#'
    {0x24,0x2A,0x7F,0x2A,0x12}, // '$'
    {0x23,0x13,0x08,0x64,0x62}, // '%'
    {0x36,0x49,0x55,0x22,0x50}, // '&'
    {0x00,0x05,0x03,0x00,0x00}, // '''
    {0x00,0x1C,0x22,0x41,0x00}, // '('
    {0x00,0x41,0x22,0x1C,0x00}, // ')'
    {0x14,0x08,0x3E,0x08,0x14}, // '*'
    {0x08,0x08,0x3E,0x08,0x08}, // '+'
    {0x00,0x50,0x30,0x00,0x00}, // ','
    {0x08,0x08,0x08,0x08,0x08}, // '-'
    {0x00,0x60,0x60,0x00,0x00}, // '.'
    {0x20,0x10,0x08,0x04,0x02}, // '/'
    {0x3E,0x51,0x49,0x45,0x3E}, // '0'
    {0x00,0x42,0x7F,0x40,0x00}, // '1'
    {0x42,0x61,0x51,0x49,0x46}, // '2'
    {0x21,0x41,0x45,0x4B,0x31}, // '3'
    {0x18,0x14,0x12,0x7F,0x10}, // '4'
    {0x27,0x45,0x45,0x45,0x39}, // '5'
    {0x3C,0x4A,0x49,0x49,0x30}, // '6'
    {0x01,0x71,0x09,0x05,0x03}, // '7'
    {0x36,0x49,0x49,0x49,0x36}, // '8'
    {0x06,0x49,0x49,0x29,0x1E}, // '9'
    {0x00,0x36,0x36,0x00,0x00}, // ':'
    {0x00,0x56,0x36,0x00,0x00}, // ';'
    {0x08,0x14,0x22,0x41,0x00}, // '<'
    {0x14,0x14,0x14,0x14,0x14}, // '='
    {0x00,0x41,0x22,0x14,0x08}, // '>'
    {0x02,0x01,0x51,0x09,0x06}, // '?'
    {0x32,0x49,0x79,0x41,0x3E}, // '@'
    {0x7E,0x11,0x11,0x11,0x7E}, // 'A'
    {0x7F,0x49,0x49,0x49,0x36}, // 'B'
    {0x3E,0x41,0x41,0x41,0x22}, // 'C'
    {0x7F,0x41,0x41,0x22,0x1C}, // 'D'
    {0x7F,0x49,0x49,0x49,0x41}, // 'E'
    {0x7F,0x09,0x09,0x01,0x01}, // 'F'
    {0x3E,0x41,0x49,0x49,0x7A}, // 'G'
    {0x7F,0x08,0x08,0x08,0x7F}, // 'H'
    {0x00,0x41,0x7F,0x41,0x00}, // 'I'
    {0x20,0x40,0x41,0x3F,0x01}, // 'J'
    {0x7F,0x08,0x14,0x22,0x41}, // 'K'
    {0x7F,0x40,0x40,0x40,0x40}, // 'L'
    {0x7F,0x02,0x04,0x02,0x7F}, // 'M'
    {0x7F,0x04,0x08,0x10,0x7F}, // 'N'
    {0x3E,0x41,0x41,0x41,0x3E}, // 'O'
    {0x7F,0x09,0x09,0x09,0x06}, // 'P'
    {0x3E,0x41,0x51,0x21,0x5E}, // 'Q'
    {0x7F,0x09,0x19,0x29,0x46}, // 'R'
    {0x46,0x49,0x49,0x49,0x31}, // 'S'
    {0x01,0x01,0x7F,0x01,0x01}, // 'T'
    {0x3F,0x40,0x40,0x40,0x3F}, // 'U'
    {0x1F,0x20,0x40,0x20,0x1F}, // 'V'
    {0x3F,0x40,0x38,0x40,0x3F}, // 'W'
    {0x63,0x14,0x08,0x14,0x63}, // 'X'
    {0x07,0x08,0x70,0x08,0x07}, // 'Y'
    {0x61,0x51,0x49,0x45,0x43}, // 'Z'
    {0x00,0x7F,0x41,0x41,0x00}, // '['
    {0x02,0x04,0x08,0x10,0x20}, // '\'
    {0x00,0x41,0x41,0x7F,0x00}, // ']'
    {0x04,0x02,0x01,0x02,0x04}, // '^'
    {0x40,0x40,0x40,0x40,0x40}, // '_'
    {0x00,0x01,0x02,0x04,0x00}, // '`'
    {0x20,0x54,0x54,0x54,0x78}, // 'a'
    {0x7F,0x48,0x44,0x44,0x38}, // 'b'
    {0x38,0x44,0x44,0x44,0x20}, // 'c'
    {0x38,0x44,0x44,0x48,0x7F}, // 'd'
    {0x38,0x54,0x54,0x54,0x18}, // 'e'
    {0x08,0x7E,0x09,0x01,0x02}, // 'f'
    {0x0C,0x52,0x52,0x52,0x3E}, // 'g'
    {0x7F,0x08,0x04,0x04,0x78}, // 'h'
    {0x00,0x44,0x7D,0x40,0x00}, // 'i'
    {0x20,0x40,0x44,0x3D,0x00}, // 'j'
    {0x7F,0x10,0x28,0x44,0x00}, // 'k'
    {0x00,0x41,0x7F,0x40,0x00}, // 'l'
    {0x7C,0x04,0x18,0x04,0x78}, // 'm'
    {0x7C,0x08,0x04,0x04,0x78}, // 'n'
    {0x38,0x44,0x44,0x44,0x38}, // 'o'
    {0x7C,0x14,0x14,0x14,0x08}, // 'p'
    {0x08,0x14,0x14,0x18,0x7C}, // 'q'
    {0x7C,0x08,0x04,0x04,0x08}, // 'r'
    {0x48,0x54,0x54,0x54,0x20}, // 's'
    {0x04,0x3F,0x44,0x40,0x20}, // 't'
    {0x3C,0x40,0x40,0x40,0x7C}, // 'u'
    {0x1C,0x20,0x40,0x20,0x1C}, // 'v'
    {0x3C,0x40,0x20,0x40,0x3C}, // 'w'
    {0x44,0x28,0x10,0x28,0x44}, // 'x'
    {0x0C,0x50,0x50,0x50,0x3C}, // 'y'
    {0x44,0x64,0x54,0x4C,0x44}, // 'z'
    {0x00,0x08,0x36,0x41,0x00}, // '{'
    {0x00,0x00,0x7F,0x00,0x00}, // '|'
    {0x00,0x41,0x36,0x08,0x00}, // '}'
    {0x10,0x08,0x08,0x10,0x08}, // '~'
};

uint8_t SSD1306::_fontByte(char c, uint8_t col) {
    if (c < 0x20 || c > 0x7E) c = '?';
    return pgm_read_byte(&FONT5X7[static_cast<uint8_t>(c) - 0x20][col]);
}

// ================================================================
// I2C command helpers
// ================================================================

void SSD1306::_cmd(uint8_t c) {
    uint8_t buf[2] = { 0x00, c };
    _i2c.write(_addr, buf, 2);
}

void SSD1306::_cmd2(uint8_t c, uint8_t a) {
    uint8_t buf[3] = { 0x00, c, a };
    _i2c.write(_addr, buf, 3);
}

void SSD1306::_cmd3(uint8_t c, uint8_t a0, uint8_t a1) {
    uint8_t buf[4] = { 0x00, c, a0, a1 };
    _i2c.write(_addr, buf, 4);
}

// ================================================================
// Initialisation
// ================================================================

void SSD1306::begin() {
    _cmd(0xAE);          // display off
    _cmd2(0xD5, 0x80);   // clock div / OSC freq
    _cmd2(0xA8, 0x3F);   // multiplex ratio (63 for 64-px tall)
    _cmd2(0xD3, 0x00);   // display offset = 0
    _cmd(0x40);          // start line = 0
    _cmd2(0x8D, 0x14);   // charge pump enable
    _cmd2(0x20, 0x00);   // horizontal addressing mode
    _cmd(0xA1);          // segment remap (col 127 → SEG0)
    _cmd(0xC8);          // COM scan direction remapped (top → bottom)
    _cmd2(0xDA, 0x12);   // COM pins config: alternative, no left-right remap
    _cmd2(0x81, 0xCF);   // contrast
    _cmd2(0xD9, 0xF1);   // pre-charge period
    _cmd2(0xDB, 0x40);   // VCOMH deselect level
    _cmd(0xA4);          // pixels from RAM (not all-on)
    _cmd(0xA6);          // normal display (not inverted)
    _cmd(0xAF);          // display on
    clear();
    display();
}

// ================================================================
// Framebuffer → display
// ================================================================

void SSD1306::display() {
    _cmd3(0x21, 0, WIDTH - 1);    // column address: 0–127
    _cmd3(0x22, 0, PAGES - 1);    // page address:   0–7
    _i2c.beginTransmit(_addr);
    _i2c.transmit(0x40);          // data stream
    for (uint16_t i = 0; i < static_cast<uint16_t>(WIDTH) * PAGES; ++i)
        _i2c.transmit(_buf[i]);
    _i2c.endTransmit();
}

// ================================================================
// Display control
// ================================================================

void SSD1306::on(bool enable) {
    _cmd(enable ? 0xAF : 0xAE);
}

void SSD1306::invert(bool inv) {
    _cmd(inv ? 0xA7 : 0xA6);
}

void SSD1306::contrast(uint8_t level) {
    _cmd2(0x81, level);
}

void SSD1306::flip(bool flipH, bool flipV) {
    _cmd(flipH ? 0xA0 : 0xA1);  // segment remap
    _cmd(flipV ? 0xC0 : 0xC8);  // COM scan direction
}

// ================================================================
// Pixel operations
// ================================================================

void SSD1306::drawPixel(uint8_t x, uint8_t y, bool on) {
    if (x >= WIDTH || y >= HEIGHT) return;
    uint8_t& b = _buf[static_cast<uint16_t>(y / 8) * WIDTH + x];
    if (on) b |=  static_cast<uint8_t>(1 << (y & 7));
    else    b &= ~static_cast<uint8_t>(1 << (y & 7));
}

bool SSD1306::getPixel(uint8_t x, uint8_t y) const {
    if (x >= WIDTH || y >= HEIGHT) return false;
    return (_buf[static_cast<uint16_t>(y / 8) * WIDTH + x] >> (y & 7)) & 1;
}

// ================================================================
// Primitives
// ================================================================

void SSD1306::drawHLine(uint8_t x, uint8_t y, uint8_t w, bool on) {
    for (uint8_t i = 0; i < w && (x + i) < WIDTH; ++i)
        drawPixel(x + i, y, on);
}

void SSD1306::drawVLine(uint8_t x, uint8_t y, uint8_t h, bool on) {
    for (uint8_t i = 0; i < h && (y + i) < HEIGHT; ++i)
        drawPixel(x, y + i, on);
}

void SSD1306::drawLine(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, bool on) {
    int16_t dx =  static_cast<int16_t>(x1) - x0;
    int16_t dy =  static_cast<int16_t>(y1) - y0;
    int16_t ax = (dx < 0) ? -dx : dx;
    int16_t ay = (dy < 0) ? -dy : dy;
    int8_t  sx = (x0 < x1) ? 1 : -1;
    int8_t  sy = (y0 < y1) ? 1 : -1;
    int16_t err = ax - ay;

    while (true) {
        drawPixel(x0, y0, on);
        if (x0 == x1 && y0 == y1) break;
        int16_t e2 = err * 2;
        if (e2 > -ay) { err -= ay; x0 = static_cast<uint8_t>(x0 + sx); }
        if (e2 <  ax) { err += ax; y0 = static_cast<uint8_t>(y0 + sy); }
    }
}

void SSD1306::drawRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, bool on) {
    drawHLine(x,         y,         w, on);
    drawHLine(x,         y + h - 1, w, on);
    drawVLine(x,         y,         h, on);
    drawVLine(x + w - 1, y,         h, on);
}

void SSD1306::fillRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, bool on) {
    for (uint8_t row = 0; row < h && (y + row) < HEIGHT; ++row)
        drawHLine(x, y + row, w, on);
}

void SSD1306::drawCircle(uint8_t cx, uint8_t cy, uint8_t r, bool on) {
    int16_t x = 0, y = r, d = 1 - static_cast<int16_t>(r);
    while (y >= x) {
        drawPixel(cx + x, cy + y, on);
        drawPixel(cx - x, cy + y, on);
        drawPixel(cx + x, cy - y, on);
        drawPixel(cx - x, cy - y, on);
        drawPixel(cx + y, cy + x, on);
        drawPixel(cx - y, cy + x, on);
        drawPixel(cx + y, cy - x, on);
        drawPixel(cx - y, cy - x, on);
        if (d < 0) { d += 2 * x + 3; }
        else       { d += 2 * (x - y) + 5; --y; }
        ++x;
    }
}

void SSD1306::fillCircle(uint8_t cx, uint8_t cy, uint8_t r, bool on) {
    int16_t x = 0, y = r, d = 1 - static_cast<int16_t>(r);
    while (y >= x) {
        drawHLine(static_cast<uint8_t>(cx - y), static_cast<uint8_t>(cy + x),
                  static_cast<uint8_t>(2 * y + 1), on);
        drawHLine(static_cast<uint8_t>(cx - y), static_cast<uint8_t>(cy - x),
                  static_cast<uint8_t>(2 * y + 1), on);
        if (x != y) {
            drawHLine(static_cast<uint8_t>(cx - x), static_cast<uint8_t>(cy + y),
                      static_cast<uint8_t>(2 * x + 1), on);
            drawHLine(static_cast<uint8_t>(cx - x), static_cast<uint8_t>(cy - y),
                      static_cast<uint8_t>(2 * x + 1), on);
        }
        if (d < 0) { d += 2 * x + 3; }
        else       { d += 2 * (x - y) + 5; --y; }
        ++x;
    }
}

// ================================================================
// Bitmap (native SSD1306 page-column format, from PROGMEM)
// ================================================================

void SSD1306::drawBitmap(uint8_t x, uint8_t y, uint8_t w, uint8_t h,
                         const uint8_t* pgmData, bool on) {
    uint8_t pages = h / 8;
    uint8_t startPage = y / 8;
    for (uint8_t p = 0; p < pages; ++p) {
        uint8_t pg = startPage + p;
        if (pg >= PAGES) break;
        for (uint8_t col = 0; col < w; ++col) {
            uint8_t bx = x + col;
            if (bx >= WIDTH) break;
            uint8_t byte = pgm_read_byte(pgmData + static_cast<uint16_t>(p) * w + col);
            if (on) _buf[static_cast<uint16_t>(pg) * WIDTH + bx] |=  byte;
            else    _buf[static_cast<uint16_t>(pg) * WIDTH + bx] &= ~byte;
        }
    }
}

// ================================================================
// Text
// ================================================================

void SSD1306::setCursor(uint8_t x, uint8_t y) {
    _curX = x;
    _curY = y;
}

void SSD1306::print(char c) {
    if (c == '\n') {
        _curX  = 0;
        _curY += 8;
        return;
    }
    if (_curX + 6 > WIDTH) {   // wrap
        _curX  = 0;
        _curY += 8;
    }
    if (_curY + 8 > HEIGHT) return;

    uint8_t page = _curY / 8;
    for (uint8_t col = 0; col < 5; ++col) {
        _buf[static_cast<uint16_t>(page) * WIDTH + _curX + col] = _fontByte(c, col);
    }
    _buf[static_cast<uint16_t>(page) * WIDTH + _curX + 5] = 0x00; // 1-px gap
    _curX += 6;
}

void SSD1306::print(const char* str) {
    while (*str) print(*str++);
}

void SSD1306::print_P(const char* pgmStr) {
    char c;
    while ((c = static_cast<char>(pgm_read_byte(pgmStr++)))) print(c);
}

void SSD1306::print(int32_t value) {
    if (value < 0) { print('-'); value = -value; }
    printU(static_cast<uint32_t>(value), 10);
}

void SSD1306::printU(uint32_t value, uint8_t base) {
    char buf[11];
    uint8_t i = 0;
    if (value == 0) { print('0'); return; }
    while (value && i < sizeof(buf)) {
        uint8_t d = static_cast<uint8_t>(value % base);
        buf[i++] = static_cast<char>(d < 10 ? '0' + d : 'A' + d - 10);
        value /= base;
    }
    for (int8_t j = static_cast<int8_t>(i) - 1; j >= 0; --j)
        print(buf[j]);
}

// ================================================================
// Hardware scrolling
// ================================================================

void SSD1306::scrollRight(uint8_t startPage, uint8_t endPage) {
    stopScroll();
    uint8_t cmd[] = { 0x00, 0x26, 0x00, startPage, 0x00, endPage, 0x00, 0xFF };
    _i2c.write(_addr, cmd, sizeof(cmd));
    _cmd(0x2F);
}

void SSD1306::scrollLeft(uint8_t startPage, uint8_t endPage) {
    stopScroll();
    uint8_t cmd[] = { 0x00, 0x27, 0x00, startPage, 0x00, endPage, 0x00, 0xFF };
    _i2c.write(_addr, cmd, sizeof(cmd));
    _cmd(0x2F);
}

void SSD1306::scrollDiagRight(uint8_t startPage, uint8_t endPage) {
    stopScroll();
    _cmd3(0xA3, 0x00, 0x40);  // Set Vertical Scroll Area: 0 fixed rows, 64 scroll rows
    uint8_t cmd[] = { 0x00, 0x29, 0x00, startPage, 0x00, endPage, 0x01 };
    _i2c.write(_addr, cmd, sizeof(cmd));
    _cmd(0x2F);
}

void SSD1306::scrollDiagLeft(uint8_t startPage, uint8_t endPage) {
    stopScroll();
    _cmd3(0xA3, 0x00, 0x40);  // Set Vertical Scroll Area: 0 fixed rows, 64 scroll rows
    uint8_t cmd[] = { 0x00, 0x2A, 0x00, startPage, 0x00, endPage, 0x01 };
    _i2c.write(_addr, cmd, sizeof(cmd));
    _cmd(0x2F);
}

void SSD1306::stopScroll() {
    _cmd(0x2E);
}

} // namespace MikroDuino
