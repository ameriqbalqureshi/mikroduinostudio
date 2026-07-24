#pragma once
/*
 * MikroDuino ST7735 Module
 * 128x160 (and 128x128 / 160x80) SPI color TFT display driver, RGB565.
 *
 * Architecture — NO framebuffer:
 *   A 128x160 RGB565 framebuffer is 128*160*2 = 40960 bytes — far more RAM
 *   than an ATmega328P has in total (2 KB), and more than half of what even
 *   an ATmega128 has (4 KB). Unlike SSD1306 (a 1024-byte monochrome
 *   framebuffer comfortably fits in RAM), every drawing call in this driver
 *   talks to the display's own GRAM directly over SPI: it opens a
 *   rectangular address window (CASET/RASET/RAMWR) and streams pixel colors
 *   straight out, with no local copy anywhere.
 *
 *   Consequence for performance: fillScreen(), fillRect(), drawFastHLine/
 *   VLine(), text, and PROGMEM bitmaps all open ONE address window and
 *   stream every pixel in a single continuous SPI burst — fast. drawLine()
 *   on a diagonal, drawCircle(), and drawTriangle()'s outline instead call
 *   drawPixel() per point, and EACH drawPixel() opens and closes its own
 *   1x1 address window (a handful of command bytes on top of the 2 data
 *   bytes) — correct, but meaningfully slower per pixel than a filled
 *   shape. For animation-heavy diagonal graphics, prefer redrawing only
 *   the small dirty rectangle that actually changed.
 *
 * Wiring (4-wire SPI + control lines):
 *
 *   ┌─────────┬───────────────┬──────────────────────────────────────────┐
 *   │ Signal  │ ATmega328P    │ Notes                                     │
 *   ├─────────┼───────────────┼──────────────────────────────────────────┤
 *   │ SCK     │ PB5           │ Hardware SPI clock                        │
 *   │ MOSI    │ PB3           │ Hardware SPI data out (display has no     │
 *   │         │               │ MISO — this is a write-only device)       │
 *   │ CS      │ any free pin  │ Passed to the constructor; PB2 (hardware  │
 *   │         │               │ SS) is the natural choice but not         │
 *   │         │               │ required — SPI.beginMaster() configures   │
 *   │         │               │ PB2 as an output regardless, since the    │
 *   │         │               │ AVR SPI hardware requires that even when  │
 *   │         │               │ a different pin drives this device's CS   │
 *   │ DC      │ any free pin  │ Data/Command select                       │
 *   │ RST     │ any free pin  │ Optional — pass NO_PIN to omit and rely   │
 *   │         │               │ on the software reset command instead     │
 *   │ BL      │ any free pin  │ Optional backlight enable — many breakout │
 *   │         │               │ boards tie this straight to 3.3V instead  │
 *   │ VCC/GND │ 3.3V/GND      │ Most ST7735 breakouts are 3.3V-only —     │
 *   │         │               │ check the specific board before wiring    │
 *   │         │               │ it to a 5V MCU without level shifting     │
 *   └─────────┴───────────────┴──────────────────────────────────────────┘
 *
 * Usage:
 *   SPI.beginMaster(SPIClockDiv::DIV2, SPIMode::Mode0);  // fast, mode 0
 *   ST7735 tft(PB2, PB1, PB0);          // cs, dc, rst
 *   tft.begin();                         // default: BlackTab, 128x160
 *   tft.fillScreen(ST7735::BLUE);
 *   tft.setCursor(4, 4);
 *   tft.setTextColor(ST7735::WHITE, ST7735::BLUE);
 *   tft.print("Hello!");
 *
 * Panel variants: this exact controller ships behind red, black, and green
 * PCB tabs (a decades-old hobbyist naming convention, not this driver's
 * invention) with slightly different visible-GRAM offsets, plus a 128x128
 * square variant and a 160x80 "mini" variant. begin(Variant) selects the
 * right offsets/dimensions — if the picture is shifted a couple of pixels
 * or has a stray line of stale pixels along one edge, try a different
 * Variant; this is a well-known, common source of confusion across every
 * ST7735 library, not specific to this one.
 *
 * Requires: ST7735.cpp compiled with the project.
 */

#include <mikroduino/gpio.hpp>
#include <mikroduino/spi.hpp>
#include <stdint.h>
#include <avr/pgmspace.h>

