#include "../include/ST7735.hpp"
#include <util/delay.h>

namespace MikroDuino {

// ================================================================
// 5x7 ASCII font — 95 characters (0x20 space ... 0x7E tilde)
// Each character: 5 bytes (columns left->right), bit 0 = top pixel.
// Same table as the SSD1306 module's font (kept as a separate copy here
// rather than a shared header, matching how every other module in this
// SDK owns its own font data rather than depending on another module).
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

uint8_t ST7735::_fontByte(char c, uint8_t col) {
    if (c < 0x20 || c > 0x7E) c = '?';
    return pgm_read_byte(&FONT5X7[static_cast<uint8_t>(c) - 0x20][col]);
}

// ================================================================
// Low-level bus helpers
// ================================================================

void ST7735::_delayMs(uint16_t ms) {
    while (ms--) _delay_ms(1);
}

void ST7735::_hwReset() {
    if (_rst == NO_PIN) return;
    GPIO::set(_rst);
    _delayMs(10);
    GPIO::clear(_rst);
    _delayMs(10);
    GPIO::set(_rst);
    _delayMs(150);
}

void ST7735::_cmd(uint8_t c) {
    GPIO::clear(_cs);
    GPIO::clear(_dc);
    _spi.transfer(c);
    GPIO::set(_cs);
}

void ST7735::_cmdArgs(uint8_t c, const uint8_t* args, uint8_t n) {
    GPIO::clear(_cs);
    GPIO::clear(_dc);
    _spi.transfer(c);
    if (n) {
        GPIO::set(_dc);
        for (uint8_t i = 0; i < n; ++i) _spi.transfer(args[i]);
    }
    GPIO::set(_cs);
}

// ================================================================
// Initialisation
// ================================================================

void ST7735::begin(Variant variant, Rotation rotation, bool bgr) {
    _bgr = bgr;

    GPIO::output(_cs);
    GPIO::output(_dc);
    GPIO::set(_cs);
    if (_rst != NO_PIN) GPIO::output(_rst);
    if (_bl  != NO_PIN) { GPIO::output(_bl); GPIO::set(_bl); }

    switch (variant) {
        case Variant::BlackTab:    _colstart = 0;  _rowstart = 0; _panelW = 128; _panelH = 160; break;
        case Variant::RedTab:      _colstart = 0;  _rowstart = 0; _panelW = 128; _panelH = 160; break;
        case Variant::GreenTab:    _colstart = 2;  _rowstart = 1; _panelW = 128; _panelH = 160; break;
        case Variant::GreenTab128: _colstart = 2;  _rowstart = 3; _panelW = 128; _panelH = 128; break;
        case Variant::Mini160x80:  _colstart = 26; _rowstart = 1; _panelW = 160; _panelH = 80;  break;
    }

    _hwReset();

    _cmd(0x01);            // SWRESET — redundant after a hardware reset, cheap
                            // insurance when rstPin is NO_PIN and this IS the reset.
    _delayMs(150);
    _cmd(0x11);             // SLPOUT
    _delayMs(255);

    { const uint8_t a[] = {0x01, 0x2C, 0x2D};                         _cmdArgs(0xB1, a, 3); } // FRMCTR1 (normal mode)
    { const uint8_t a[] = {0x01, 0x2C, 0x2D};                         _cmdArgs(0xB2, a, 3); } // FRMCTR2 (idle mode)
    { const uint8_t a[] = {0x01, 0x2C, 0x2D, 0x01, 0x2C, 0x2D};       _cmdArgs(0xB3, a, 6); } // FRMCTR3 (partial mode)
    { const uint8_t a[] = {0x07};                                     _cmdArgs(0xB4, a, 1); } // INVCTR — line inversion
    { const uint8_t a[] = {0xA2, 0x02, 0x84};                         _cmdArgs(0xC0, a, 3); } // PWCTR1
    { const uint8_t a[] = {0xC5};                                     _cmdArgs(0xC1, a, 1); } // PWCTR2
    { const uint8_t a[] = {0x0A, 0x00};                               _cmdArgs(0xC2, a, 2); } // PWCTR3
    { const uint8_t a[] = {0x8A, 0x2A};                               _cmdArgs(0xC3, a, 2); } // PWCTR4
    { const uint8_t a[] = {0x8A, 0xEE};                               _cmdArgs(0xC4, a, 2); } // PWCTR5
    { const uint8_t a[] = {0x0E};                                     _cmdArgs(0xC5, a, 1); } // VMCTR1

    _cmd(0x20);              // INVOFF — normal (non-inverted) pixel polarity

    setRotation(rotation);   // sends MADCTL

    { const uint8_t a[] = {0x05};                                     _cmdArgs(0x3A, a, 1); } // COLMOD: 16 bpp (RGB565)

    { const uint8_t a[] = {0x02,0x1C,0x07,0x12,0x37,0x32,0x29,0x2D,
                            0x29,0x25,0x2B,0x39,0x00,0x01,0x03,0x10};  _cmdArgs(0xE0, a, 16); } // GMCTRP1 (positive gamma)
    { const uint8_t a[] = {0x03,0x1D,0x07,0x06,0x2E,0x2C,0x29,0x2D,
                            0x2E,0x2E,0x37,0x3F,0x00,0x00,0x02,0x10};  _cmdArgs(0xE1, a, 16); } // GMCTRN1 (negative gamma)

    _cmd(0x13);              // NORON — normal display mode on
    _delayMs(10);
    _cmd(0x29);              // DISPON
    _delayMs(100);

    fillScreen(BLACK);
}

void ST7735::setRotation(Rotation rotation) {
    _rotation = rotation;

    static constexpr uint8_t MADCTL_MY  = 0x80;
    static constexpr uint8_t MADCTL_MX  = 0x40;
    static constexpr uint8_t MADCTL_MV  = 0x20;
    static constexpr uint8_t MADCTL_BGR = 0x08;

    uint8_t madctl = 0;
    switch (rotation) {
        case Rotation::Deg0:
            madctl = MADCTL_MX | MADCTL_MY;
            _width = _panelW; _height = _panelH;
            break;
        case Rotation::Deg90:
            madctl = MADCTL_MY | MADCTL_MV;
            _width = _panelH; _height = _panelW;
            break;
        case Rotation::Deg180:
            madctl = 0;
            _width = _panelW; _height = _panelH;
            break;
        case Rotation::Deg270:
            madctl = MADCTL_MX | MADCTL_MV;
            _width = _panelH; _height = _panelW;
            break;
    }
    if (_bgr) madctl |= MADCTL_BGR;

    const uint8_t a[] = { madctl };
    _cmdArgs(0x36, a, 1);
}

// ================================================================
// Display control
// ================================================================

void ST7735::backlight(bool on) {
    if (_bl == NO_PIN) return;
    GPIO::write(_bl, on);
}

void ST7735::enableDisplay(bool on) {
    _cmd(on ? 0x29 : 0x28);   // DISPON / DISPOFF
}

void ST7735::sleep(bool enable) {
    _cmd(enable ? 0x10 : 0x11);   // SLPIN / SLPOUT
    _delayMs(enable ? 5 : 120);   // SLPOUT needs >= 120 ms before further commands
}

void ST7735::invertDisplay(bool inv) {
    _cmd(inv ? 0x21 : 0x20);   // INVON / INVOFF
}

// ================================================================
// Low-level streaming API
// ================================================================

void ST7735::startWrite(int16_t x, int16_t y, int16_t w, int16_t h) {
    uint16_t x0 = static_cast<uint16_t>(x) + _colstart;
    uint16_t x1 = static_cast<uint16_t>(x + w - 1) + _colstart;
    uint16_t y0 = static_cast<uint16_t>(y) + _rowstart;
    uint16_t y1 = static_cast<uint16_t>(y + h - 1) + _rowstart;

    const uint8_t colArgs[4] = {
        static_cast<uint8_t>(x0 >> 8), static_cast<uint8_t>(x0 & 0xFFu),
        static_cast<uint8_t>(x1 >> 8), static_cast<uint8_t>(x1 & 0xFFu)
    };
    _cmdArgs(0x2A, colArgs, 4);   // CASET

    const uint8_t rowArgs[4] = {
        static_cast<uint8_t>(y0 >> 8), static_cast<uint8_t>(y0 & 0xFFu),
        static_cast<uint8_t>(y1 >> 8), static_cast<uint8_t>(y1 & 0xFFu)
    };
    _cmdArgs(0x2B, rowArgs, 4);   // RASET

    // RAMWR — deliberately does NOT raise CS afterwards: the caller streams
    // exactly w*h pixels via pushColor()/pushColors() and then calls
    // endWrite() to close the transaction.
    GPIO::clear(_cs);
    GPIO::clear(_dc);
    _spi.transfer(0x2C);
    GPIO::set(_dc);
}

void ST7735::pushColor(uint16_t color) {
    _spi.transfer(static_cast<uint8_t>(color >> 8));
    _spi.transfer(static_cast<uint8_t>(color & 0xFFu));
}

void ST7735::pushColors(const uint16_t* colors, uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) pushColor(colors[i]);
}

