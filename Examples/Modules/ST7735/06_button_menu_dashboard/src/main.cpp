/*
 * On-Screen Settings Menu — ST7735 + Button + EEPROM — MikroDuino
 * Module SDK (capstone)
 *
 * Project 6 of 6 in the examples/Modules/ST7735 series. A single-button
 * settings menu rendered on the TFT itself, controlling three of the
 * display's OWN hardware-level settings — rotation, inversion, and
 * backlight — each with a real, immediately visible effect, persisted
 * in on-chip EEPROM so they survive a reset. Same structure as
 * examples/Modules/SSD1306/06's capstone (contrast/invert/flip), with
 * ST7735-specific settings in place of SSD1306's.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ SCK/MOSI│ PB5/3 │ Hardware SPI                               │
 *   │ CS/DC   │ PB2/1 │ ST7735 (projects 1-5)                      │
 *   │ RST     │ PB0   │                                            │
 *   │ BL      │ PC1   │ Backlight enable — unlike projects 1-5,     │
 *   │         │       │ this project actually wires and drives it,  │
 *   │         │       │ since the Backlight setting needs a real    │
 *   │         │       │ pin to have any physical effect              │
 *   │ SW      │ PD4   │ Push button to GND, internal pull-up         │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * Menu gestures (Button's clicked()/doubleClicked()/longPressed()):
 *   clicked()       -> move the highlight to the next setting
 *   doubleClicked() -> change the current setting's value, applied and
 *                       saved immediately
 *   longPressed()   -> hold to reset ALL settings to defaults;
 *                       heldMs() drives a live filled-rectangle
 *                       progress bar while held, in a color, the same
 *                       "arming" feedback idea as
 *                       examples/Modules/SSD1306/06's pixel-width
 *                       version.
 *
 * Settings, each a real ST7735-driver-level effect:
 *   Rotation  — Deg0/Deg90/Deg180/Deg270, applied with setRotation()
 *               (project 3). Changing it mid-menu changes width()/
 *               height(), so redrawMenu() below always derives its
 *               layout from those rather than a hardcoded size — the
 *               same discipline project 3 established.
 *   Invert    — ON/OFF, applied with invertDisplay().
 *   Backlight — ON/OFF, applied with backlight() — the one setting in
 *               this project that needs its own wired pin (see the
 *               hardware table above) to do anything at all.
 *
 * ST7735 concepts reused from the whole series:
 *   - fillScreen(), setCursor(), setTextColor() opaque, print() (project 1).
 *   - fillRect(), drawFastHLine() (project 2).
 *   - setRotation(), width(), height() (project 3).
 *   - invertDisplay(), backlight() — declared in the header alongside
 *     begin()/enableDisplay()/sleep(), used here for the first time in
 *     the series.
 *
 * Non-ST7735 concepts reused:
 *   - Button (examples/Modules/Button) for the three menu gestures.
 *   - EEPROMDriver with the same magic-byte "first boot vs. real data"
 *     pattern used by examples/Modules/SSD1306/06 and
 *     examples/Modules/LCD/06.
 *   - The Timer0-overflow millis() clock (examples/timer/02), driving
 *     Button::update()'s 1 ms cadence.
 */

#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <mikroduino/spi.hpp>
#include <mikroduino/timer.hpp>
#include <mikroduino/eeprom.hpp>
#include <mikroduino/registers.hpp>
#include <Button.hpp>
#include <ST7735.hpp>

using namespace MikroDuino;

ST7735 tft(PB2, PB1, PB0, PC1);   // cs, dc, rst, bl
Button  menuButton(PD4);

// ── millis() clock (same technique as examples/timer/02_software_millis_clock) ──

static constexpr uint16_t MICROS_PER_OVERFLOW = 1024;
static constexpr uint16_t MILLIS_INC = MICROS_PER_OVERFLOW / 1000;
static constexpr uint16_t FRACT_INC  = MICROS_PER_OVERFLOW % 1000;
static constexpr uint16_t FRACT_MAX  = 1000;

