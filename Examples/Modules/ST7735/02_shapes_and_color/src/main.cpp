/*
 * ST7735 Drawing Primitives + color565() — MikroDuino Module SDK
 *
 * Project 2 of 6 in the examples/Modules/ST7735 series. Composes a
 * single static scene using every shape primitive the driver exposes.
 * Unlike examples/Modules/SSD1306/02 (which redraws its scene into a
 * framebuffer every loop, then pushes it with display()), this project
 * draws its scene exactly ONCE: since ST7735 has no framebuffer, every
 * primitive call is already visible the instant it returns, and
 * redrawing the same static scene on a timer would only cost SPI
 * bandwidth and risk visible flicker on the shapes being redrawn — see
 * ST7735.hpp's class-level comment on the fast (span-based) vs. slow
 * (per-pixel) primitives for why that matters more here than it did for
 * SSD1306's RAM-buffered redraws.
 *
 * Hardware: identical to project 1 (ATmega328P @ 16 MHz, SPI color TFT):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ SCK/MOSI│ PB5/3 │ Hardware SPI                               │
 *   │ CS/DC   │ PB2/1 │                                            │
 *   │ RST     │ PB0   │                                            │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * ST7735 concepts introduced:
 *   - color565(r, g, b) — builds a 16-bit RGB565 color from 8-bit
 *     channel values; used below for two shades the named-constant
 *     palette doesn't cover (a mid-gray and a pale pink).
 *   - drawPixel(x, y, color) — the primitive drawLine()/drawCircle()
 *     are themselves built from; each call opens and closes its own
 *     1x1 GRAM address window.
 *   - drawFastHLine / drawFastVLine — axis-aligned spans, each a single
 *     continuous SPI burst regardless of length — the cheapest primitive
 *     in the driver per pixel drawn.
 *   - drawLine — any slope; a diagonal costs one drawPixel() (and
 *     therefore one address-window command) per point, meaningfully
 *     more per pixel than the fast axis-aligned lines above.
 *   - drawRect / fillRect — outline vs. filled rectangles, both built
 *     from the fast span primitives.
 *   - drawCircle / fillCircle — outline (per-pixel, like drawLine) vs.
 *     filled (built from drawFastHLine spans, so it's the fast one).
 *   - drawRoundRect / fillRoundRect — a rectangle with quarter-circle
 *     corners; the corner radius is clamped internally if it's larger
 *     than half the shape's width or height.
 *   - drawTriangle / fillTriangle — outline (three drawLine() calls) vs.
 *     filled (a scanline fill built from drawFastHLine spans).
 */

#include <util/delay.h>
#include <mikroduino/spi.hpp>
#include <ST7735.hpp>

using namespace MikroDuino;

ST7735 tft(PB2, PB1, PB0);

int main() {
    SPI.beginMaster(SPIClockDiv::DIV2, SPIMode::Mode0);
    tft.begin(ST7735::Variant::BlackTab);

    tft.fillScreen(ST7735::BLACK);

    // ---- Title ----
    tft.setTextColor(ST7735::WHITE, ST7735::BLACK);
    tft.setCursor(4, 4);
    tft.print("Shapes + Color");
    tft.drawFastHLine(0, 16, tft.width(), ST7735::WHITE);

    // ---- Nested rectangle outlines, top-left ----
    tft.drawRect(4,  22, 40, 40, ST7735::RED);
    tft.drawRect(9,  27, 30, 30, ST7735::ORANGE);
    tft.drawRect(14, 32, 20, 20, ST7735::YELLOW);
    tft.drawRect(19, 37, 10, 10, ST7735::GREEN);

    // ---- Filled rectangle with a hollow center, top-middle ----
    tft.fillRect(52, 22, 30, 40, ST7735::CYAN);
    tft.fillRect(60, 30, 14, 24, ST7735::BLACK);

    // ---- Circles, top-right ----
    uint16_t gray = ST7735::color565(128, 128, 128);
    tft.drawCircle(108, 42, 18, ST7735::MAGENTA);
    tft.fillCircle(108, 42, 10, gray);

    // ---- Rounded rectangles, middle row ----
    uint16_t pink = ST7735::color565(255, 180, 200);
    tft.drawRoundRect(4,  70, 56, 30, 8, ST7735::BLUE);
    tft.fillRoundRect(66, 70, 56, 30, 8, pink);

    // ---- Triangles, bottom row ----
    tft.drawTriangle(10, 155, 40, 105, 55, 155, ST7735::WHITE);
    tft.fillTriangle(70, 155, 100, 105, 122, 155, ST7735::GREEN);

    // ---- Diagonal line fan across the very bottom edge ----
    for (int16_t i = 0; i < 8; ++i)
        tft.drawLine(0, 159, static_cast<int16_t>(16 * i), 130, ST7735::YELLOW);

    // Static scene: nothing to update once drawn (see the header note
    // above on why this project doesn't redraw on a timer).
    while (true) {}
}
