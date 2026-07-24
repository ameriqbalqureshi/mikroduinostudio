/*
 * On-Screen Settings Menu — LCD + Button + EEPROM — MikroDuino Module SDK
 * (capstone)
 *
 * Project 6 of 6 in the examples/Modules/LCD series. A real single-
 * button settings menu rendered on the LCD itself (rather than over
 * USART, the way examples/Modules/Button/04_double_click_menu did the
 * same kind of navigation) — three settings, each backed by an actual
 * hardware effect, persisted in on-chip EEPROM so they survive a reset.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌──────────────┬───────┬──────────────────────────────────────────┐
 *   │ RS           │ PB0   │ LCD (same wiring as projects 1-5)          │
 *   │ EN           │ PB1   │                                            │
 *   │ D4-D7        │ PB2-5 │                                            │
 *   │ Backlight    │ PC1   │ Drives a backlight transistor/MOSFET —    │
 *   │              │       │ HIGH = backlight on                        │
 *   │ Menu button  │ PD2   │ Other leg to GND — all menu navigation     │
 *   │ Screen toggle│ PD3   │ Other leg to GND — hold 1.5s: blank/wake   │
 *   │              │       │ the display without losing its contents    │
 *   └──────────────┴───────┴──────────────────────────────────────────┘
 *
 * Menu gestures (all on the PD2 button):
 *   clicked()       -> move to the next setting
 *   doubleClicked() -> change the current setting's value
 *   longPressed()   -> hold to reset ALL settings to defaults; heldMs()
 *                       drives a live progress bar on the LCD itself
 *                       while held — the same "arming" feedback idea
 *                       Button project 3 built with an LED, this time
 *                       rendered as on-screen UI instead
 *
 * Settings, each with a real effect:
 *   Backlight  — doubleClicked() toggles the PC1 backlight drive pin.
 *   Cursor     — doubleClicked() cycles Off -> Cursor -> Blink -> Both,
 *                 applying LCD cursor()/blink() live.
 *   Icon Demo  — doubleClicked() cycles through 3 createChar() icons
 *                 (not persisted — purely a visual createChar() demo,
 *                 project 4's technique reused here inside a menu
 *                 instead of a standalone animation).
 *
 * LCD concepts reused from the whole series:
 *   - CharLCD(...), begin(), clear(), setCursor(), print(const char*),
 *     print(int32_t), writeChar(), createChar() (project 4).
 *   - cursor()/blink() (project 2), applied live as a user-selectable
 *     setting rather than a fixed demo sequence.
 *   - display(bool) (project 2) — this time driven by the PD3 admin
 *     button to blank/wake the screen, its own contents preserved
 *     underneath exactly as project 2 demonstrated.
 *
 * Non-LCD concepts reused:
 *   - Button (examples/Modules/Button) for the menu button, and the
 *     PLAIN-GPIO, hand-timed pattern (examples/Modules/Button/06) for
 *     the screen-toggle button — the same "Button for rich gestures,
 *     manual GPIO+millis() for one simple hold-timed action" split that
 *     project made explicit.
 *   - The Timer0-overflow millis() clock (examples/timer/02), driving
 *     both the non-blocking Button::update() cadence and the screen-
 *     toggle button's 1.5 s hold timer.
 *   - EEPROMDriver (mikroduino/eeprom.hpp) with the same 0xA5-magic-byte
 *     "first boot" pattern used by examples/Modules/Button/06's
 *     combination lock, this time storing UI preferences instead of a
 *     secret code.
 */

#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/timer.hpp>
#include <mikroduino/eeprom.hpp>
#include <mikroduino/registers.hpp>
#include <Button.hpp>
#include <LCD.hpp>

using namespace MikroDuino;

static constexpr uint8_t BACKLIGHT_PIN     = PC1;
static constexpr uint8_t SCREEN_TOGGLE_PIN = PD3;

CharLCD lcd(PB0, PB1, PB2, PB3, PB4, PB5);
Button  menuButton(PD2);

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

// ── Icons for the "Icon Demo" setting ───────────────────────────────────

