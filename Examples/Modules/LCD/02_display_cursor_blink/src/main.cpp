/*
 * LCD Display/Cursor/Blink Control — MikroDuino Module SDK
 *
 * Project 2 of 6 in the examples/Modules/LCD series. Same wiring as
 * project 1. Introduces the three independent on/off display attributes
 * — display(), cursor(), blink() — via a small typewriter effect, then
 * a "screen flash" using display() alone, then a running hex counter
 * via printHex() and the low-level writeChar().
 *
 * Hardware: identical to project 1 — RS=PB0, EN=PB1, D4-D7=PB2-PB5.
 *
 * LCD concepts introduced:
 *   - cursor(bool on) — shows/hides a permanent underline at the current
 *     cursor position (the HD44780's hardware cursor, not something
 *     CharLCD draws itself).
 *   - blink(bool on) — makes the character CELL at the cursor position
 *     blink (alternating between the character and a solid block),
 *     independent of cursor() — the two combine into four distinct
 *     looks: neither, underline only, blink only, or both together.
 *   - display(bool on) — the display attribute that differs most from
 *     what it sounds like: display(false) does NOT clear the screen's
 *     contents (that's clear()'s job) — it just stops driving the
 *     pixels, so whatever was on screen reappears unchanged the instant
 *     display(true) runs again. This project's "screen flash" relies
 *     exactly on that distinction.
 *   - writeChar(uint8_t c) — the primitive print() itself is built on:
 *     sends one raw byte to the LCD as DATA (as opposed to command()'s
 *     internal use for control bytes). print() is just a loop calling
 *     writeChar() once per character; this project calls it directly to
 *     add a per-character delay for the typewriter effect, something
 *     print()'s all-at-once loop doesn't offer.
 *   - printHex(uint8_t value) — writes exactly two hex digit characters
 *     (uppercase, no "0x" prefix) for a byte value — handy for register
 *     dumps or debug displays without hand-building the hex text first.
 *
 * LCD concepts reused from project 1:
 *   - CharLCD(...), begin(), clear(), setCursor(), print(const char*).
 */

#include <util/delay.h>
#include <LCD.hpp>

using namespace MikroDuino;

CharLCD lcd(PB0, PB1, PB2, PB3, PB4, PB5);

// avr-libc's _delay_ms() needs a compile-time constant argument to
// generate an accurate cycle-counted delay — it can't take a runtime
// variable directly. This one-millisecond-at-a-time loop (the same
// pattern used throughout this SDK's examples wherever a delay length
// is only known at runtime) sidesteps that restriction.
static void delay_ms(uint16_t ms) { while (ms--) _delay_ms(1); }

// Type out a message one character at a time with a visible cursor,
// using writeChar() directly instead of print() so a delay can be
// inserted between characters.
static void typewrite(const char* msg, uint16_t perCharMs) {
    while (*msg) {
        lcd.writeChar(static_cast<uint8_t>(*msg++));
        delay_ms(perCharMs);
    }
}

int main() {
    lcd.begin();

    // -----------------------------------------------------------------------
    // Typewriter effect: cursor + blink both on, so the insertion point is
    // clearly visible while text appears one letter at a time.
    // -----------------------------------------------------------------------
    lcd.cursor(true);
    lcd.blink(true);

    lcd.setCursor(0, 0);
    typewrite("Typewriter...", 120);
    _delay_ms(600);

    lcd.cursor(false);
    lcd.blink(false);
    _delay_ms(500);

    // -----------------------------------------------------------------------
    // display(false)/display(true): the screen "flashes" without losing
    // its content — clear() is never called here, so the exact same text
    // reappears every time display(true) runs.
    // -----------------------------------------------------------------------
    for (uint8_t i = 0; i < 4; ++i) {
        lcd.display(false);
        _delay_ms(200);
        lcd.display(true);
        _delay_ms(200);
    }

    // -----------------------------------------------------------------------
    // Running hex byte counter via printHex() + writeChar().
    // -----------------------------------------------------------------------
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Hex counter:");

    uint8_t value = 0;
    while (true) {
        lcd.setCursor(0, 1);
        lcd.print("0x");
        lcd.printHex(value);
        lcd.print("  ");   // pad in case the previous frame left stray glyphs

        _delay_ms(500);
        ++value;   // wraps 0xFF -> 0x00 automatically (uint8_t)
    }
}