void ST7735::pushColors_P(const uint16_t* pgmColors, uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) pushColor(pgm_read_word(&pgmColors[i]));
}

void ST7735::endWrite() {
    GPIO::set(_cs);
}

// ================================================================
// Clipping
// ================================================================

bool ST7735::_clip(int16_t& x, int16_t& y, int16_t& w, int16_t& h) const {
    if (w <= 0 || h <= 0) return false;
    if (x < 0) { w = static_cast<int16_t>(w + x); x = 0; }
    if (y < 0) { h = static_cast<int16_t>(h + y); y = 0; }
    if (w <= 0 || h <= 0) return false;
    if (x >= static_cast<int16_t>(_width) || y >= static_cast<int16_t>(_height)) return false;
    if (static_cast<int16_t>(x + w) > static_cast<int16_t>(_width))  w = static_cast<int16_t>(_width  - x);
    if (static_cast<int16_t>(y + h) > static_cast<int16_t>(_height)) h = static_cast<int16_t>(_height - y);
    return (w > 0 && h > 0);
}

// ================================================================
// Pixel + fast axis-aligned primitives
// ================================================================

void ST7735::drawPixel(int16_t x, int16_t y, uint16_t color) {
    if (x < 0 || y < 0 || x >= static_cast<int16_t>(_width) || y >= static_cast<int16_t>(_height)) return;
    startWrite(x, y, 1, 1);
    pushColor(color);
    endWrite();
}

