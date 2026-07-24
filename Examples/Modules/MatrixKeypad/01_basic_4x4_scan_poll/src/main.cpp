/*
 * MatrixKeypad Basics — 4x4 Blocking Scan Poll — MikroDuino Module SDK
 *
 * The simplest possible use of the MatrixKeypad module: a standard 4x4
 * membrane keypad scanned in a plain blocking loop, printed over USART.
 * This is project 1 of 6 in the examples/Modules/MatrixKeypad series,
 * which walks MatrixKeypad<ROWS,COLS> from a single blocking scan() up to
 * a capstone LCD + Servo door-access dashboard backed by an EEPROM PIN.
 *
 * Hardware (ATmega328P @ 16 MHz, standard 4x4 membrane keypad):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ ROW 1-4 │ PB0-3 │ Driven LOW one at a time during scan       │
 *   │ COL 1-4 │ PB4-7 │ Read with internal pull-up                 │
 *   │ TXD     │ PD1   │ USB-serial adapter, 9600 8N1, for readings │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * MatrixKeypad concepts introduced:
 *   - MatrixKeypad<ROWS,COLS> — a template class; ROWS and COLS (4 and 4
 *     here) are fixed at compile time. The same class body serves any
 *     size keypad from 1x1 up to 8x8 with no runtime overhead for pins it
 *     doesn't have.
 *   - MatrixKeypad(rowPins, colPins, keyMap) — this project uses the
 *     module's own built-in MikroDuino::Keypad::MAP_4x4 layout (1-9, A-D,
 *     *, 0, #) rather than defining a custom map; project 3 keeps using
 *     it, and every later project in this series does too, since it
 *     matches the vast majority of off-the-shelf 4x4 keypads.
 *   - begin() — configures every row pin as an output (idle HIGH) and
 *     every column pin as an input with the internal pull-up enabled.
 *   - scan() — performs one full row-by-row pass and returns the key
 *     character under the ONE column that reads LOW, or '\0' if no key
 *     (or more than one key at once — a "collision") is pressed. Calling
 *     it in a loop while a key stays held returns that SAME key every
 *     time, which is why this project's output repeats a held digit
 *     rather than printing it once — project 2 is exactly about fixing
 *     that.
 */

#include <util/delay.h>
#include <mikroduino/usart.hpp>
#include <MatrixKeypad.hpp>

using namespace MikroDuino;

static const uint8_t ROW_PINS[4] = { PB0, PB1, PB2, PB3 };
static const uint8_t COL_PINS[4] = { PB4, PB5, PB6, PB7 };

// Built-in 4x4 layout: {'1','2','3','A'} / {'4','5','6','B'} /
// {'7','8','9','C'} / {'*','0','#','D'}
MatrixKeypad<4, 4> keypad(ROW_PINS, COL_PINS, (const char*)Keypad::MAP_4x4);

int main() {
    USART0.begin(9600);
    USART0.writeLine_P(PSTR("MatrixKeypad basic 4x4 scan poll"));
    USART0.writeLine_P(PSTR("================================="));
    USART0.writeLine_P(PSTR("Prints the pressed key every scan (repeats while held)"));
    USART0.writeLine_P(PSTR(""));

    keypad.begin();

    while (true) {
        char key = keypad.scan();

        if (key) {
            USART0.write_P(PSTR("key: "));
            USART0.write(static_cast<uint8_t>(key));
            USART0.writeLine_P(PSTR(""));
        }

        _delay_ms(20);   // scan() wants to be called every 10-50 ms for debounce
    }
}
