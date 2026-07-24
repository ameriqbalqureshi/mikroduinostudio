/*
 * ST7735 Live Potentiometer Gauge Dashboard — ADCDriver + Incremental
 * (Dirty-Rect) Redraw — MikroDuino Module SDK
 *
 * Project 5 of 6 in the examples/Modules/ST7735 series. A color-coded
 * bar gauge tracking a potentiometer in real time, plus small min/max
 * envelope markers. The interesting part isn't the ADC reading itself
 * (examples/Modules/SSD1306/05 already covers that) — it's HOW the bar
 * gets redrawn: SSD1306's version clears its whole framebuffer and
 * redraws the entire scene every loop, which costs nothing extra
 * because display() pushes the same 1024 bytes either way. ST7735 has
 * no framebuffer, so a naive "clear the whole screen, redraw everything"
 * loop here would mean re-sending every pixel of the gauge 10 times a
 * second even though only the bar's fill width actually changed — this
 * project instead redraws only the pixels that changed since the last
 * frame (the "dirty rectangle" technique), which is the normal way any
 * unbuffered display driver handles animation.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ SCK/MOSI│ PB5/3 │ Hardware SPI                               │
 *   │ CS/DC   │ PB2/1 │ ST7735 (projects 1-4)                      │
 *   │ RST     │ PB0   │                                            │
 *   │ ADC0    │ PC0   │ Potentiometer wiper; outer legs to 5V/GND  │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * ST7735 concepts reused from projects 1-2:
 *   - fillScreen(), drawRect(), fillTriangle() for the static frame and
 *     the marker triangles.
 *   - setTextColor(fg, bg) opaque text (project 1) for the numeric
 *     readout — a shorter new number cleanly overwrites a longer old
 *     one without the bar's manual erase-then-redraw dance below.
 *
 * New (non-ST7735) library used for interactivity:
 *   - ADCDriver (mikroduino/adc.hpp): ADC_Driver.begin() and
 *     ADC_Driver.read(channel) — the same blocking single-conversion
 *     call examples/Modules/SSD1306/05 and examples/Modules/LCD/05 use.
 *
 * The dirty-rect technique itself (ordinary application logic, not part
 * of the ST7735 class): this project remembers the PREVIOUS frame's bar
 * fill width and color. Each redraw first erases only the sliver the bar
 * shrank by (if it got narrower) with a background-colored fillRect(),
 * then draws the current fill width in the current zone color — never
 * touching the pixels that didn't change. The min/max markers work the
 * same way: only erased and redrawn when a new extreme is actually set.
 */

#include <util/delay.h>
#include <mikroduino/spi.hpp>
#include <mikroduino/adc.hpp>
#include <ST7735.hpp>

using namespace MikroDuino;

static constexpr uint8_t POT_CHANNEL = 0;   // ADC0 / PC0

ST7735 tft(PB2, PB1, PB0);

static constexpr int16_t BAR_X = 8, BAR_Y = 60, BAR_W = 112, BAR_H = 20;
static constexpr int16_t MARK_Y = 44;   // min/max triangle row, above the bar

static int16_t potToBarX(uint16_t potValue) {
    return static_cast<int16_t>(BAR_X + (static_cast<uint32_t>(potValue) * (BAR_W - 1)) / 1023u);
}

static uint16_t zoneColor(uint16_t potValue) {
    uint16_t percent = static_cast<uint16_t>((static_cast<uint32_t>(potValue) * 100u) / 1023u);
    if (percent < 40)  return ST7735::GREEN;
    if (percent < 75)  return ST7735::YELLOW;
    return ST7735::RED;
}

static void drawMarker(int16_t x, uint16_t color) {
    tft.fillTriangle(static_cast<int16_t>(x - 3), MARK_Y,
                      static_cast<int16_t>(x + 3), MARK_Y,
                      x,                            static_cast<int16_t>(MARK_Y + 6), color);
}

int main() {
    SPI.beginMaster(SPIClockDiv::DIV2, SPIMode::Mode0);
    tft.begin(ST7735::Variant::BlackTab);
    ADC_Driver.begin();

    tft.fillScreen(ST7735::BLACK);
    tft.setTextColor(ST7735::WHITE, ST7735::BLACK);
    tft.setCursor(4, 4);
    tft.print("Pot Gauge");
    tft.drawRect(BAR_X - 1, BAR_Y - 1, BAR_W + 2, BAR_H + 2, ST7735::WHITE);

    uint16_t minValue = 1023;
    uint16_t maxValue = 0;
    int16_t  prevFillW = 0;
    uint16_t prevColor = ST7735::BLACK;

    while (true) {
        uint16_t potValue = ADC_Driver.read(POT_CHANNEL);

        // ---- Numeric readout — opaque text overwrites itself cleanly ----
        tft.setCursor(4, 20);
        tft.print("ADC: ");
        tft.print(static_cast<int32_t>(potValue));
        tft.print("   ");

        // ---- Bar: dirty-rect update, only touching changed pixels ----
        uint16_t color = zoneColor(potValue);
        int16_t  fillW = static_cast<int16_t>((static_cast<uint32_t>(potValue) * BAR_W) / 1023u);

        if (fillW < prevFillW) {
            // Shrank: erase exactly the sliver that's no longer filled.
            tft.fillRect(static_cast<int16_t>(BAR_X + fillW), BAR_Y,
                         static_cast<int16_t>(prevFillW - fillW), BAR_H, ST7735::BLACK);
        }
        if (fillW > 0 && (fillW != prevFillW || color != prevColor)) {
            // Grew, or same width but crossed into a new color zone —
            // either way, the filled portion itself needs a fresh draw.
            tft.fillRect(BAR_X, BAR_Y, fillW, BAR_H, color);
        }
        prevFillW = fillW;
        prevColor = color;

        // ---- Min/max markers — only touched when a new extreme is set ----
        bool newExtreme = false;
        if (potValue < minValue) { minValue = potValue; newExtreme = true; }
        if (potValue > maxValue) { maxValue = potValue; newExtreme = true; }

        if (newExtreme) {
            tft.fillRect(BAR_X, MARK_Y, BAR_W, 7, ST7735::BLACK);   // clear the whole marker row
            drawMarker(potToBarX(minValue), ST7735::CYAN);
            drawMarker(potToBarX(maxValue), ST7735::MAGENTA);
        }

        // A human turning a knob doesn't need sub-100ms feedback, and
        // touching only the changed pixels keeps every redraw well under
        // that budget regardless.
        _delay_ms(100);
    }
}