static const uint8_t ICON_HEART[8] = {
    0b00000,0b01010,0b11111,0b11111,0b01110,0b00100,0b00000,0b00000,
};
static const uint8_t ICON_SMILEY[8] = {
    0b00000,0b01010,0b01010,0b00000,0b10001,0b01110,0b00000,0b00000,
};
static const uint8_t ICON_ARROW[8] = {
    0b00000,0b00100,0b00010,0b11111,0b00010,0b00100,0b00000,0b00000,
};
static constexpr uint8_t ICON_COUNT = 3;
static const char ICON_NAME_0[] PROGMEM = "Heart";
static const char ICON_NAME_1[] PROGMEM = "Smiley";
static const char ICON_NAME_2[] PROGMEM = "Arrow";
static const char* const ICON_NAMES[ICON_COUNT] PROGMEM = { ICON_NAME_0, ICON_NAME_1, ICON_NAME_2 };

// ── Settings, EEPROM-backed ─────────────────────────────────────────────

struct Settings {
    uint8_t magic;
    uint8_t backlightOn;   // 0/1
    uint8_t cursorStyle;   // 0=Off 1=Cursor 2=Blink 3=Both
};

static constexpr uint16_t EEPROM_ADDR = 0x000;
static constexpr uint8_t  MAGIC       = 0xA5;

static Settings g_settings;

static void applyBacklight() {
    GPIO::write(BACKLIGHT_PIN, g_settings.backlightOn != 0);
}

static void applyCursorStyle() {
    lcd.cursor(g_settings.cursorStyle == 1 || g_settings.cursorStyle == 3);
    lcd.blink(g_settings.cursorStyle == 2 || g_settings.cursorStyle == 3);
}

static void saveSettings() {
    EEPROM.update(EEPROM_ADDR, g_settings);   // skips the write if unchanged
}

static void loadOrInitSettings() {
    g_settings = EEPROM.get<Settings>(EEPROM_ADDR);
    if (g_settings.magic != MAGIC) {
        g_settings.magic       = MAGIC;
        g_settings.backlightOn = 1;
        g_settings.cursorStyle = 0;
        saveSettings();
    }
    applyBacklight();
    applyCursorStyle();
}

// ── Menu state ───────────────────────────────────────────────────────────

static constexpr uint8_t ITEM_COUNT = 3;
static const char ITEM_NAME_0[] PROGMEM = "Backlight";
static const char ITEM_NAME_1[] PROGMEM = "Cursor";
static const char ITEM_NAME_2[] PROGMEM = "Icon Demo";
static const char* const ITEM_NAMES[ITEM_COUNT] PROGMEM = { ITEM_NAME_0, ITEM_NAME_1, ITEM_NAME_2 };

static const char CURSOR_STYLE_OFF[]    PROGMEM = "Off";
static const char CURSOR_STYLE_CURSOR[] PROGMEM = "Cursor";
static const char CURSOR_STYLE_BLINK[]  PROGMEM = "Blink";
static const char CURSOR_STYLE_BOTH[]   PROGMEM = "Both";
static const char* const CURSOR_STYLE_NAMES[4] PROGMEM = {
    CURSOR_STYLE_OFF, CURSOR_STYLE_CURSOR, CURSOR_STYLE_BLINK, CURSOR_STYLE_BOTH
};

static uint8_t g_topIndex  = 0;
static uint8_t g_iconIndex = 0;

static void printProgmemName(const char* const table[], uint8_t index) {
    const char* p = reinterpret_cast<const char*>(pgm_read_word(&table[index]));
    lcd.print(p);
}

static void redrawMenu() {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(">");
    printProgmemName(ITEM_NAMES, g_topIndex);

    lcd.setCursor(0, 1);
    switch (g_topIndex) {
        case 0:
            lcd.print("Backlight: ");
            lcd.print(g_settings.backlightOn ? "ON" : "OFF");
            break;
        case 1:
            lcd.print("Style: ");
            printProgmemName(CURSOR_STYLE_NAMES, g_settings.cursorStyle);
            break;
        case 2:
            lcd.writeChar(g_iconIndex);
            lcd.print(" ");
            printProgmemName(ICON_NAMES, g_iconIndex);
            break;
    }
}