void ST7735::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (!_clip(x, y, w, h)) return;
    startWrite(x, y, w, h);
    uint32_t n = static_cast<uint32_t>(w) * static_cast<uint32_t>(h);
    for (uint32_t i = 0; i < n; ++i) pushColor(color);
    endWrite();
}

void ST7735::drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) { fillRect(x, y, w, 1, color); }
void ST7735::drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) { fillRect(x, y, 1, h, color); }

void ST7735::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    drawFastHLine(x,         y,         w, color);
    drawFastHLine(x,         y + h - 1, w, color);
    drawFastVLine(x,         y,         h, color);
    drawFastVLine(x + w - 1, y,         h, color);
}

// ================================================================
// Per-pixel primitives
// ================================================================

void ST7735::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
    // Fast paths: axis-aligned lines are just 1px-thick filled spans.
    if (x0 == x1) {
        int16_t y = (y0 < y1) ? y0 : y1;
        int16_t h = static_cast<int16_t>(((y0 < y1) ? (y1 - y0) : (y0 - y1)) + 1);
        drawFastVLine(x0, y, h, color);
        return;
    }
    if (y0 == y1) {
        int16_t x = (x0 < x1) ? x0 : x1;
        int16_t w = static_cast<int16_t>(((x0 < x1) ? (x1 - x0) : (x0 - x1)) + 1);
        drawFastHLine(x, y0, w, color);
        return;
    }

    // Bresenham for the general diagonal case — one drawPixel() (and
    // therefore one address-window command) per point. See the header's
    // class-level note on why this is slower than the span-based
    // primitives above.
    int16_t dx = static_cast<int16_t>((x1 > x0) ? (x1 - x0) : (x0 - x1));
    int16_t dy = static_cast<int16_t>((y1 > y0) ? (y1 - y0) : (y0 - y1));
    int8_t  sx = (x0 < x1) ? 1 : -1;
    int8_t  sy = (y0 < y1) ? 1 : -1;
    int16_t err = static_cast<int16_t>(dx - dy);

    while (true) {
        drawPixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int16_t e2 = static_cast<int16_t>(err * 2);
        if (e2 > -dy) { err = static_cast<int16_t>(err - dy); x0 = static_cast<int16_t>(x0 + sx); }
        if (e2 <  dx) { err = static_cast<int16_t>(err + dx); y0 = static_cast<int16_t>(y0 + sy); }
    }
}

