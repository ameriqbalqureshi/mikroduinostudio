/*
 * DS3231 set() + Weekday Names — MikroDuino Module SDK
 *
 * Project 2 of 6 in the examples/Modules/DS3231 series. Programs a
 * starting date/time into the RTC with set() — but only when the
 * oscillator was actually stopped, exactly like the raw-I2C
 * examples/i2c/04_ds1307_set_get_time example's rationale: a DS3231
 * with a healthy backup battery keeps correct time across resets and
 * even full power loss to the ATmega, so unconditionally overwriting it
 * with a fixed compile-time value on every boot would defeat the whole
 * point of having a battery-backed RTC. This project also turns the raw
 * numeric dayOfWeek field into a real weekday name.
 *
 * Hardware: identical to project 1 — SDA/SCL on PC4/PC5, USART0 TX on
 * PD1 at 9600 8N1.
 *
 * DS3231 concepts reused from project 1:
 *   - DS3231(), begin(), isRunning(), now().
 *
 * DS3231 concept introduced:
 *   - set(const DateTime&) — writes all 7 clock registers in a single
 *     I2C burst. Note there's no read-modify-write here: set() always
 *     writes every field, so a partially-filled DateTime (say, with
 *     dayOfWeek left at its default 0) would happily get written as-is
 *     — it's on the caller to fill in every field with a sensible
 *     value, which this project does explicitly for all seven.
 *
 * Weekday names are ordinary application code, not part of the module:
 * DateTime.dayOfWeek is just a number, 1-7, with 1=Sunday by this SDK's
 * (and the raw-I2C DS1307 examples') convention — the DS3231 chip
 * itself has no opinion on what the numbers mean or which day a week
 * starts on, it just increments and wraps a counter you told it how to
 * seed. A 7-entry PROGMEM table (the same "table of PROGMEM strings"
 * pattern examples/Modules/DCMotor/06 and LCD/06 both used for their
 * own on-screen menus) turns that number into "Mon", "Tue", etc.
 */

#include <avr/pgmspace.h>
#include <util/delay.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/usart.hpp>
#include <mikroduino/i2c.hpp>
#include <DS3231.hpp>

using namespace MikroDuino;

DS3231 rtc;

static const char DAY_SUN[] PROGMEM = "Sun";
static const char DAY_MON[] PROGMEM = "Mon";
static const char DAY_TUE[] PROGMEM = "Tue";
static const char DAY_WED[] PROGMEM = "Wed";
static const char DAY_THU[] PROGMEM = "Thu";
static const char DAY_FRI[] PROGMEM = "Fri";
static const char DAY_SAT[] PROGMEM = "Sat";
static const char* const DAY_NAMES[7] PROGMEM = {
    DAY_SUN, DAY_MON, DAY_TUE, DAY_WED, DAY_THU, DAY_FRI, DAY_SAT
};

static void printWeekday(uint8_t dayOfWeek) {
    // dayOfWeek is 1-7; DAY_NAMES is 0-indexed. Both the table and each
    // name it points to live in flash, so the fetched pointer must be
    // read back with write_P() (pgm_read_byte internally), never with
    // an ordinary string print — a plain SRAM read of a flash address
    // returns garbage on classic AVR.
    PGM_P name = reinterpret_cast<PGM_P>(pgm_read_word(&DAY_NAMES[dayOfWeek - 1]));
    USART0.write_P(name);
}

static void printField2(uint8_t v) {
    USART0.write(static_cast<char>('0' + (v / 10)));
    USART0.write(static_cast<char>('0' + (v % 10)));
}

int main() {
    GPIO::inputPullup(PC4);
    GPIO::inputPullup(PC5);

    USART0.begin(9600);
    USART0.writeLine_P(PSTR("DS3231 set() + weekday names"));
    USART0.writeLine_P(PSTR("=============================="));

    I2C.beginMaster(100000UL);

    bool wasHalted = !rtc.isRunning();
    rtc.begin();

    if (wasHalted) {
        USART0.writeLine_P(PSTR("Oscillator was stopped -> writing startup date/time"));

        // An arbitrary starting date/time — edit to taste, or wire up a
        // button/serial command to set it interactively. dayOfWeek=2
        // here is Monday (1=Sunday convention), matching 2026-07-20.
        DateTime startup = { 0, 0, 12, 2, 20, 7, 26 };   // Mon 2026-07-20 12:00:00
        rtc.set(startup);
        USART0.writeLine_P(PSTR("rtc.set() done."));
    } else {
        USART0.writeLine_P(PSTR("Oscillator already running - leaving the clock alone."));
    }
    USART0.writeLine_P(PSTR(""));

    while (true) {
        DateTime now = rtc.now();

        printWeekday(now.dayOfWeek);
        USART0.write_P(PSTR(" 20"));
        printField2(now.year);
        USART0.write_P(PSTR("-"));
        printField2(now.month);
        USART0.write_P(PSTR("-"));
        printField2(now.day);
        USART0.write_P(PSTR("  "));
        printField2(now.hour);
        USART0.write_P(PSTR(":"));
        printField2(now.minute);
        USART0.write_P(PSTR(":"));
        printField2(now.second);
        USART0.writeLine_P(PSTR(""));

        _delay_ms(1000);
    }
}
