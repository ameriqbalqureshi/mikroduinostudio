/*
 * On-Screen Settings Menu — SSD1306 + Button + EEPROM — MikroDuino
 * Module SDK (capstone)
 *
 * Project 6 of 6 in the examples/Modules/SSD1306 series. A single-
 * button settings menu rendered on the OLED itself, controlling three
 * of the display's OWN hardware-level settings from project 3 —
 * contrast, invert, and flip — each with a real, immediately visible
 * effect, persisted in on-chip EEPROM so they survive a reset. Unlike
 * examples/Modules/LCD/06's backlight setting (which needed an extra
 * transistor on a spare pin to have any physical effect), every
 * setting here is something the SSD1306 class can already do to
 * itself, which makes this menu a genuinely self-contained project.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ SDA     │ PC4   │ I2C data, address 0x3C                     │
 *   │ SCL     │ PC5   │ I2C clock                                  │
 *   │ SW      │ PD4   │ Push button to GND, internal pull-up        │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * Menu gestures (Button's clicked()/doubleClicked()/longPressed()):
 *   clicked()       -> move the highlight to the next setting
 *   doubleClicked() -> change the current setting's value, applied and
 *                       saved immediately
 *   longPressed()   -> hold to reset ALL settings to defaults;
 *                       heldMs() drives a live pixel-width progress
 *                       bar drawn directly on the OLED while held —
 *                       the same "arming" feedback idea as
 *                       examples/Modules/LCD/06's character-cell
 *                       version, this time a genuine filling rectangle
 *                       instead of a row of '#' characters.
 *
 * Settings, each a real SSD1306 hardware effect:
 *   Contrast — 5 presets (0x00, 0x40, 0x80, 0xC0, 0xFF) cycled with
 *              contrast(), the same command project 3's sweep used.
 *   Invert   — ON/OFF, applied with invert() (project 3).
 *   Flip     — None / H / V / Both, applied with flip(flipH, flipV)
 *              (project 3) — cycling this while the menu itself is
 *              visible is a very direct way to see exactly what each
 *              combination does to on-screen text.
 *
 * SSD1306 concepts reused from the whole series:
 *   - clear(), setCursor(), print(const char*), print(int32_t),
 *     display() (project 1).
 *   - drawRect(), fillRect() (project 2), reused here for both the
 *     menu's selection marker and the long-press progress bar.
 *   - contrast(), invert(), flip() (project 3), now driven by menu
 *     state instead of a fixed demo sequence.
 *
 * Non-SSD1306 concepts reused:
 *   - Button (examples/Modules/Button) for the three menu gestures.
 *   - EEPROMDriver with the same 0xA5-magic-byte "first boot vs. real
 *     data" pattern used by examples/Modules/LCD/06 and
 *     examples/Modules/RotaryEncoder/05-06.
 *   - The Timer0-overflow millis() clock (examples/timer/02), driving
 *     Button::update()'s 1 ms cadence.
 */

#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/i2c.hpp>
#include <mikroduino/timer.hpp>
#include <mikroduino/eeprom.hpp>
#include <mikroduino/registers.hpp>
#include <Button.hpp>
#include <SSD1306.hpp>

using namespace MikroDuino;

SSD1306 oled(I2C, 0x3C);
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
    uint8_t contrastIndex;   // 0-4
    uint8_t invertOn;        // 0/1
    uint8_t flipMode;        // 0=none 1=H 2=V 3=Both
};

static constexpr uint16_t EEPROM_ADDR = 0x000;
static constexpr uint8_t  MAGIC       = 0xA5;

static const uint8_t CONTRAST_LEVELS[5] = { 0x00, 0x40, 0x80, 0xC0, 0xFF };

static Settings g_settings;

static void applyContrast() { oled.contrast(CONTRAST_LEVELS[g_settings.contrastIndex]); }
static void applyInvert()   { oled.invert(g_settings.invertOn != 0); }
static void applyFlip() {
    bool flipH = (g_settings.flipMode == 1) || (g_settings.flipMode == 3);
    bool flipV = (g_settings.flipMode == 2) || (g_settings.flipMode == 3);
    oled.flip(flipH, flipV);
}

static void applyAll() { applyContrast(); applyInvert(); applyFlip(); }

static void saveSettings() {
    EEPROM.update(EEPROM_ADDR, g_settings);   // skips the write if unchanged
}

static void loadOrInitSettings() {
    g_settings = EEPROM.get<Settings>(EEPROM_ADDR);
    if (g_settings.magic != MAGIC) {
        g_settings.magic         = MAGIC;
        g_settings.contrastIndex = 4;   // 0xFF, brightest
        g_settings.invertOn      = 0;
        g_settings.flipMode      = 0;
        saveSettings();
    }
    applyAll();
}