void ST7735::drawCircle(int16_t cx, int16_t cy, int16_t r, uint16_t color) {
    int16_t x = 0, y = r, d = static_cast<int16_t>(1 - r);
    while (y >= x) {
        drawPixel(cx + x, cy + y, color);
        drawPixel(cx - x, cy + y, color);
        drawPixel(cx + x, cy - y, color);
        drawPixel(cx - x, cy - y, color);
        drawPixel(cx + y, cy + x, color);
        drawPixel(cx - y, cy + x, color);
        drawPixel(cx + y, cy - x, color);
        drawPixel(cx - y, cy - x, color);
        if (d < 0) { d = static_cast<int16_t>(d + 2 * x + 3); }
        else       { d = static_cast<int16_t>(d + 2 * (x - y) + 5); --y; }
        ++x;
    }
}

void ST7735::fillCircle(int16_t cx, int16_t cy, int16_t r, uint16_t color) {
    int16_t x = 0, y = r, d = static_cast<int16_t>(1 - r);
    while (y >= x) {
        drawFastHLine(static_cast<int16_t>(cx - y), static_cast<int16_t>(cy + x),
                      static_cast<int16_t>(2 * y + 1), color);
        drawFastHLine(static_cast<int16_t>(cx - y), static_cast<int16_t>(cy - x),
                      static_cast<int16_t>(2 * y + 1), color);
        if (x != y) {
            drawFastHLine(static_cast<int16_t>(cx - x), static_cast<int16_t>(cy + y),
                          static_cast<int16_t>(2 * x + 1), color);
            drawFastHLine(static_cast<int16_t>(cx - x), static_cast<int16_t>(cy - y),
                          static_cast<int16_t>(2 * x + 1), color);
        }
        if (d < 0) { d = static_cast<int16_t>(d + 2 * x + 3); }
        else       { d = static_cast<int16_t>(d + 2 * (x - y) + 5); --y; }
        ++x;
    }
}

// cornerMask: bit0=top-right, bit1=top-left, bit2=bottom-left, bit3=bottom-right
void ST7735::_drawCircleHelper(int16_t cx, int16_t cy, int16_t r, uint8_t cornerMask, uint16_t color) {
    int16_t x = 0, y = r, d = static_cast<int16_t>(1 - r);
    while (y >= x) {
        if (cornerMask & 0x01u) { drawPixel(cx + x, cy - y, color); drawPixel(cx + y, cy - x, color); }
        if (cornerMask & 0x02u) { drawPixel(cx - x, cy - y, color); drawPixel(cx - y, cy - x, color); }
        if (cornerMask & 0x04u) { drawPixel(cx - x, cy + y, color); drawPixel(cx - y, cy + x, color); }
        if (cornerMask & 0x08u) { drawPixel(cx + x, cy + y, color); drawPixel(cx + y, cy + x, color); }
        if (d < 0) { d = static_cast<int16_t>(d + 2 * x + 3); }
        else       { d = static_cast<int16_t>(d + 2 * (x - y) + 5); --y; }
        ++x;
    }
}

