/*
 * MAX72xx — Blocking Scroll: Cycling Marquee Messages — MikroDuino
 * Module SDK
 *
 * Project 4 of 6 in the examples/Modules/MAX72xx series. Projects 1-3 all
 * drew a single static frame and left it on screen. This project
 * introduces MatrixDisplay's other text primitive — scrollText(), a
 * BLOCKING marquee that scrolls a whole message leftward across the
 * display one pixel at a time — and cycles through a short playlist of
 * messages, briefly blanking the display between each one with
 * setEnabled() so the boundary between messages is unambiguous.
 *
 * Hardware — identical wiring to project 1:
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ DIN     │ D11   │ MOSI                                        │
 *   │ CLK     │ D13   │ SCK                                          │
 *   │ CS      │ D10   │ Any free digital pin                        │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * MAX72xx concepts introduced:
 *   - scrollText(text, delayMs) — scrolls the WHOLE string across the
 *     display and returns only once it has fully scrolled off the left
 *     edge. It needs no timekeeping (see its header comment: it uses
 *     _delay_ms() internally, one millisecond at a time), which is
 *     exactly why it is BLOCKING — nothing else in this project's loop()
 *     can run while a message is scrolling. That trade-off is deliberate
 *     for this project (there IS nothing else to run); project 5
 *     replaces it with the non-blocking beginScroll()/updateScroll() pair
 *     specifically to lift that restriction.
 *   - setEnabled(false) / setEnabled(true) — drives the MAX72XX chip's
 *     own hardware shutdown mode, a true LED-off blackout (not just
 *     clearing pixel data) used here as a brief pulse between messages so
 *     one message's scroll-off and the next message's scroll-in never
 *     visually run together.
 */

#include <Arduino.h>
#include <SPI.h>
#include <MD_MAX72xx.h>
#include <MAX72xx.hpp>

using namespace MikroDuino;

ARDUINO_MAIN()

static constexpr uint8_t  CS_PIN        = 10;
static constexpr uint8_t  NUM_MODULES   =  4;
static constexpr uint16_t SCROLL_DELAY_MS = 40;   // ms between each 1-pixel shift
static constexpr uint16_t BLACKOUT_MS     = 250;  // pause between messages

MatrixDisplay mx(MatrixDisplay::FC16, CS_PIN, NUM_MODULES);

static const char* const MESSAGES[] = {
    "HELLO",
    "WORLD",
    "MAX7219 MARQUEE",
    "MIKRODUINO SDK",
};
static constexpr uint8_t NUM_MESSAGES = sizeof(MESSAGES) / sizeof(MESSAGES[0]);

void setup() {
    mx.begin();
    mx.setBrightness(5);
}

void loop() {
    for (uint8_t i = 0; i < NUM_MESSAGES; i++) {
        mx.scrollText(MESSAGES[i], SCROLL_DELAY_MS);

        mx.setEnabled(false);
        delay(BLACKOUT_MS);
        mx.setEnabled(true);
    }
}