// ── Menu state ───────────────────────────────────────────────────────────

static constexpr uint8_t ITEM_COUNT = 3;
static const char ITEM_NAME_0[] PROGMEM = "Contrast";
static const char ITEM_NAME_1[] PROGMEM = "Invert";
static const char ITEM_NAME_2[] PROGMEM = "Flip";
static const char* const ITEM_NAMES[ITEM_COUNT] PROGMEM = { ITEM_NAME_0, ITEM_NAME_1, ITEM_NAME_2 };

static const char FLIP_NAME_0[] PROGMEM = "None";
static const char FLIP_NAME_1[] PROGMEM = "H";
static const char FLIP_NAME_2[] PROGMEM = "V";
static const char FLIP_NAME_3[] PROGMEM = "Both";
static const char* const FLIP_NAMES[4] PROGMEM = { FLIP_NAME_0, FLIP_NAME_1, FLIP_NAME_2, FLIP_NAME_3 };

static uint8_t g_topIndex = 0;

static void printProgmemName(const char* const table[], uint8_t index) {
    const char* p = reinterpret_cast<const char*>(pgm_read_word(&table[index]));
    oled.print(p);
}

static void redrawMenu() {
    oled.clear();

    for (uint8_t i = 0; i < ITEM_COUNT; ++i) {
        oled.setCursor(0, static_cast<uint8_t>(i * 8));
        oled.print(i == g_topIndex ? ">" : " ");
        printProgmemName(ITEM_NAMES, i);
    }

    oled.drawHLine(0, 25, 128);
    oled.setCursor(0, 32);
    switch (g_topIndex) {
        case 0:
            oled.print("Value: ");
            oled.print(static_cast<int32_t>(g_settings.contrastIndex + 1));
            oled.print("/5");
            break;
        case 1:
            oled.print("Value: ");
            oled.print(g_settings.invertOn ? "ON" : "OFF");
            break;
        case 2:
            oled.print("Value: ");
            printProgmemName(FLIP_NAMES, g_settings.flipMode);
            break;
    }

    oled.display();
}

int main() {
    GPIO::inputPullup(PC4);
    GPIO::inputPullup(PC5);

    I2C.beginMaster(400000UL);
    oled.begin();
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
    static constexpr uint8_t  BAR_X = 4, BAR_Y = 56, BAR_W = 120, BAR_H = 6;

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
                    g_settings.contrastIndex = static_cast<uint8_t>((g_settings.contrastIndex + 1) % 5);
                    applyContrast();
                    break;
                case 1:
                    g_settings.invertOn = g_settings.invertOn ? 0 : 1;
                    applyInvert();
                    break;
                case 2:
                    g_settings.flipMode = static_cast<uint8_t>((g_settings.flipMode + 1) % 4);
                    applyFlip();
                    break;
            }
            saveSettings();
            redrawMenu();
        }

        // -------------------------------------------------------------
        // Long-press: reset everything to defaults, with a live pixel
        // progress bar while held. No explicit "clear the bar on
        // release" step is needed here — every press that WASN'T a
        // long-press resolves through clicked()/doubleClicked() below,
        // and every one that WAS resolves through longPressed(); both
        // paths already call redrawMenu(), which naturally erases
        // whatever partial bar was left on screen.
        // -------------------------------------------------------------
        if (menuButton.isDown() && menuButton.heldMs() > 0) {
            uint16_t held = menuButton.heldMs();
            uint8_t filled = static_cast<uint8_t>(
                (static_cast<uint32_t>(held > RESET_HOLD_MS ? RESET_HOLD_MS : held) * BAR_W) / RESET_HOLD_MS
            );

            static uint8_t lastFilled = 0xFF;
            if (filled != lastFilled) {
                lastFilled = filled;
                oled.drawRect(BAR_X - 1, BAR_Y - 1, BAR_W + 2, BAR_H + 2);
                oled.fillRect(BAR_X, BAR_Y, BAR_W, BAR_H, false);   // clear old fill
                if (filled > 0) oled.fillRect(BAR_X, BAR_Y, filled, BAR_H, true);
                oled.display();
            }
        }

        if (menuButton.longPressed()) {
            g_settings.contrastIndex = 4;
            g_settings.invertOn      = 0;
            g_settings.flipMode      = 0;
            applyAll();
            saveSettings();

            oled.clear();
            oled.setCursor(20, 24);
            oled.print("Reset to");
            oled.setCursor(20, 32);
            oled.print("defaults!");
            oled.display();
            _delay_ms(1000);

            redrawMenu();
        }
    }
}
