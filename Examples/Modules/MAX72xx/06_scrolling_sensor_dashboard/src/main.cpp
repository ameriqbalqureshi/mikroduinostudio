/*
 * MAX72xx Scrolling Sensor Dashboard — Potentiometer Bargraph + Live
 * Scrolling Ticker — MikroDuino Module SDK (capstone)
 *
 * Project 6 of 6 in the examples/Modules/MAX72xx series. Everything the
 * previous five projects introduced, combined into one small live
 * instrument: a potentiometer's ADC reading is shown two different ways
 * on the same 32-column matrix, switchable at any time with a button —
 * BAR mode redraws project 2's drawFilledRect() every sample as a
 * classic level-meter bargraph, TICKER mode re-arms project 5's
 * non-blocking scroll with a freshly formatted "VAL:nnn" string every
 * time the previous pass finishes. USART mirrors both the current mode
 * and the raw sensor value, the same secondary-dashboard role every
 * other module series' capstone gives it.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ DIN     │ D11   │ MOSI                                        │
 *   │ CLK     │ D13   │ SCK                                          │
 *   │ CS      │ D10   │ Any free digital pin                        │
 *   │ MODE BTN│ D2    │ Other leg to GND — internal pull-up enabled │
 *   │ POT     │ A0    │ Wiper of a 10k potentiometer (or any 0-5V   │
 *   │         │       │ analog sensor) — ends to VCC/GND             │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * MAX72xx concepts introduced:
 *   - Mixing modes on ONE MatrixDisplay instance: BAR mode calls
 *     drawFilledRect() every sample period; TICKER mode drives
 *     beginScroll()/updateScroll(). Switching modes just clears the
 *     display and starts using different methods on the next loop() —
 *     MatrixDisplay itself has no concept of "mode", the state machine
 *     is entirely this project's own, the same "one shared object,
 *     multiple caller-defined states" shape IRArray/06 and DS3231/06 use
 *     for their own capstones.
 *   - A pointer-lifetime rule beginScroll() has that no earlier project
 *     needed to worry about: beginScroll(text, ...) stores the POINTER it
 *     is given (see MAX72xx.hpp/.cpp — _scroll.p = text), not a copy. This
 *     project's ticker text lives in a single static buffer that is only
 *     ever rewritten from armTicker(), which is only ever called once the
 *     PREVIOUS scroll pass has fully finished (updateScroll() returned
 *     false) or on a fresh mode switch — never while a scroll driven from
 *     that same buffer is still in flight. Rewriting the buffer mid-scroll
 *     would visibly corrupt the characters already queued to shift out.
 *   - A defensive minimum bar width of 1: drawFilledRect(row, col, height,
 *     0) hands the library's drawHLine() a zero-width span, which
 *     computes an end column of col-1 — for col=0 that underflows the
 *     unsigned column type to 65535 and makes drawHLine() loop far past
 *     the real 32-column display. Clamping the mapped bar width to at
 *     least 1 column (instead of letting a near-zero pot reading produce
 *     a literal 0) avoids handing the library that input at all.
 */

#include <Arduino.h>
#include <SPI.h>
#include <MD_MAX72xx.h>
#include <MAX72xx.hpp>
#include <string.h>

using namespace MikroDuino;

ARDUINO_MAIN()

static constexpr uint8_t  CS_PIN      = 10;
static constexpr uint8_t  NUM_MODULES =  4;
static constexpr uint8_t  BUTTON_PIN  =  2;
static constexpr uint8_t  POT_PIN     = A0;

static constexpr uint16_t DEBOUNCE_MS      = 200;
static constexpr uint16_t BAR_REFRESH_MS   = 100;
static constexpr uint16_t SCROLL_DELAY_MS  = 40;
static constexpr uint16_t SERIAL_PERIOD_MS = 500;

MatrixDisplay mx(MatrixDisplay::FC16, CS_PIN, NUM_MODULES);

enum Mode : uint8_t { MODE_BAR, MODE_TICKER };

static Mode     g_mode          = MODE_BAR;
static bool     g_lastButton    = false;
static uint32_t g_lastChangeMs  = 0;
static uint32_t g_lastSampleMs  = 0;
static uint32_t g_lastPrintMs   = 0;

static char g_tickerBuf[12];   // "VAL:" + up to 4 digits + 2-space gap + terminator

static void buildTickerText(uint16_t value) {
    char num[5];
    itoa(value, num, 10);
    strcpy(g_tickerBuf, "VAL:");
    strcat(g_tickerBuf, num);
    strcat(g_tickerBuf, "  ");
}

// Sample the pot fresh and (re)arm the scroll. Only ever called when no
// scroll driven from g_tickerBuf is currently in flight — see header comment.
static void armTicker() {
    buildTickerText(static_cast<uint16_t>(analogRead(POT_PIN)));
    mx.beginScroll(g_tickerBuf, SCROLL_DELAY_MS);
}

static void enterMode(Mode m) {
    g_mode = m;
    mx.clear();
    Serial.print("MODE: ");
    Serial.println(m == MODE_BAR ? "BAR" : "TICKER");
    if (m == MODE_TICKER) armTicker();
}

void setup() {
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    mx.begin();
    mx.setBrightness(6);

    Serial.begin(9600);
    Serial.println("MAX72xx scrolling sensor dashboard");
    Serial.println("Click the button to switch BAR <-> TICKER");

    Arduino::beginTimekeeping();

    enterMode(MODE_BAR);
}

void loop() {
    uint32_t now = millis();

    // ---- Button: cycle BAR <-> TICKER, debounced against millis() ----
    bool pressed = (digitalRead(BUTTON_PIN) == LOW);
    if (pressed && !g_lastButton && (now - g_lastChangeMs) >= DEBOUNCE_MS) {
        enterMode(g_mode == MODE_BAR ? MODE_TICKER : MODE_BAR);
        g_lastChangeMs = now;
    }
    g_lastButton = pressed;

    // ---- Mode-specific display update ----
    if (g_mode == MODE_BAR) {
        if (now - g_lastSampleMs >= BAR_REFRESH_MS) {
            g_lastSampleMs = now;

            uint16_t pot = static_cast<uint16_t>(analogRead(POT_PIN));
            uint16_t totalCols = mx.columnCount();
            uint8_t  barWidth  = static_cast<uint8_t>(
                (static_cast<uint32_t>(pot) * totalCols) / 1024);
            if (barWidth == 0) barWidth = 1;   // see header comment: never pass width 0

            mx.clear();
            mx.drawFilledRect(0, 0, 8, barWidth);
        }
    } else {   // MODE_TICKER
        if (!mx.updateScroll()) {
            armTicker();   // pass finished: resample and start the next one
        }
    }

    // ---- Serial mirror ----
    if (now - g_lastPrintMs >= SERIAL_PERIOD_MS) {
        g_lastPrintMs = now;
        Serial.print("value=");
        Serial.println(analogRead(POT_PIN));
    }
}