// Same cornerMask convention as _drawCircleHelper, but fills a full
// quadrant with vertical spans instead of plotting single points — each
// span runs from the circle's boundary at that column in to the corner's
// own center row/column, so it never draws outside the quadrant.
void ST7735::_fillCircleHelper(int16_t cx, int16_t cy, int16_t r, uint8_t cornerMask, uint16_t color) {
    int16_t x = 0, y = r, d = static_cast<int16_t>(1 - r);
    while (y >= x) {
        if (cornerMask & 0x01u) {   // top-right: x>=cx, y<=cy
            drawFastVLine(static_cast<int16_t>(cx + x), static_cast<int16_t>(cy - y), static_cast<int16_t>(y + 1), color);
            drawFastVLine(static_cast<int16_t>(cx + y), static_cast<int16_t>(cy - x), static_cast<int16_t>(x + 1), color);
        }
        if (cornerMask & 0x02u) {   // top-left: x<=cx, y<=cy
            drawFastVLine(static_cast<int16_t>(cx - x), static_cast<int16_t>(cy - y), static_cast<int16_t>(y + 1), color);
            drawFastVLine(static_cast<int16_t>(cx - y), static_cast<int16_t>(cy - x), static_cast<int16_t>(x + 1), color);
        }
        if (cornerMask & 0x04u) {   // bottom-left: x<=cx, y>=cy
            drawFastVLine(static_cast<int16_t>(cx - x), cy, static_cast<int16_t>(y + 1), color);
            drawFastVLine(static_cast<int16_t>(cx - y), cy, static_cast<int16_t>(x + 1), color);
        }
        if (cornerMask & 0x08u) {   // bottom-right: x>=cx, y>=cy
            drawFastVLine(static_cast<int16_t>(cx + x), cy, static_cast<int16_t>(y + 1), color);
            drawFastVLine(static_cast<int16_t>(cx + y), cy, static_cast<int16_t>(x + 1), color);
        }
        if (d < 0) { d = static_cast<int16_t>(d + 2 * x + 3); }
        else       { d = static_cast<int16_t>(d + 2 * (x - y) + 5); --y; }
        ++x;
    }
}

void ST7735::drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color) {
    if (r < 0) r = 0;
    if (r > w / 2) r = static_cast<int16_t>(w / 2);
    if (r > h / 2) r = static_cast<int16_t>(h / 2);

    drawFastHLine(static_cast<int16_t>(x + r), y,                             static_cast<int16_t>(w - 2 * r), color);
    drawFastHLine(static_cast<int16_t>(x + r), static_cast<int16_t>(y + h - 1), static_cast<int16_t>(w - 2 * r), color);
    drawFastVLine(x,                           static_cast<int16_t>(y + r),    static_cast<int16_t>(h - 2 * r), color);
    drawFastVLine(static_cast<int16_t>(x + w - 1), static_cast<int16_t>(y + r), static_cast<int16_t>(h - 2 * r), color);

    _drawCircleHelper(static_cast<int16_t>(x + r),         static_cast<int16_t>(y + r),         r, 0x02u, color); // top-left
    _drawCircleHelper(static_cast<int16_t>(x + w - 1 - r), static_cast<int16_t>(y + r),         r, 0x01u, color); // top-right
    _drawCircleHelper(static_cast<int16_t>(x + r),         static_cast<int16_t>(y + h - 1 - r), r, 0x04u, color); // bottom-left
    _drawCircleHelper(static_cast<int16_t>(x + w - 1 - r), static_cast<int16_t>(y + h - 1 - r), r, 0x08u, color); // bottom-right
}

void ST7735::fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color) {
    if (r < 0) r = 0;
    if (r > w / 2) r = static_cast<int16_t>(w / 2);
    if (r > h / 2) r = static_cast<int16_t>(h / 2);

    fillRect(static_cast<int16_t>(x + r), y, static_cast<int16_t>(w - 2 * r), h, color);                       // center band, full height
    fillRect(x,                           static_cast<int16_t>(y + r), r, static_cast<int16_t>(h - 2 * r), color); // left band
    fillRect(static_cast<int16_t>(x + w - r), static_cast<int16_t>(y + r), r, static_cast<int16_t>(h - 2 * r), color); // right band

    _fillCircleHelper(static_cast<int16_t>(x + r),         static_cast<int16_t>(y + r),         r, 0x02u, color); // top-left
    _fillCircleHelper(static_cast<int16_t>(x + w - 1 - r), static_cast<int16_t>(y + r),         r, 0x01u, color); // top-right
    _fillCircleHelper(static_cast<int16_t>(x + r),         static_cast<int16_t>(y + h - 1 - r), r, 0x04u, color); // bottom-left
    _fillCircleHelper(static_cast<int16_t>(x + w - 1 - r), static_cast<int16_t>(y + h - 1 - r), r, 0x08u, color); // bottom-right
}

