/*
 * IRRemote Blocking receive() + Named Buttons — MikroDuino Module SDK
 *
 * Project 2 of 6 in the examples/Modules/IRRemote series. Swaps project
 * 1's `poll()` for `receive(timeoutMs)`, filters out frames from any
 * OTHER remote's address, and turns raw command bytes into human-
 * readable button names for one specific, widely-used remote.
 *
 * The button codes below match the cheap 21-key NEC remote bundled with
 * many Arduino starter kits (device address 0x00). If your remote is a
 * different one, run project 1 first, note down its address and the
 * command byte each button sends, and edit BUTTON_TABLE / DEVICE_ADDRESS
 * to match — the decode logic itself works with ANY NEC remote.
 *
 * Hardware: identical to project 1 — IR receiver OUT on PD2, USART0 at
 * 9600 8N1 on PD1.
 *
 * IRRemote concepts introduced:
 *   - receive(timeoutMs) — BLOCKING, unlike poll(). It first syncs to
 *     idle (in case a frame was already mid-flight when called), then
 *     waits up to timeoutMs for the NEXT frame to begin from its very
 *     first edge. Because it always catches a frame at its true start,
 *     unlike poll() it does not depend on being called every ≤ 1 ms —
 *     call it whenever convenient and it will wait for you. This
 *     project uses receive(200) so it also glues consecutive repeat
 *     frames (which arrive roughly every 108 ms while a button is held)
 *     into one continuous "held" stream printed as dots instead of a
 *     new line per repeat.
 *   - Address filtering — c.address is checked against DEVICE_ADDRESS
 *     before the command is even looked up. Every consumer electronics
 *     remote shares the same 32-bit NEC frame format, so without this
 *     check a neighbour's TV remote or a different device's remote
 *     would decode as perfectly "valid" NEC frames and get looked up in
 *     THIS project's button table by pure coincidence of command byte.
 *
 * IRRemote concepts reused from project 1:
 *   - NECCode (address/command/valid/repeat), IRReceiver(pin), begin().
 *   - The repeat flag, now used for something real: a NEW press
 *     (repeat==false) prints the button's name on its own line; a
 *     REPEAT (repeat==true, button still held) prints a single "." with
 *     no newline, so a long hold shows as one line of dots instead of
 *     scrolling the terminal with the same button name ~9 times a
 *     second.
 */

#include <avr/pgmspace.h>
#include <mikroduino/usart.hpp>
#include <IRRemote.hpp>

using namespace MikroDuino;

IRReceiver irReceiver(PD2);

static constexpr uint8_t DEVICE_ADDRESS = 0x00;

// Command byte -> button name, for the reference 21-key remote.
struct ButtonEntry { uint8_t command; const char* name; };

static const char NAME_CHM[]   PROGMEM = "CH-";
static const char NAME_CH[]    PROGMEM = "CH";
static const char NAME_CHP[]   PROGMEM = "CH+";
static const char NAME_PREV[]  PROGMEM = "PREV";
static const char NAME_NEXT[]  PROGMEM = "NEXT";
static const char NAME_PLAY[]  PROGMEM = "PLAY/PAUSE";
static const char NAME_VOLM[]  PROGMEM = "VOL-";
static const char NAME_VOLP[]  PROGMEM = "VOL+";
static const char NAME_EQ[]    PROGMEM = "EQ";
static const char NAME_0[]     PROGMEM = "0";
static const char NAME_100[]   PROGMEM = "100+";
static const char NAME_200[]   PROGMEM = "200+";
static const char NAME_1[]     PROGMEM = "1";
static const char NAME_2[]     PROGMEM = "2";
static const char NAME_3[]     PROGMEM = "3";
static const char NAME_4[]     PROGMEM = "4";
static const char NAME_5[]     PROGMEM = "5";
static const char NAME_6[]     PROGMEM = "6";
static const char NAME_7[]     PROGMEM = "7";
static const char NAME_8[]     PROGMEM = "8";
static const char NAME_9[]     PROGMEM = "9";

static const ButtonEntry BUTTON_TABLE[] PROGMEM = {
    { 0x45, NAME_CHM }, { 0x46, NAME_CH },  { 0x47, NAME_CHP },
    { 0x44, NAME_PREV },{ 0x40, NAME_NEXT },{ 0x43, NAME_PLAY },
    { 0x07, NAME_VOLM },{ 0x15, NAME_VOLP },{ 0x09, NAME_EQ },
    { 0x16, NAME_0 },   { 0x19, NAME_100 }, { 0x0D, NAME_200 },
    { 0x0C, NAME_1 },   { 0x18, NAME_2 },   { 0x5E, NAME_3 },
    { 0x08, NAME_4 },   { 0x1C, NAME_5 },   { 0x5A, NAME_6 },
    { 0x42, NAME_7 },   { 0x52, NAME_8 },   { 0x4A, NAME_9 },
};
static constexpr uint8_t BUTTON_TABLE_LEN = sizeof(BUTTON_TABLE) / sizeof(BUTTON_TABLE[0]);

// Looks up a command byte in PROGMEM; returns nullptr if not found.
static const char* buttonName(uint8_t command) {
    for (uint8_t i = 0; i < BUTTON_TABLE_LEN; ++i) {
        uint8_t cmd = pgm_read_byte(&BUTTON_TABLE[i].command);
        if (cmd == command) {
            return reinterpret_cast<const char*>(pgm_read_word(&BUTTON_TABLE[i].name));
        }
    }
    return nullptr;
}

int main() {
    USART0.begin(9600);
    USART0.writeLine_P(PSTR("IRRemote blocking receive() + named buttons"));
    USART0.writeLine_P(PSTR("============================================"));
    USART0.writeLine_P(PSTR("Press a button on the reference 21-key remote."));
    USART0.writeLine_P(PSTR(""));

    irReceiver.begin();

    bool holding = false;

    while (true) {
        NECCode c = irReceiver.receive(200);   // wait up to 200 ms for the next frame

        if (!c.valid) {
            if (holding) { USART0.writeLine_P(PSTR("")); holding = false; }   // hold ended
            continue;
        }

        if (c.repeat) {
            USART0.write_P(PSTR("."));
            holding = true;
            continue;
        }

        if (holding) { USART0.writeLine_P(PSTR("")); holding = false; }

        if (c.address != DEVICE_ADDRESS) {
            USART0.write_P(PSTR("ignoring foreign remote, addr=0x"));
            USART0.writeInt(static_cast<int32_t>(c.address), 16);
            USART0.writeLine_P(PSTR(""));
            continue;
        }

        const char* name = buttonName(c.command);
        if (name != nullptr) {
            USART0.write_P(PSTR("Button: "));
            USART0.write_P(name);
            USART0.writeLine_P(PSTR(""));
        } else {
            USART0.write_P(PSTR("Unknown command 0x"));
            USART0.writeInt(static_cast<int32_t>(c.command), 16);
            USART0.writeLine_P(PSTR(""));
        }
    }
}