static volatile uint32_t g_millis = 0;
static volatile uint16_t g_fract  = 0;

ISR(TIMER0_OVF_vect) {
    uint32_t m = g_millis;
    uint16_t f = g_fract;
    m += MILLIS_INC;
    f += FRACT_INC;
    if (f >= FRACT_MAX) { f -= FRACT_MAX; ++m; }
    g_fract  = f;
    g_millis = m;
}

static uint32_t millis() {
    uint32_t snapshot;
    ATOMIC_BLOCK_START;
    snapshot = g_millis;
    ATOMIC_BLOCK_END;
    return snapshot;
}

// ── Settings, EEPROM-backed ─────────────────────────────────────────────

struct Settings {
    uint8_t magic;
    uint8_t rotationIndex;   // 0-3
    uint8_t invertOn;        // 0/1
    uint8_t backlightOn;     // 0/1
};

static constexpr uint16_t EEPROM_ADDR = 0x000;
static constexpr uint8_t  MAGIC       = 0xB7;

static Settings g_settings;

static void applyRotation()  { tft.setRotation(static_cast<ST7735::Rotation>(g_settings.rotationIndex)); }
static void applyInvert()    { tft.invertDisplay(g_settings.invertOn != 0); }
static void applyBacklight() { tft.backlight(g_settings.backlightOn != 0); }
static void applyAll() { applyRotation(); applyInvert(); applyBacklight(); }

static void saveSettings() {
    EEPROM.update(EEPROM_ADDR, g_settings);   // skips the write if unchanged
}

static void loadOrInitSettings() {
    g_settings = EEPROM.get<Settings>(EEPROM_ADDR);
    if (g_settings.magic != MAGIC) {
        g_settings.magic         = MAGIC;
        g_settings.rotationIndex = 0;
        g_settings.invertOn      = 0;
        g_settings.backlightOn   = 1;
        saveSettings();
    }
    applyAll();
}

// ── Menu state ───────────────────────────────────────────────────────────

static constexpr uint8_t ITEM_COUNT = 3;
static const char ITEM_NAME_0[] PROGMEM = "Rotation";
static const char ITEM_NAME_1[] PROGMEM = "Invert";
static const char ITEM_NAME_2[] PROGMEM = "Backlight";
static const char* const ITEM_NAMES[ITEM_COUNT] PROGMEM = { ITEM_NAME_0, ITEM_NAME_1, ITEM_NAME_2 };

static const char ROT_NAME_0[] PROGMEM = "0deg";
static const char ROT_NAME_1[] PROGMEM = "90deg";
static const char ROT_NAME_2[] PROGMEM = "180deg";
static const char ROT_NAME_3[] PROGMEM = "270deg";
static const char* const ROT_NAMES[4] PROGMEM = { ROT_NAME_0, ROT_NAME_1, ROT_NAME_2, ROT_NAME_3 };

static uint8_t g_topIndex = 0;

static void printProgmemName(const char* const table[], uint8_t index) {
    const char* p = reinterpret_cast<const char*>(pgm_read_word(&table[index]));
    tft.print_P(p);
}

static void redrawMenu() {
    int16_t w = static_cast<int16_t>(tft.width());

    tft.fillScreen(ST7735::BLACK);
    tft.setTextColor(ST7735::WHITE, ST7735::BLACK);

    for (uint8_t i = 0; i < ITEM_COUNT; ++i) {
        tft.setCursor(4, static_cast<int16_t>(8 + i * 16));
        tft.print(i == g_topIndex ? ">" : " ");
        printProgmemName(ITEM_NAMES, i);
    }

    tft.drawFastHLine(0, 60, w, ST7735::WHITE);

    tft.setCursor(4, 68);
    tft.print("Value: ");
    switch (g_topIndex) {
        case 0: printProgmemName(ROT_NAMES, g_settings.rotationIndex); break;
        case 1: tft.print(g_settings.invertOn    ? "ON" : "OFF");      break;
        case 2: tft.print(g_settings.backlightOn ? "ON" : "OFF");      break;
    }
}

