/*
 * On-Screen Settings Menu Driven by a Rotary Encoder — RotaryEncoder +
 * LCD + EEPROM — MikroDuino Module SDK (capstone)
 *
 * Project 6 of 6 in the examples/Modules/RotaryEncoder series. The
 * classic "one knob does everything" UI: rotate to move a highlight
 * between settings, click to enter/exit editing a setting, rotate
 * again while editing to change its value. This is the same on-screen
 * menu idea as examples/Modules/LCD/06_settings_menu, but that project
 * used a plain push button (clicked()/doubleClicked()/longPressed())
 * for BOTH navigation and value changes — with only one button, "move
 * the highlight" and "change the value" had to be two different click
 * patterns on the same control. A rotary encoder removes that
 * ambiguity entirely: rotation always means "change something,"
 * clicking always means "confirm and move to the next mode," and
 * which THING rotation changes is just whichever mode you're in.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌──────────────┬───────┬──────────────────────────────────────────┐
 *   │ RS           │ PB0   │ LCD (4-bit mode, same wiring as the LCD   │
 *   │ EN           │ PB1   │ series' projects 1-6)                     │
 *   │ D4-D7        │ PB2-5 │                                            │
 *   │ Backlight    │ PC1   │ Drives a backlight transistor/MOSFET —    │
 *   │              │       │ HIGH = backlight on                        │
 *   │ CH_A         │ PD2   │ Encoder channel A, internal pull-up        │
 *   │ CH_B         │ PD3   │ Encoder channel B, internal pull-up        │
 *   │ SW           │ PD4   │ Encoder's integrated push button            │
 *   └──────────────┴───────┴──────────────────────────────────────────┘
 *
 * Menu modes:
 *   NAVIGATE — rotate moves the highlighted setting up/down (cyclic);
 *              click enters EDIT for the highlighted setting, except
 *              "Reset Defaults", which has no value to edit and just
 *              fires immediately on click.
 *   EDIT     — rotate changes the highlighted setting's value live;
 *              click commits it to EEPROM and returns to NAVIGATE.
 *   Hold >= 1.5s in EITHER mode resets every setting to its default
 *   and returns to NAVIGATE, with a live progress bar on row 1 while
 *   held — the same "arming" feedback idea as LCD project 6's
 *   long-press reset and Button project 3's LED version of it, this
 *   time reused a third time as an on-screen bar.
 *
 * Settings, each with a real effect:
 *   Backlight — ON/OFF, applied to the PC1 transistor immediately.
 *   Timeout   — 5-60 s (5 s steps): how long the backlight stays lit
 *               after the last rotation or click before auto-dimming.
 *               Any new activity turns it back on. This is the
 *               project's only always-running background behaviour,
 *               so it's driven from the same millis() clock as the
 *               menu's own button debouncing rather than a blocking
 *               delay, exactly like examples/Modules/HCSR04/04's two
 *               independent millis()-scheduled jobs.
 *
 * RotaryEncoder concepts reused from the whole series:
 *   - RotaryEncoder(pinA, pinB, pinBtn), begin(), _isrHandler() routed
 *     from ISR(PCINT2_vect) (project 1).
 *   - direction() as the single "something changed by one step, which
 *     way" signal driving BOTH menu navigation and value edits — just
 *     interpreted differently depending on g_mode (project 2).
 *   - updateButton() on a millis() cadence, buttonReleased() used as
 *     the click gesture, buttonIsDown()/buttonHeldMs() for the
 *     long-press reset (project 3).
 *
 * Non-RotaryEncoder concepts reused:
 *   - CharLCD(...), begin(), clear(), setCursor(), print() (LCD series).
 *   - EEPROMDriver with the same 0xA5-magic-byte pattern used by
 *     project 5 and examples/Modules/LCD/06.
 *   - The Timer0-overflow millis() clock (examples/timer/02).
 */

#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/timer.hpp>
#include <mikroduino/eeprom.hpp>
#include <mikroduino/registers.hpp>
#include <RotaryEncoder.hpp>
#include <LCD.hpp>

using namespace MikroDuino;

static constexpr uint8_t BACKLIGHT_PIN = PC1;

CharLCD       lcd(PB0, PB1, PB2, PB3, PB4, PB5);
RotaryEncoder encoder(PD2, PD3, PD4);

ISR(PCINT2_vect) { encoder._isrHandler(); }

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
    uint8_t backlightOn;   // 0/1
    uint8_t timeoutSec;    // 5-60
};

static constexpr uint16_t EEPROM_ADDR      = 0x000;
static constexpr uint8_t  MAGIC            = 0xA5;
static constexpr uint8_t  DEFAULT_TIMEOUT  = 15;

static Settings g_settings;

static void applyBacklight() {
    GPIO::write(BACKLIGHT_PIN, g_settings.backlightOn != 0);
}

static void saveSettings() {
    EEPROM.update(EEPROM_ADDR, g_settings);   // skips the write if unchanged
}

static void loadOrInitSettings() {
    g_settings = EEPROM.get<Settings>(EEPROM_ADDR);
    if (g_settings.magic != MAGIC) {
        g_settings.magic       = MAGIC;
        g_settings.backlightOn = 1;
        g_settings.timeoutSec  = DEFAULT_TIMEOUT;
        saveSettings();
    }
    applyBacklight();
}

// ── Menu state ───────────────────────────────────────────────────────────

enum class Mode : uint8_t { NAVIGATE, EDIT };

static constexpr uint8_t ITEM_COUNT = 3;
static const char ITEM_NAME_0[] PROGMEM = "Backlight";
static const char ITEM_NAME_1[] PROGMEM = "Timeout (s)";
static const char ITEM_NAME_2[] PROGMEM = "Reset Defaults";
static const char* const ITEM_NAMES[ITEM_COUNT] PROGMEM = { ITEM_NAME_0, ITEM_NAME_1, ITEM_NAME_2 };

