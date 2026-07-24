/*
 * ST7735 Basics — begin(), fillScreen(), setTextColor(), print() —
 * MikroDuino Module SDK
 *
 * The simplest possible use of the ST7735 module: initialise a 128x160
 * SPI color TFT, print a greeting, and show a live-updating uptime
 * counter below it. This is project 1 of 6 in the examples/Modules/ST7735
 * series, which walks the ST7735 class from a single "Hello World" up to
 * a capstone on-screen settings menu driven by a push button.
 *
 * Hardware (ATmega328P @ 16 MHz, ST7735 128x160 SPI color TFT):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ SCK     │ PB5   │ Hardware SPI clock                        │
 *   │ MOSI    │ PB3   │ Hardware SPI data out (this is a           │
 *   │         │       │ write-only display — no MISO wiring)       │
 *   │ CS      │ PB2   │ Chip select                                │
 *   │ DC      │ PB1   │ Data/Command select                        │
 *   │ RST     │ PB0   │ Reset                                      │
 *   │ VCC/GND │ 3.3V  │ Most breakouts are 3.3V-only — check yours  │
 *   │         │       │ before wiring to a 5V board without level   │
 *   │         │       │ shifting                                   │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * ST7735 concepts introduced:
 *   - No framebuffer, unlike SSD1306: every drawing call talks straight
 *     to the display's own GRAM over SPI. There is no display()/push
 *     step — the instant fillScreen()/print() return, the pixels are
 *     already on screen. See ST7735.hpp's class-level comment for why
 *     (a 128x160 RGB565 framebuffer would be 40 KB — far more RAM than
 *     even an ATmega128 has).
 *   - SPI.beginMaster() — must be called once before ST7735::begin(),
 *     exactly like I2C.beginMaster() is required before an I2C module's
 *     begin() in the SSD1306/DS3231 examples. DIV2 (8 MHz @ 16 MHz F_CPU)
 *     is comfortably inside this controller's SPI speed budget.
 *   - ST7735(cs, dc, rst, bl, spi) — bl (backlight pin) and spi both
 *     default (NO_PIN and the global SPI driver), so a board with no
 *     dedicated backlight pin only needs to pass cs/dc/rst, as here.
 *   - begin(variant, rotation, bgr) — runs the ST7735's documented init
 *     sequence. variant (Variant::BlackTab here, this project's panel)
 *     selects the small GRAM-offset differences between red/black/green
 *     PCB tabs — see the header comment if the picture looks shifted by
 *     a couple of pixels or has a stray line of stale color along one
 *     edge; that is a well-known symptom of picking the wrong Variant,
 *     not a wiring fault.
 *   - fillScreen(color) — the fast path: one continuous SPI burst over
 *     every pixel. color565(r,g,b) or one of the named constants
 *     (ST7735::BLUE here) build a 16-bit RGB565 color.
 *   - setCursor(x, y) — pixel coordinates, not character cells.
 *   - setTextColor(fg, bg) — the two-argument, OPAQUE form: every glyph
 *     cell is written in one burst (fast), with bg filling in around the
 *     letter shapes — used for the uptime counter below so a shorter new
 *     number cleanly overwrites a longer old one without needing to pad
 *     with spaces first. setTextColor(fg) (one argument) is the
 *     transparent form — project 3 covers it.
 *   - print(const char*) / print(int32_t) — same text API shape as
 *     every other display module in this SDK (SSD1306, LCD).
 */

#include <util/delay.h>
#include <mikroduino/spi.hpp>
#include <ST7735.hpp>

using namespace MikroDuino;

ST7735 tft(PB2, PB1, PB0);   // cs, dc, rst — no backlight pin on this board

int main() {
    SPI.beginMaster(SPIClockDiv::DIV2, SPIMode::Mode0);
    tft.begin(ST7735::Variant::BlackTab);

    tft.fillScreen(ST7735::BLUE);
    tft.setTextColor(ST7735::WHITE, ST7735::BLUE);

    tft.setCursor(8, 8);
    tft.print("Hello,");
    tft.setCursor(8, 24);
    tft.print("Mikro-Duino!");

    _delay_ms(2000);

    tft.fillScreen(ST7735::BLACK);
    tft.setTextColor(ST7735::WHITE, ST7735::BLACK);
    tft.setCursor(8, 8);
    tft.print("Uptime (s):");

    int32_t seconds = 0;
    while (true) {
        tft.setCursor(8, 24);
        tft.print(seconds);
        tft.print("   ");   // pad over any leftover digits from a longer previous value

        _delay_ms(1000);
        ++seconds;
    }
}
