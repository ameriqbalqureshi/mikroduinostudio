/*
 * Button Single-Button Menu — doubleClicked() + setDoubleClickMs() —
 * MikroDuino Module SDK
 *
 * Project 4 of 6 in the examples/Modules/Button series. Builds a small
 * two-level menu — the kind found in cheap single-button devices
 * (digital watches, basic thermostats) — driven entirely by one button
 * and all three "what kind of press was that" gestures at once:
 *
 *   clicked()       -> move to the next item / adjust a value
 *   doubleClicked() -> select an item / confirm-and-save a value
 *   longPressed()   -> cancel / back out to the top level
 *
 * Hardware: identical to projects 1-3 — button on PD2, USART0 at 9600
 * 8N1 on PD1. (No LED this time — the "display" is the USART log; a
 * real product would show this menu on an LCD/OLED, both covered by
 * later module series in this SDK.)
 *
 * Button concepts introduced:
 *   - doubleClicked() — a one-shot event, true when two clicked()-style
 *     press/release cycles happen within doubleClickMs of each other. As
 *     with clicked() vs. longPressed(), Button resolves the ambiguity
 *     for you: a first click doesn't fire clicked() immediately — it
 *     WAITS out the double-click window to see if a second click
 *     follows, and reports exactly one of clicked() or doubleClicked(),
 *     never both, for the same gesture. That wait is also why clicked()
 *     always arrives slightly after the physical release, not
 *     instantaneously — see this project's console output for how that
 *     feels in practice.
 *   - setDoubleClickMs(ms) — reconfigures the double-click window. The
 *     350 ms default suits a general-purpose button; this project widens
 *     it to 500 ms, a deliberate trade-off explained below.
 *
 * Button concepts reused from projects 1-3:
 *   - Button(pin), begin(), update(), clicked(), longPressed().
 *
 * The setDoubleClickMs() trade-off: widening the window makes
 * doubleClicked() easier to land reliably, but it also means two
 * DELIBERATELY separate, fairly quick single clicks (e.g. rapidly
 * paging through a long list with clicked()) become more likely to be
 * misread as one doubleClicked(). 500 ms is a reasonable middle ground
 * for a slow, deliberate menu like this one; a fast-paging UI would want
 * to keep the window closer to the default.
 */

#include <avr/pgmspace.h>
#include <util/delay.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/usart.hpp>
#include <Button.hpp>

using namespace MikroDuino;

Button button(PD2);

// ── Top-level menu ───────────────────────────────────────────────────────

static constexpr uint8_t ITEM_COUNT = 4;
static const char ITEM_BRIGHTNESS[] PROGMEM = "Brightness";
static const char ITEM_SPEED[]      PROGMEM = "Speed";
static const char ITEM_MODE_A[]     PROGMEM = "Mode A (no submenu)";
static const char ITEM_MODE_B[]     PROGMEM = "Mode B (no submenu)";
static const char* const ITEM_NAMES[ITEM_COUNT] PROGMEM = {
    ITEM_BRIGHTNESS, ITEM_SPEED, ITEM_MODE_A, ITEM_MODE_B
};

static void printItemName(uint8_t index) {
    const char* p = reinterpret_cast<const char*>(pgm_read_word(&ITEM_NAMES[index]));
    USART0.write_P(p);
}

enum class MenuState : uint8_t { TopLevel, AdjustBrightness, AdjustSpeed };

static MenuState g_state = MenuState::TopLevel;
static uint8_t   g_topIndex = 0;

static uint8_t g_brightness = 50;   // 0-100, in steps of 10
static uint8_t g_speed      = 3;    // 1-5

static void printTopLevel() {
    USART0.write_P(PSTR("Menu: "));
    printItemName(g_topIndex);
    USART0.writeLine_P(PSTR("   (click=next, double-click=select)"));
}

int main() {
    button.begin();
    button.setDoubleClickMs(500);   // widen from the 350 ms default — see header comment

    USART0.begin(9600);
    USART0.writeLine_P(PSTR("Single-button menu (double-click select, long-press back)"));
    USART0.writeLine_P(PSTR("============================================================"));
    printTopLevel();

    while (true) {
        button.update();

        switch (g_state) {
            case MenuState::TopLevel:
                if (button.clicked()) {
                    g_topIndex = static_cast<uint8_t>((g_topIndex + 1) % ITEM_COUNT);
                    printTopLevel();
                }
                if (button.doubleClicked()) {
                    if (g_topIndex == 0) {
                        g_state = MenuState::AdjustBrightness;
                        USART0.write_P(PSTR("-> Adjust Brightness: "));
                        USART0.writeInt(g_brightness);
                        USART0.writeLine_P(PSTR("  (click=+10, double-click=save, long-press=cancel)"));
                    } else if (g_topIndex == 1) {
                        g_state = MenuState::AdjustSpeed;
                        USART0.write_P(PSTR("-> Adjust Speed: "));
                        USART0.writeInt(g_speed);
                        USART0.writeLine_P(PSTR("  (click=+1, double-click=save, long-press=cancel)"));
                    } else {
                        USART0.write_P(PSTR("(\""));
                        printItemName(g_topIndex);
                        USART0.writeLine_P(PSTR("\" has no submenu)"));
                    }
                }
                if (button.longPressed()) {
                    g_topIndex = 0;
                    USART0.writeLine_P(PSTR("Long-press at top level -> reset to first item"));
                    printTopLevel();
                }
                break;

            case MenuState::AdjustBrightness:
                if (button.clicked()) {
                    g_brightness = static_cast<uint8_t>((g_brightness + 10) % 110);
                    USART0.write_P(PSTR("  brightness="));
                    USART0.writeInt(g_brightness);
                    USART0.writeLine_P(PSTR(""));
                }
                if (button.doubleClicked()) {
                    USART0.write_P(PSTR("Saved brightness="));
                    USART0.writeInt(g_brightness);
                    USART0.writeLine_P(PSTR(" -> back to menu"));
                    g_state = MenuState::TopLevel;
                    printTopLevel();
                }
                if (button.longPressed()) {
                    USART0.writeLine_P(PSTR("Cancelled -> back to menu (brightness unchanged)"));
                    g_state = MenuState::TopLevel;
                    printTopLevel();
                }
                break;

            case MenuState::AdjustSpeed:
                if (button.clicked()) {
                    g_speed = static_cast<uint8_t>((g_speed % 5) + 1);   // wraps 1..5
                    USART0.write_P(PSTR("  speed="));
                    USART0.writeInt(g_speed);
                    USART0.writeLine_P(PSTR(""));
                }
                if (button.doubleClicked()) {
                    USART0.write_P(PSTR("Saved speed="));
                    USART0.writeInt(g_speed);
                    USART0.writeLine_P(PSTR(" -> back to menu"));
                    g_state = MenuState::TopLevel;
                    printTopLevel();
                }
                if (button.longPressed()) {
                    USART0.writeLine_P(PSTR("Cancelled -> back to menu (speed unchanged)"));
                    g_state = MenuState::TopLevel;
                    printTopLevel();
                }
                break;
        }

        _delay_ms(1);
    }
}