int main() {
    SPI.beginMaster(SPIClockDiv::DIV2, SPIMode::Mode0);
    tft.begin(ST7735::Variant::BlackTab);
    menuButton.begin();

    Timer0.mode(TimerMode::Normal);
    Timer0.prescaler(TimerPrescaler::DIV64);
    Timer0.start();
    Timer0.enableOverflow();
    sei();

    loadOrInitSettings();
    redrawMenu();

    uint32_t lastUpdateMs = millis();

    static constexpr uint16_t RESET_HOLD_MS = 1500;
    static constexpr int16_t  BAR_X = 4, BAR_Y = 140, BAR_H = 8;

    while (true) {
        uint32_t now = millis();

        while (now - lastUpdateMs >= 1) {
            menuButton.update();
            ++lastUpdateMs;
        }

        if (menuButton.clicked()) {
            g_topIndex = static_cast<uint8_t>((g_topIndex + 1) % ITEM_COUNT);
            redrawMenu();
        }

        if (menuButton.doubleClicked()) {
            switch (g_topIndex) {
                case 0:
                    g_settings.rotationIndex = static_cast<uint8_t>((g_settings.rotationIndex + 1) % 4);
                    applyRotation();   // geometry may have changed — redrawMenu() below re-derives layout
                    break;
                case 1:
                    g_settings.invertOn = g_settings.invertOn ? 0 : 1;
                    applyInvert();
                    break;
                case 2:
                    g_settings.backlightOn = g_settings.backlightOn ? 0 : 1;
                    applyBacklight();
                    break;
            }
            saveSettings();
            redrawMenu();
        }

        // -------------------------------------------------------------
        // Long-press: reset everything to defaults, with a live filled
        // progress bar while held. No explicit "clear the bar on
        // release" step is needed: every press that WASN'T a long-press
        // resolves through clicked()/doubleClicked() above, and every
        // one that WAS resolves through longPressed() below; both paths
        // already call redrawMenu(), which naturally erases whatever
        // partial bar was left on screen.
        // -------------------------------------------------------------
        if (menuButton.isDown() && menuButton.heldMs() > 0) {
            int16_t barW = static_cast<int16_t>(tft.width() - 2 * BAR_X);
            uint16_t held = menuButton.heldMs();
            int16_t filled = static_cast<int16_t>(
                (static_cast<uint32_t>(held > RESET_HOLD_MS ? RESET_HOLD_MS : held) * barW) / RESET_HOLD_MS
            );

            static int16_t lastFilled = -1;
            if (filled != lastFilled) {
                lastFilled = filled;
                tft.drawRect(static_cast<int16_t>(BAR_X - 1), static_cast<int16_t>(BAR_Y - 1),
                             static_cast<int16_t>(barW + 2), static_cast<int16_t>(BAR_H + 2), ST7735::WHITE);
                tft.fillRect(BAR_X, BAR_Y, barW, BAR_H, ST7735::BLACK);   // clear old fill
                if (filled > 0) tft.fillRect(BAR_X, BAR_Y, filled, BAR_H, ST7735::RED);
            }
        }

        if (menuButton.longPressed()) {
            g_settings.rotationIndex = 0;
            g_settings.invertOn      = 0;
            g_settings.backlightOn   = 1;
            applyAll();
            saveSettings();

            tft.fillScreen(ST7735::BLACK);
            tft.setTextColor(ST7735::WHITE, ST7735::BLACK);
            tft.setCursor(20, 70);
            tft.print("Reset to");
            tft.setCursor(20, 86);
            tft.print("defaults!");
            _delay_ms(1000);

            redrawMenu();
        }
    }
}