static Mode    g_mode     = Mode::NAVIGATE;
static uint8_t g_topIndex = 0;

static void printProgmemName(uint8_t index) {
    const char* p = reinterpret_cast<const char*>(pgm_read_word(&ITEM_NAMES[index]));
    lcd.print(p);
}

static void redrawMenu() {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(g_mode == Mode::EDIT ? "*" : ">");
    printProgmemName(g_topIndex);

    lcd.setCursor(0, 1);
    switch (g_topIndex) {
        case 0:
            lcd.print("Value: ");
            lcd.print(g_settings.backlightOn ? "ON" : "OFF");
            break;
        case 1:
            lcd.print("Value: ");
            lcd.print(static_cast<int32_t>(g_settings.timeoutSec));
            break;
        case 2:
            lcd.print("(click to reset)");
            break;
    }
}

int main() {
    GPIO::output(BACKLIGHT_PIN);

    lcd.begin();
    encoder.begin();

    Timer0.mode(TimerMode::Normal);
    Timer0.prescaler(TimerPrescaler::DIV64);
    Timer0.start();
    Timer0.enableOverflow();
    sei();

    loadOrInitSettings();
    redrawMenu();

    uint32_t lastUpdateMs   = millis();
    uint32_t lastActivityMs = millis();
    bool     resetArmed     = false;

    static constexpr uint16_t RESET_HOLD_MS = 1500;

    while (true) {
        uint32_t now = millis();

        while (now - lastUpdateMs >= 1) {
            encoder.updateButton();
            ++lastUpdateMs;
        }

        // -------------------------------------------------------------
        // Backlight auto-timeout — its own independent millis() job,
        // running whether or not the menu is being touched right now.
        // -------------------------------------------------------------
        bool backlightWantsOn = g_settings.backlightOn != 0 &&
            (now - lastActivityMs) < (static_cast<uint32_t>(g_settings.timeoutSec) * 1000UL);
        GPIO::write(BACKLIGHT_PIN, backlightWantsOn);

        // -------------------------------------------------------------
        // Rotation — meaning depends on the current mode.
        // -------------------------------------------------------------
        int8_t step = encoder.direction();
        if (step != 0) {
            lastActivityMs = now;

            if (g_mode == Mode::NAVIGATE) {
                int16_t next = static_cast<int16_t>(g_topIndex) + step;
                if (next < 0) next += ITEM_COUNT;
                if (next >= ITEM_COUNT) next -= ITEM_COUNT;
                g_topIndex = static_cast<uint8_t>(next);
            } else {   // EDIT
                switch (g_topIndex) {
                    case 0:
                        g_settings.backlightOn = g_settings.backlightOn ? 0 : 1;
                        break;
                    case 1: {
                        int16_t t = static_cast<int16_t>(g_settings.timeoutSec) + (step * 5);
                        if (t < 5)  t = 5;
                        if (t > 60) t = 60;
                        g_settings.timeoutSec = static_cast<uint8_t>(t);
                        break;
                    }
                    default:
                        break;
                }
            }
            redrawMenu();
        }

        // -------------------------------------------------------------
        // Click — confirm/enter, drained every pass so the flag never
        // sits stale even on passes with no rotation.
        // -------------------------------------------------------------
        encoder.buttonPressed();
        if (encoder.buttonReleased() && !resetArmed) {
            lastActivityMs = now;

            if (g_mode == Mode::NAVIGATE) {
                if (g_topIndex == 2) {
                    // "Reset Defaults" has no value to edit — act immediately.
                    g_settings.backlightOn = 1;
                    g_settings.timeoutSec  = DEFAULT_TIMEOUT;
                    saveSettings();
                    lcd.clear();
                    lcd.setCursor(0, 0);
                    lcd.print("Reset to");
                    lcd.setCursor(0, 1);
                    lcd.print("defaults!");
                    _delay_ms(1000);
                } else {
                    g_mode = Mode::EDIT;
                }
            } else {   // EDIT -> confirm and save
                saveSettings();
                applyBacklight();
                g_mode = Mode::NAVIGATE;
            }
            redrawMenu();
        }

        // -------------------------------------------------------------
        // Long hold, either mode: reset everything, with a live
        // progress bar on row 1 while held.
        // -------------------------------------------------------------
        if (encoder.buttonIsDown()) {
            uint16_t held = encoder.buttonHeldMs();

            if (held > 0) {
                uint8_t filled = static_cast<uint8_t>(
                    (static_cast<uint32_t>(held > RESET_HOLD_MS ? RESET_HOLD_MS : held) * 10) / RESET_HOLD_MS
                );
                static uint8_t lastFilled = 0xFF;
                if (filled != lastFilled) {
                    lastFilled = filled;
                    lcd.setCursor(0, 1);
                    lcd.writeChar('[');
                    for (uint8_t i = 0; i < 10; ++i) lcd.writeChar(i < filled ? '#' : ' ');
                    lcd.writeChar(']');
                }
            }

            if (!resetArmed && held >= RESET_HOLD_MS) {
                resetArmed = true;
                lastActivityMs = now;

                g_settings.backlightOn = 1;
                g_settings.timeoutSec  = DEFAULT_TIMEOUT;
                saveSettings();
                applyBacklight();
                g_mode = Mode::NAVIGATE;

                lcd.clear();
                lcd.setCursor(0, 0);
                lcd.print("Reset to");
                lcd.setCursor(0, 1);
                lcd.print("defaults!");
                _delay_ms(1000);
                redrawMenu();
            }
        } else {
            resetArmed = false;
        }
    }
}