int main() {
    GPIO::output(BACKLIGHT_PIN);
    GPIO::inputPullup(SCREEN_TOGGLE_PIN);

    lcd.begin();
    lcd.createChar(0, ICON_HEART);
    lcd.createChar(1, ICON_SMILEY);
    lcd.createChar(2, ICON_ARROW);

    menuButton.begin();

    Timer0.mode(TimerMode::Normal);
    Timer0.prescaler(TimerPrescaler::DIV64);
    Timer0.start();
    Timer0.enableOverflow();
    sei();

    loadOrInitSettings();
    redrawMenu();

    uint32_t lastUpdateMs = millis();

    bool     screenLastLow  = false;
    uint32_t screenDownSince = 0;
    bool     screenToggled   = false;
    bool     screenOn        = true;

    static constexpr uint16_t RESET_HOLD_MS = 1500;

    while (true) {
        uint32_t now = millis();

        while (now - lastUpdateMs >= 1) {
            menuButton.update();
            ++lastUpdateMs;
        }

        // -------------------------------------------------------------
        // Screen blank/wake — plain GPIO, hand-timed, no Button object
        // (same rationale as examples/Modules/Button/06's admin button).
        // -------------------------------------------------------------
        bool screenLow = (GPIO::read(SCREEN_TOGGLE_PIN) == false);
        if (screenLow && !screenLastLow) {
            screenDownSince = now;
            screenToggled   = false;
        }
        if (screenLow && !screenToggled && (now - screenDownSince >= 1500)) {
            screenToggled = true;
            screenOn = !screenOn;
            lcd.display(screenOn);   // content underneath is untouched either way
        }
        screenLastLow = screenLow;

        // -------------------------------------------------------------
        // Menu navigation — pressed()/released() are drained every cycle
        // even though neither has dedicated behaviour here: the
        // progress bar below reacts to isDown()/heldMs() directly, so
        // nothing needs the press/release edges themselves, but polling
        // them keeps both one-shot flags from sitting stale.
        // -------------------------------------------------------------
        menuButton.pressed();
        menuButton.released();

        if (menuButton.clicked()) {
            g_topIndex = static_cast<uint8_t>((g_topIndex + 1) % ITEM_COUNT);
            redrawMenu();
        }

        if (menuButton.doubleClicked()) {
            switch (g_topIndex) {
                case 0:
                    g_settings.backlightOn = g_settings.backlightOn ? 0 : 1;
                    applyBacklight();
                    saveSettings();
                    break;
                case 1:
                    g_settings.cursorStyle = static_cast<uint8_t>((g_settings.cursorStyle + 1) % 4);
                    applyCursorStyle();
                    saveSettings();
                    break;
                case 2:
                    g_iconIndex = static_cast<uint8_t>((g_iconIndex + 1) % ICON_COUNT);
                    break;
            }
            redrawMenu();
        }

        // -------------------------------------------------------------
        // Long-press: reset everything to defaults, with a live LCD
        // progress bar while held — the on-screen counterpart to Button
        // project 3's LED arming-blink.
        // -------------------------------------------------------------
        if (menuButton.isDown() && menuButton.heldMs() > 0) {
            uint16_t held = menuButton.heldMs();
            uint8_t filled = static_cast<uint8_t>(
                (static_cast<uint32_t>(held > RESET_HOLD_MS ? RESET_HOLD_MS : held) * 10) / RESET_HOLD_MS
            );

            // Only touch the LCD when the bar actually has a new cell to
            // show — this loop pass runs far more often than the bar's
            // 10-step resolution changes, and every LCD write costs a
            // handful of microsecond-scale pulses (see LCD.cpp's
            // pulse()), so redrawing on every pass would mean spending
            // most of main()'s time re-sending an unchanged bar.
            static uint8_t lastFilled = 0xFF;
            if (filled != lastFilled) {
                lastFilled = filled;
                lcd.setCursor(0, 1);
                lcd.writeChar('[');
                for (uint8_t i = 0; i < 10; ++i) lcd.writeChar(i < filled ? '#' : ' ');
                lcd.writeChar(']');
            }
        }

        if (menuButton.longPressed()) {
            g_settings.backlightOn = 1;
            g_settings.cursorStyle = 0;
            applyBacklight();
            applyCursorStyle();
            saveSettings();

            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Reset to");
            lcd.setCursor(0, 1);
            lcd.print("defaults!");
            _delay_ms(1200);

            redrawMenu();
        }
    }
}
