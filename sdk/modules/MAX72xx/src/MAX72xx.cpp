#include "../include/MAX72xx.hpp"
#include <avr/pgmspace.h>
#include <util/delay.h>

namespace MikroDuino {

// ── Constructors ──────────────────────────────────────────────────────────────

MatrixDisplay::MatrixDisplay(ModuleType mod, uint8_t csPin, uint8_t numDevices)
    : _mx(mod, static_cast<int8_t>(csPin), numDevices), _scroll{} {}

MatrixDisplay::MatrixDisplay(ModuleType mod, uint8_t dataPin, uint8_t clkPin,
                             uint8_t csPin, uint8_t numDevices)
    : _mx(mod, static_cast<int8_t>(dataPin), static_cast<int8_t>(clkPin),
          static_cast<int8_t>(csPin), numDevices), _scroll{} {}

// ── Display control ───────────────────────────────────────────────────────────

bool MatrixDisplay::begin() { return _mx.begin(); }
void MatrixDisplay::clear() { _mx.clear(); }

void MatrixDisplay::setBrightness(uint8_t level) {
    _mx.control(MD_MAX72XX::INTENSITY, level & 0x0F);
}

void MatrixDisplay::setEnabled(bool on) {
    // SHUTDOWN ON = display off; SHUTDOWN OFF = display on
    _mx.control(MD_MAX72XX::SHUTDOWN, on ? MD_MAX72XX::OFF : MD_MAX72XX::ON);
}

// ── Pixel primitives ─────────────────────────────────────────────────────────

void MatrixDisplay::setPixel(uint8_t row, uint16_t col, bool state) {
    _mx.setPoint(row, col, state);
}

bool MatrixDisplay::getPixel(uint8_t row, uint16_t col) {
    return _mx.getPoint(row, col);
}

void MatrixDisplay::fill(bool state) {
    uint8_t v = state ? 0xFF : 0x00;
    for (uint8_t r = 0; r < 8; r++)
        _mx.setRow(r, v);
}

void MatrixDisplay::drawHLine(uint8_t row, uint16_t startCol, uint16_t endCol, bool state) {
    for (uint16_t c = startCol; c <= endCol; c++)
        _mx.setPoint(row, c, state);
}

void MatrixDisplay::drawVLine(uint16_t col, uint8_t startRow, uint8_t endRow, bool state) {
    for (uint8_t r = startRow; r <= endRow; r++)
        _mx.setPoint(r, col, state);
}

void MatrixDisplay::drawRect(uint8_t row, uint16_t col,
                             uint8_t height, uint8_t width, bool state) {
    if (width == 0 || height == 0) return;
    drawHLine(row,              col, col + width - 1, state);
    drawHLine(row + height - 1, col, col + width - 1, state);
    if (height > 2) {
        // Only draw vertical sides if there are interior rows between top and bottom
        drawVLine(col,             row + 1, row + height - 2, state);
        drawVLine(col + width - 1, row + 1, row + height - 2, state);
    }
}

void MatrixDisplay::drawFilledRect(uint8_t row, uint16_t col,
                                   uint8_t height, uint8_t width, bool state) {
    for (uint8_t r = row; r < row + height; r++)
        drawHLine(r, col, col + width - 1, state);
}

// ── Bitmap drawing ────────────────────────────────────────────────────────────

void MatrixDisplay::drawBitmap(uint16_t col, const uint8_t* data, uint8_t width) {
    uint16_t total = _mx.getColumnCount();
    for (uint8_t i = 0; i < width && col + i < total; i++)
        _mx.setColumn(col + i, data[i]);
}

void MatrixDisplay::drawBitmapP(uint16_t col, const uint8_t* data_P, uint8_t width) {
    uint16_t total = _mx.getColumnCount();
    for (uint8_t i = 0; i < width && col + i < total; i++)
        _mx.setColumn(col + i, pgm_read_byte(&data_P[i]));
}

// ── Text ─────────────────────────────────────────────────────────────────────

uint16_t MatrixDisplay::printText(const char* text, uint16_t startCol) {
    if (!text) return startCol;
    uint16_t total   = _mx.getColumnCount();
    uint16_t physCol = startCol;   // physical column from the left edge (0 = leftmost)

    for (const char* p = text; *p && physCol < total; ++p) {
        // col 0 is physical RIGHT on FC16; col (total-1) is physical LEFT.
        // setChar(logCol, c) places the leftmost font column at logCol and fills
        // downward, so mapping logCol = (total-1)-physCol puts the char's left
        // edge at the correct physical position.
        uint16_t logCol = (total - 1) - physCol;
        uint8_t  w      = _mx.setChar(logCol, static_cast<uint16_t>(static_cast<uint8_t>(*p)));
        physCol += w + 1;  // advance past char + 1-column inter-character gap
    }
    return physCol;
}

void MatrixDisplay::scrollText(const char* text, uint16_t delayMs) {
    if (!text || !*text) return;

    uint16_t rightCol = _mx.getColumnCount() - 1;
    uint8_t  charBuf[8];

    auto stepDelay = [&]() {
        // Variable delay without requiring millis(): loop 1 ms at a time
        for (uint16_t ms = 0; ms < delayMs; ++ms) _delay_ms(1);
    };

    for (const char* p = text; *p; ++p) {
        uint8_t w = _mx.getChar(static_cast<uint16_t>(static_cast<uint8_t>(*p)),
                                 sizeof(charBuf), charBuf);
        for (uint8_t c = 0; c < w; ++c) {
            _mx.transform(MD_MAX72XX::TSL);
            _mx.setColumn(rightCol, charBuf[c]);
            stepDelay();
        }
        // 1-column gap between characters
        _mx.transform(MD_MAX72XX::TSL);
        _mx.setColumn(rightCol, 0);
        stepDelay();
    }

    // Flush the last character off the left edge
    for (uint16_t c = 0; c < _mx.getColumnCount(); ++c) {
        _mx.transform(MD_MAX72XX::TSL);
        _mx.setColumn(rightCol, 0);
        stepDelay();
    }
}

// ── Non-blocking scroll ───────────────────────────────────────────────────────

void MatrixDisplay::beginScroll(const char* text, uint16_t delayMs) {
    _scroll.p        = text;
    _scroll.colIdx   = 0;
    _scroll.inGap    = false;
    _scroll.delayMs  = delayMs;
    _scroll.lastMs   = millis();
    _scroll.tailCols = _mx.getColumnCount();
    _scroll.active   = (text && *text);

    if (_scroll.active) {
        _scroll.charWidth = _mx.getChar(
            static_cast<uint16_t>(static_cast<uint8_t>(*text)),
            sizeof(_scroll.charBuf), _scroll.charBuf);
    }
}

bool MatrixDisplay::updateScroll() {
    if (!_scroll.active) return false;

    uint32_t now = millis();
    if (static_cast<uint32_t>(now - _scroll.lastMs) < _scroll.delayMs) return true;
    _scroll.lastMs = now;

    _scrollAdvance();
    return _scroll.active;
}

void MatrixDisplay::_scrollAdvance() {
    uint16_t rightCol = _mx.getColumnCount() - 1;
    _mx.transform(MD_MAX72XX::TSL);

    // Text exhausted — push blank tail columns to clear the display
    if (!_scroll.p || !*_scroll.p) {
        _mx.setColumn(rightCol, 0);
        if (_scroll.tailCols > 0 && --_scroll.tailCols == 0)
            _scroll.active = false;
        return;
    }

    uint8_t colData = 0;

    if (_scroll.inGap) {
        // Emit the inter-character blank column then advance to next character
        _scroll.inGap = false;
        ++_scroll.p;
        if (*_scroll.p) {
            _scroll.charWidth = _mx.getChar(
                static_cast<uint16_t>(static_cast<uint8_t>(*_scroll.p)),
                sizeof(_scroll.charBuf), _scroll.charBuf);
            _scroll.colIdx = 0;
        }
        // colData stays 0 (gap column)
    } else {
        colData = (_scroll.colIdx < _scroll.charWidth)
                  ? _scroll.charBuf[_scroll.colIdx]
                  : 0;
        if (++_scroll.colIdx >= _scroll.charWidth)
            _scroll.inGap = true;
    }

    _mx.setColumn(rightCol, colData);
}

} // namespace MikroDuino
