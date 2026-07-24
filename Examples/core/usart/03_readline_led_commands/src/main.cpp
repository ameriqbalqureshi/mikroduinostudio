/*
 * Readline LED Commands — MikroDuino SDK (line-based text commands)
 *
 * Project 3 of 7 in the examples/usart series. Type a whole command and
 * press Enter; the board parses the line and drives two LEDs. This is
 * the first project in the series to read more than a single byte at a
 * time, and the first to combine USART with GPIO output.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ Signal  │ Pin   │ Wiring                                    │
 *   ├─────────┼───────┼──────────────────────────────────────────┤
 *   │ TXD     │ PD1   │ To RX of USB-serial adapter                │
 *   │ RXD     │ PD0   │ From TX of USB-serial adapter               │
 *   │ LED_A   │ PB5   │ LED + 220 Ω to GND (built-in LED on most   │
 *   │         │       │ boards) — controlled by ON/OFF/TOGGLE      │
 *   │ LED_B   │ PB4   │ LED + 220 Ω to GND — controlled by BLINK   │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * Open a terminal at 9600 8N1 with local echo ON (readLine() itself does
 * not echo — see the note below) and try these commands, each followed
 * by Enter:
 *
 *   ON        turn LED_A on
 *   OFF       turn LED_A off
 *   TOGGLE    flip LED_A
 *   BLINK 5   blink LED_B 5 times (default 3 if no number given)
 *   STATUS    print the current state of both LEDs
 *   HELP      print this command list
 *
 * USART concepts introduced:
 *   - readLine(buffer, size) — blocks, reading and buffering characters
 *     one at a time (internally via read(), the blocking single-byte
 *     receive from project 2), until it sees '\r' or '\n', or the
 *     buffer is one character from full. The line terminator itself is
 *     consumed but never stored, and the buffer is always left
 *     null-terminated, so it can be handled exactly like any C string
 *     (strcmp, strncmp, atoi, ...).
 *   - readLine() does NOT echo what it reads. Terminal programs
 *     (PuTTY, Arduino Serial Monitor with "local echo", `screen`, ...)
 *     usually show your own typing locally, so this normally isn't
 *     noticeable — but if your commands appear invisible while typing,
 *     that is why, and it's a terminal setting, not a bug in the board.
 *   - Everything else here (write_P/writeLine_P for flash-resident
 *     menu text, writeInt for the blink count) reuses the write-side
 *     API from project 1.
 */

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <string.h>
#include <stdlib.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/usart.hpp>

using namespace MikroDuino;

static constexpr uint8_t LED_A = PB5;
static constexpr uint8_t LED_B = PB4;

static constexpr uint8_t LINE_BUF_SIZE = 32;

static void delay_ms(uint16_t ms) { while (ms--) _delay_ms(1); }

static void print_help() {
    USART0.writeLine_P(PSTR("Commands:"));
    USART0.writeLine_P(PSTR("  ON        - LED_A on"));
    USART0.writeLine_P(PSTR("  OFF       - LED_A off"));
    USART0.writeLine_P(PSTR("  TOGGLE    - flip LED_A"));
    USART0.writeLine_P(PSTR("  BLINK n   - blink LED_B n times (default 3)"));
    USART0.writeLine_P(PSTR("  STATUS    - show LED state"));
    USART0.writeLine_P(PSTR("  HELP      - this list"));
}

// Case-insensitive "does line start with word" check, used so "on",
// "ON" and "On" are all accepted. Returns a pointer to the first
// character after the matched word (skipping one space), or nullptr
// if the line does not start with `word`.
static const char* match_word(const char* line, const char* word) {
    uint8_t i = 0;
    while (word[i]) {
        char a = line[i];
        char b = word[i];
        if (a >= 'a' && a <= 'z') a = static_cast<char>(a - 'a' + 'A');
        if (b >= 'a' && b <= 'z') b = static_cast<char>(b - 'a' + 'A');
        if (a != b) return nullptr;
        ++i;
    }
    // Word must end here (end of line) or be followed by a space.
    if (line[i] == '\0') return line + i;
    if (line[i] == ' ')  return line + i + 1;
    return nullptr;
}

int main() {
    GPIO::output(LED_A);
    GPIO::output(LED_B);
    GPIO::clear(LED_A);
    GPIO::clear(LED_B);

    USART0.begin(9600);
    USART0.writeLine_P(PSTR("Readline LED Commands ready."));
    print_help();

    char line[LINE_BUF_SIZE];

    while (true) {
        USART0.write_P(PSTR("> "));

        // Blocks here until a full line (terminated by Enter) arrives.
        uint8_t len = USART0.readLine(line, sizeof(line));
        if (len == 0) continue;   // empty line (just pressed Enter)

        const char* arg;

        if ((arg = match_word(line, "ON")) != nullptr) {
            GPIO::set(LED_A);
            USART0.writeLine_P(PSTR("LED_A: ON"));

        } else if ((arg = match_word(line, "OFF")) != nullptr) {
            GPIO::clear(LED_A);
            USART0.writeLine_P(PSTR("LED_A: OFF"));

        } else if ((arg = match_word(line, "TOGGLE")) != nullptr) {
            GPIO::toggle(LED_A);
            USART0.write_P(PSTR("LED_A: "));
            USART0.writeLine_P(GPIO::read(LED_A) ? PSTR("ON") : PSTR("OFF"));

        } else if ((arg = match_word(line, "BLINK")) != nullptr) {
            // atoi() returns 0 if `arg` is empty or not numeric, so a
            // bare "BLINK" with no number falls back to the default.
            int16_t times = atoi(arg);
            if (times <= 0) times = 3;

            USART0.write_P(PSTR("LED_B blinking "));
            USART0.writeInt(times);
            USART0.writeLine_P(PSTR(" time(s)..."));

            for (int16_t i = 0; i < times; ++i) {
                GPIO::set(LED_B);
                delay_ms(150);
                GPIO::clear(LED_B);
                delay_ms(150);
            }
            USART0.writeLine_P(PSTR("Blink done."));

        } else if ((arg = match_word(line, "STATUS")) != nullptr) {
            USART0.write_P(PSTR("LED_A="));
            USART0.writeLine_P(GPIO::read(LED_A) ? PSTR("ON") : PSTR("OFF"));
            USART0.write_P(PSTR("LED_B="));
            USART0.writeLine_P(GPIO::read(LED_B) ? PSTR("ON") : PSTR("OFF"));

        } else if ((arg = match_word(line, "HELP")) != nullptr) {
            print_help();

        } else {
            USART0.write_P(PSTR("Unknown command: '"));
            USART0.write(line);
            USART0.writeLine_P(PSTR("'  (try HELP)"));
        }
    }
}