namespace MikroDuino {

class ST7735 {
public:

    // Pass to rstPin/blPin to indicate "not wired."
    static constexpr uint8_t NO_PIN = 0xFFu;

    enum class Variant : uint8_t {
        BlackTab,     // 128x160, colstart 0, rowstart 0 — most common today
        RedTab,       // 128x160, colstart 0, rowstart 0
        GreenTab,     // 128x160 visible on 132x162 GRAM, colstart 2, rowstart 1
        GreenTab128,  // 128x128 square panel, colstart 2, rowstart 3
        Mini160x80,   // 160x80 "mini" panel, colstart 26, rowstart 1
    };

    enum class Rotation : uint8_t { Deg0 = 0, Deg90 = 1, Deg180 = 2, Deg270 = 3 };

    // ================================================================
    // RGB565 color helper + a common named palette
    // ================================================================

    static constexpr uint16_t color565(uint8_t r, uint8_t g, uint8_t b) {
        return static_cast<uint16_t>(((static_cast<uint16_t>(r) & 0xF8u) << 8) |
                                      ((static_cast<uint16_t>(g) & 0xFCu) << 3) |
                                      (b >> 3));
    }

    static constexpr uint16_t BLACK   = 0x0000;
    static constexpr uint16_t WHITE   = 0xFFFF;
    static constexpr uint16_t RED     = 0xF800;
    static constexpr uint16_t GREEN   = 0x07E0;
    static constexpr uint16_t BLUE    = 0x001F;
    static constexpr uint16_t CYAN    = 0x07FF;
    static constexpr uint16_t MAGENTA = 0xF81F;
    static constexpr uint16_t YELLOW  = 0xFFE0;
    static constexpr uint16_t ORANGE  = 0xFC00;

    // csPin/dcPin: required. rstPin/blPin: NO_PIN to omit.
    // spi: which SPI bus driver to use; defaults to the shared global SPI.
    // Does NOT call SPI.beginMaster() itself — the bus may be shared with
    // other devices, so call that once yourself before begin().
    explicit ST7735(uint8_t csPin, uint8_t dcPin,
                     uint8_t rstPin = NO_PIN, uint8_t blPin = NO_PIN,
                     SPIDriver& spi = SPI)
        : _spi(spi), _cs(csPin), _dc(dcPin), _rst(rstPin), _bl(blPin),
          _colstart(0), _rowstart(0), _panelW(128), _panelH(160),
          _width(128), _height(160),
          _cursorX(0), _cursorY(0), _textSize(1),
          _fg(WHITE), _bg(BLACK), _transparentBg(true) {}

    // ================================================================
    // Initialisation
    // ================================================================

    // Resets (hardware if rstPin given, otherwise software) and runs the
    // ST7735's documented init sequence, then applies rotation.
    // bgr: most of these clone boards are wired BGR, not RGB — leave this
    // true unless red/blue look swapped, then pass false.
    void begin(Variant variant = Variant::BlackTab, Rotation rotation = Rotation::Deg0,
               bool bgr = true);

    void setRotation(Rotation rotation);
    Rotation rotation() const { return _rotation; }

    uint16_t width()  const { return _width; }
    uint16_t height() const { return _height; }

    // Backlight control. No-op if blPin was NO_PIN.
    void backlight(bool on);

    void enableDisplay(bool on);     // DISPON / DISPOFF — GRAM retained either way
    void sleep(bool enable);         // SLPIN / SLPOUT — deeper power-down than enableDisplay
    void invertDisplay(bool inv);

    // ================================================================
    // Low-level streaming API (what every drawing primitive below is
    // built from). Use directly for custom fast fills — e.g. a computed
    // gradient row — that the primitives below don't cover.
    // ================================================================

    // Opens a rectangular GRAM address window and readies the bus for
    // exactly w*h pushColor() calls. Coordinates are NOT clipped here —
    // callers are expected to already be within [0,width) x [0,height).
    void startWrite(int16_t x, int16_t y, int16_t w, int16_t h);
    void pushColor(uint16_t color);
    void pushColors(const uint16_t* colors, uint32_t count);
    void pushColors_P(const uint16_t* pgmColors, uint32_t count);
    void endWrite();

    // ================================================================
    // Pixel + fast axis-aligned primitives (single continuous SPI burst)
    // ================================================================

    void drawPixel(int16_t x, int16_t y, uint16_t color);