void ST7735::drawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                          int16_t x2, int16_t y2, uint16_t color) {
    drawLine(x0, y0, x1, y1, color);
    drawLine(x1, y1, x2, y2, color);
    drawLine(x2, y2, x0, y0, color);
}

void ST7735::fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                          int16_t x2, int16_t y2, uint16_t color) {
    // Sort vertices so y0 <= y1 <= y2.
    if (y0 > y1) { int16_t t; t=y0;y0=y1;y1=static_cast<int16_t>(t); t=x0;x0=x1;x1=static_cast<int16_t>(t); }
    if (y1 > y2) { int16_t t; t=y1;y1=y2;y2=static_cast<int16_t>(t); t=x1;x1=x2;x2=static_cast<int16_t>(t); }
    if (y0 > y1) { int16_t t; t=y0;y0=y1;y1=static_cast<int16_t>(t); t=x0;x0=x1;x1=static_cast<int16_t>(t); }

    if (y0 == y2) {   // degenerate: all three vertices on one scanline
        int16_t minX = x0, maxX = x0;
        if (x1 < minX) minX = x1; if (x1 > maxX) maxX = x1;
        if (x2 < minX) minX = x2; if (x2 > maxX) maxX = x2;
        drawFastHLine(minX, y0, static_cast<int16_t>(maxX - minX + 1), color);
        return;
    }

    int16_t dx02 = static_cast<int16_t>(x2 - x0), dy02 = static_cast<int16_t>(y2 - y0);
    int16_t dx01 = static_cast<int16_t>(x1 - x0), dy01 = static_cast<int16_t>(y1 - y0);
    int16_t dx12 = static_cast<int16_t>(x2 - x1), dy12 = static_cast<int16_t>(y2 - y1);

    for (int16_t y = y0; y <= y2; ++y) {
        bool secondHalf = (y >= y1);
        int16_t xa = static_cast<int16_t>(x0 + static_cast<int32_t>(dx02) * (y - y0) / dy02);
        int16_t xb;
        if (!secondHalf) {
            xb = (dy01 == 0) ? x1
                              : static_cast<int16_t>(x0 + static_cast<int32_t>(dx01) * (y - y0) / dy01);
        } else {
            xb = (dy12 == 0) ? x2
                              : static_cast<int16_t>(x1 + static_cast<int32_t>(dx12) * (y - y1) / dy12);
        }
        if (xa > xb) { int16_t t = xa; xa = xb; xb = t; }
        drawFastHLine(xa, y, static_cast<int16_t>(xb - xa + 1), color);
    }
}

// ================================================================
// Bitmaps (PROGMEM)
// ================================================================

void ST7735::drawBitmap565(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t* pgmData) {
    // Not clipped: a straight PROGMEM pixel dump assumes the caller placed
    // it fully on-screen (partial clipping would require skipping source
    // pixels mid-row, which this simple straight-through streamer doesn't do).
    if (x < 0 || y < 0 ||
        x + w > static_cast<int16_t>(_width) || y + h > static_cast<int16_t>(_height)) return;

    startWrite(x, y, w, h);
    uint32_t n = static_cast<uint32_t>(w) * static_cast<uint32_t>(h);
    for (uint32_t i = 0; i < n; ++i) pushColor(pgm_read_word(&pgmData[i]));
    endWrite();
}

