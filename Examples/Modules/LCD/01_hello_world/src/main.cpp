/*
 * LCD Basics — Hello World — MikroDuino Module SDK
 *
 * The simplest possible use of the LCD module: initialise a 16x2
 * character LCD, print a greeting, and show a live-updating counter on
 * the second line. This is project 1 of 6 in the examples/Modules/LCD
 * series, which walks the CharLCD class from a single "Hello World" up
 * to a capstone settings menu combining LCD, Button, and EEPROM.
 *
 * Hardware (ATmega328P @ 16 MHz, HD44780-compatible 16x2 character LCD):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ Signal  │ Pin   │ Wiring                                    │
 *   ├─────────┼───────┼──────────────────────────────────────────┤
 *   │ RS      │ PB0   │ LCD register-select                        │
 *   │ EN      │ PB1   │ LCD enable (latch strobe)                  │
 *   │ D4      │ PB2   │ LCD data bit 4                              │
 *   │ D5      │ PB3   │ LCD data bit 5                              │
 *   │ D6      │ PB4   │ LCD data bit 6                              │
 *   │ D7      │ PB5   │ LCD data bit 7                              │
 *   │ —       │ —     │ LCD R/W -> GND (this driver only ever       │
 *   │         │       │ writes, never reads back, so R/W is tied    │
 *   │         │       │ permanently to ground rather than driven)   │
 *   │ —       │ —     │ LCD VSS -> GND, VDD -> 5V, VO -> contrast   │
 *   │         │       │ pot wiper (10 k trimmer between 5V/GND)     │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * All six control/data lines deliberately land on PORTB here — a tidy
 * single 6-wire ribbon from one port, easy to breadboard and easy to
 * read back off the schematic. Nothing about CharLCD requires this;
 * any six free GPIO pins work equally well.
 *
 * This driver uses the LCD in 4-BIT mode: only D4-D7 are wired (D0-D3
 * are left unconnected on the LCD side), and every byte — command or
 * character — is sent as two 4-bit nibbles. This halves the pin count
 * compared to 8-bit mode at the cost of needing two enable pulses per
 * byte, a trade almost every microcontroller LCD project makes.
 *
 * LCD concepts introduced:
 *   - CharLCD(rs, en, d4, d5, d6, d7, cols=16, rows=2) — the constructor
 *     just records which pins are which and the display's geometry; no
 *     hardware is touched until begin() runs.
 *   - begin() — configures all six pins as outputs and runs the
 *     HD44780's documented 4-bit initialisation sequence (a specific
 *     series of "nibble" writes with mandated delays — see LCD.cpp for
 *     the exact timings straight from the datasheet). Must be called
 *     once before anything else.
 *   - print(const char* str) — writes a null-terminated C string,
 *     character by character, starting at the LCD's current cursor
 *     position.
 *   - setCursor(col, row) — moves the cursor (and therefore where the
 *     next print()/writeChar() lands) to a specific column/row, 0-based.
 *     Row 1 is the SECOND physical row on a 16x2 — the same 0-based
 *     convention used throughout this SDK's pin/index numbering.
 *   - print(int32_t value) — formats a signed integer to decimal text
 *     internally (via avr-libc's ltoa()) and prints that, so numbers
 *     don't need to be converted to strings by hand first.
 */

#include <util/delay.h>
#include <LCD.hpp>

using namespace MikroDuino;

CharLCD lcd(PB0, PB1, PB2, PB3, PB4, PB5);   // 16x2 by default

int main() {
    lcd.begin();

    lcd.setCursor(0, 0);
    lcd.print("Hello, Mikro-");
    lcd.setCursor(0, 1);
    lcd.print("Duino!");
    _delay_ms(2000);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Uptime (s):");

    int32_t seconds = 0;
    while (true) {
        lcd.setCursor(0, 1);
        lcd.print(seconds);
        lcd.print("   ");   // pad over any leftover digits from a longer previous value

        _delay_ms(1000);
        ++seconds;
    }
}