    void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color);
    void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color);

    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
    void fillScreen(uint16_t color) { fillRect(0, 0, _width, _height, color); }

    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);

    // ================================================================
    // Per-pixel primitives (see the class-level note on performance)
    // ================================================================

    void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);

    void drawCircle(int16_t cx, int16_t cy, int16_t r, uint16_t color);
    void fillCircle(int16_t cx, int16_t cy, int16_t r, uint16_t color);   // fast: uses drawFastHLine spans

    void drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color);
    void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color);  // fast: uses fillRect + fillCircle spans

    void drawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                       int16_t x2, int16_t y2, uint16_t color);
    void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                       int16_t x2, int16_t y2, uint16_t color);           // fast: uses drawFastHLine spans

    // ================================================================
    // Bitmaps (PROGMEM)
    // ================================================================

    // Straight RGB565 pixel dump, row-major, w*h uint16_t entries. Fast:
    // one continuous burst. Must be placed fully on-screen (x,y >= 0 and
    // x+w/y+h within width()/height()) — silently does nothing otherwise,
    // since a straight PROGMEM-to-GRAM streamer can't skip source pixels
    // mid-row to clip a partially offscreen bitmap.
    void drawBitmap565(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t* pgmData);

    // 1-bit-per-pixel bitmap, row-major, MSB-first, each row padded to a
    // byte boundary (the format every "image to Arduino code" converter
    // targeting Adafruit_GFX-style libraries produces). Set bits draw
    // fgColor; clear bits draw bgColor if opaque=true, or are skipped
    // (left showing whatever was already on screen) if opaque=false.
    // Same on-screen requirement as drawBitmap565() when opaque=true;
    // opaque=false draws pixel-by-pixel and clips normally.
    void drawBitmap1(int16_t x, int16_t y, int16_t w, int16_t h, const uint8_t* pgmData,
                      uint16_t fgColor, uint16_t bgColor = BLACK, bool opaque = true);

    // ================================================================
    // Text (5x7 font, scalable; each unscaled glyph cell is 6x8:
    // 5 font columns + 1 column of spacing)
    // ================================================================

    void setCursor(int16_t x, int16_t y) { _cursorX = x; _cursorY = y; }
    void setTextSize(uint8_t size) { _textSize = (size == 0) ? 1 : size; }

    // One-argument form: transparent background (only fg pixels drawn,
    // slower — see the class-level performance note). Two-argument form:
    // opaque background, one fast continuous burst per glyph cell.
    void setTextColor(uint16_t fg) { _fg = fg; _transparentBg = true; }
    void setTextColor(uint16_t fg, uint16_t bg) { _fg = fg; _bg = bg; _transparentBg = false; }

    void print(char c);
    void print(const char* str);
    void print_P(const char* pgmStr);
    void print(int32_t value);
    void printU(uint32_t value, uint8_t base = 10);

private:
    SPIDriver& _spi;
    uint8_t    _cs, _dc, _rst, _bl;

    uint8_t  _colstart, _rowstart;   // GRAM offset for the selected Variant
    uint16_t _panelW, _panelH;       // native (rotation 0) dimensions
    uint16_t _width, _height;        // effective dimensions after rotation
    Rotation _rotation = Rotation::Deg0;
    bool     _bgr = true;

    int16_t  _cursorX, _cursorY;
    uint8_t  _textSize;
    uint16_t _fg, _bg;
    bool     _transparentBg;

    static void _delayMs(uint16_t ms);
    void _hwReset();
    void _cmd(uint8_t c);
    void _cmdArgs(uint8_t c, const uint8_t* args, uint8_t n);

    // Clip a rectangle to the screen. Returns false if fully offscreen.
    bool _clip(int16_t& x, int16_t& y, int16_t& w, int16_t& h) const;

    // cornerMask / here and below: bit0=top-right, bit1=top-left,
    // bit2=bottom-left, bit3=bottom-right (private convention, used only
    // by drawRoundRect/fillRoundRect).
    void _drawCircleHelper(int16_t cx, int16_t cy, int16_t r, uint8_t cornerMask, uint16_t color);
    void _fillCircleHelper(int16_t cx, int16_t cy, int16_t r, uint8_t cornerMask, uint16_t color);

    static uint8_t _fontByte(char c, uint8_t col);
};

} // namespace MikroDuino