void ST7735::drawBitmap1(int16_t x, int16_t y, int16_t w, int16_t h, const uint8_t* pgmData,
                         uint16_t fgColor, uint16_t bgColor, bool opaque) {
    uint8_t bytesPerRow = static_cast<uint8_t>((w + 7) / 8);

    if (opaque) {
        if (x < 0 || y < 0 ||
            x + w > static_cast<int16_t>(_width) || y + h > static_cast<int16_t>(_height)) return;

        startWrite(x, y, w, h);
        for (int16_t row = 0; row < h; ++row) {
            for (int16_t col = 0; col < w; ++col) {
                uint8_t byte = pgm_read_byte(pgmData + static_cast<uint16_t>(row) * bytesPerRow + col / 8);
                bool set = static_cast<bool>((byte >> (7 - (col & 7))) & 1u);
                pushColor(set ? fgColor : bgColor);
            }
        }
        endWrite();
    } else {
        for (int16_t row = 0; row < h; ++row) {
            for (int16_t col = 0; col < w; ++col) {
                uint8_t byte = pgm_read_byte(pgmData + static_cast<uint16_t>(row) * bytesPerRow + col / 8);
                bool set = static_cast<bool>((byte >> (7 - (col & 7))) & 1u);
                if (set) drawPixel(static_cast<int16_t>(x + col), static_cast<int16_t>(y + row), fgColor);
            }
        }
    }
}

// ================================================================
// Text
// ================================================================

void ST7735::print(char c) {
    uint8_t cellW = static_cast<uint8_t>(6u * _textSize);
    uint8_t cellH = static_cast<uint8_t>(8u * _textSize);

    if (c == '\n') {
        _cursorX = 0;
        _cursorY = static_cast<int16_t>(_cursorY + cellH);
        return;
    }
    if (_cursorX + cellW > static_cast<int16_t>(_width)) {
        _cursorX = 0;
        _cursorY = static_cast<int16_t>(_cursorY + cellH);
    }
    if (_cursorY + cellH > static_cast<int16_t>(_height)) return;   // below bottom edge — drop silently

    if (!_transparentBg) {
        // Opaque: the whole glyph cell is written in one continuous burst.
        startWrite(_cursorX, _cursorY, cellW, cellH);
        for (uint8_t row = 0; row < cellH; ++row) {
            uint8_t fontRow = static_cast<uint8_t>(row / _textSize);
            for (uint8_t col = 0; col < cellW; ++col) {
                uint8_t fontCol = static_cast<uint8_t>(col / _textSize);
                bool set = false;
                if (fontCol < 5) {
                    set = static_cast<bool>((_fontByte(c, fontCol) >> fontRow) & 1u);
                }
                pushColor(set ? _fg : _bg);
            }
        }
        endWrite();
    } else {
        // Transparent: only foreground pixels are written, so a single
        // contiguous window can't be used — each set bit (or, for
        // textSize > 1, each scaled block) opens its own small write.
        for (uint8_t fontCol = 0; fontCol < 5; ++fontCol) {
            uint8_t colByte = _fontByte(c, fontCol);
            for (uint8_t fontRow = 0; fontRow < 7; ++fontRow) {
                if ((colByte >> fontRow) & 1u) {
                    if (_textSize == 1) {
                        drawPixel(static_cast<int16_t>(_cursorX + fontCol),
                                  static_cast<int16_t>(_cursorY + fontRow), _fg);
                    } else {
                        fillRect(static_cast<int16_t>(_cursorX + fontCol * _textSize),
                                 static_cast<int16_t>(_cursorY + fontRow * _textSize),
                                 _textSize, _textSize, _fg);
                    }
                }
            }
        }
    }

    _cursorX = static_cast<int16_t>(_cursorX + cellW);
}

void ST7735::print(const char* str) {
    while (*str) print(*str++);
}

void ST7735::print_P(const char* pgmStr) {
    char c;
    while ((c = static_cast<char>(pgm_read_byte(pgmStr++)))) print(c);
}

void ST7735::print(int32_t value) {
    if (value < 0) { print('-'); value = -value; }
    printU(static_cast<uint32_t>(value), 10);
}

void ST7735::printU(uint32_t value, uint8_t base) {
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

} // namespace MikroDuino
