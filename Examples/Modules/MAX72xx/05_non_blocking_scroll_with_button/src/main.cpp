/*
 * MAX72xx — Non-Blocking Scroll + Brightness Button — MikroDuino Module SDK
 *
 * Project 5 of 6 in the examples/Modules/MAX72xx series. Project 4's
 * scrollText() blocked the entire program for as long as a message took
 * to scroll — fine when scrolling is the ONLY thing happening, but not
 * once a project needs to react to anything else while a message is in
 * motion. This project switches to MatrixDisplay's non-blocking scroll
 * pair — beginScroll() once, updateScroll() every loop() — so a button
 * can cycle the display brightness through four preset levels WHILE the
 * marquee keeps scrolling continuously, never pausing to service the
 * button.
 *
 * Hardware — identical wiring to project 1, plus a button:
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ DIN     │ D11   │ MOSI                                        │
 *   │ CLK     │ D13   │ SCK                                          │
 *   │ CS      │ D10   │ Any free digital pin                        │
 *   │ BTN     │ D2    │ Other leg to GND — internal pull-up enabled │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * MAX72xx concepts introduced:
 *   - Arduino::beginTimekeeping() — beginScroll()/updateScroll() are
 *     documented (see MAX72xx.hpp) to REQUIRE millis(), which on this
 *     SDK's Arduino compatibility layer does nothing until this call
 *     enables the underlying Timer0 overflow interrupt. Forgetting it is
 *     the single most common mistake with the non-blocking scroll API —
 *     updateScroll() would silently never advance.
 *   - beginScroll(text, delayMs) — arms the scroll state machine once;
 *     unlike scrollText(), it returns immediately.
 *   - updateScroll() — called every loop() iteration. Internally it
 *     checks millis() itself and only actually shifts a pixel once
 *     delayMs has elapsed since the last shift, so calling it far more
 *     often than that (as this project's tight loop() does) costs
 *     nothing — the same "call it constantly, let it decide when to act"
 *     shape as this SDK's own SoftTimer::expired(). It returns true while
 *     still scrolling and false once the message has fully scrolled off
 *     — at which point this project immediately re-arms it with
 *     beginScroll() again, producing a continuously repeating marquee.
 *   - A hand-timed button debounce using millis() rather than a
 *     _delay_ms() poll — the whole point of this project is that nothing
 *     may block, so the debounce below is a plain "ignore further edges
 *     for DEBOUNCE_MS after the last accepted one" check against the same
 *     millis() clock beginTimekeeping() just enabled, not a separate
 *     timing mechanism.
 */

#include <Arduino.h>
#include <SPI.h>
#include <MD_MAX72xx.h>
#include <MAX72xx.hpp>

using namespace MikroDuino;

ARDUINO_MAIN()

static constexpr uint8_t  CS_PIN      = 10;
static constexpr uint8_t  NUM_MODULES =  4;
static constexpr uint8_t  BUTTON_PIN  =  2;
static constexpr uint16_t SCROLL_DELAY_MS = 40;
static constexpr uint16_t DEBOUNCE_MS     = 200;

MatrixDisplay mx(MatrixDisplay::FC16, CS_PIN, NUM_MODULES);

static const char MARQUEE_TEXT[] = "NON-BLOCKING SCROLL DEMO  ";  // trailing gap before it repeats

static const uint8_t BRIGHTNESS_LEVELS[] = { 1, 5, 9, 14 };
static constexpr uint8_t NUM_LEVELS = sizeof(BRIGHTNESS_LEVELS) / sizeof(BRIGHTNESS_LEVELS[0]);

static uint8_t  g_levelIndex   = 1;   // start at BRIGHTNESS_LEVELS[1]
static bool     g_lastButton   = false;
static uint32_t g_lastChangeMs = 0;

void setup() {
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    mx.begin();
    mx.setBrightness(BRIGHTNESS_LEVELS[g_levelIndex]);

    Arduino::beginTimekeeping();   // required before beginScroll()/updateScroll() or millis()

    mx.beginScroll(MARQUEE_TEXT, SCROLL_DELAY_MS);
}

void loop() {
    // ---- Keep the marquee moving: re-arm the instant one pass finishes ----
    if (!mx.updateScroll()) {
        mx.beginScroll(MARQUEE_TEXT, SCROLL_DELAY_MS);
    }

    // ---- Button: cycle brightness, debounced against millis(), never blocking ----
    bool pressed = (digitalRead(BUTTON_PIN) == LOW);
    uint32_t now = millis();

    if (pressed && !g_lastButton && (now - g_lastChangeMs) >= DEBOUNCE_MS) {
        g_levelIndex   = static_cast<uint8_t>((g_levelIndex + 1) % NUM_LEVELS);
        mx.setBrightness(BRIGHTNESS_LEVELS[g_levelIndex]);
        g_lastChangeMs = now;
    }
    g_lastButton = pressed;
}
