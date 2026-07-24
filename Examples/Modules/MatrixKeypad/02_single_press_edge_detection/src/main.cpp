/*
 * MatrixKeypad — Single-Press Edge Detection + anyPressed() — MikroDuino
 * Module SDK
 *
 * Project 2 of 6 in the examples/Modules/MatrixKeypad series. Project 1's
 * scan() kept reporting the same key over and over for as long as it was
 * held, which is correct behaviour for scan() itself but rarely what a
 * UI wants — a held '5' should register as ONE keypress, not dozens.
 * This project adds the transition-detection pattern MatrixKeypad's own
 * header comment recommends, and introduces anyPressed() to show the
 * difference between scan()'s single-key-only collision guard and a
 * simple presence check that doesn't care how many keys are down.
 *
 * Hardware — identical wiring to project 1, plus an LED:
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ ROW 1-4 │ PB0-3 │ Driven LOW one at a time during scan       │
 *   │ COL 1-4 │ PB4-7 │ Read with internal pull-up                 │
 *   │ LED     │ PD5   │ Lit whenever ANY key is physically touched │
 *   │ TXD     │ PD1   │ USB-serial adapter, 9600 8N1, for readings │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * MatrixKeypad concepts introduced:
 *   - Transition detection — comparing this scan()'s result against the
 *     PREVIOUS scan()'s result and only acting when they differ AND the
 *     new value is non-zero. This is plain application logic sitting on
 *     top of scan(), not a separate library method — MatrixKeypad itself
 *     has no notion of "this is a new press" versus "still held".
 *   - anyPressed() — true if ANY key is down, with no collision guard and
 *     no key identity returned. Two keys held at once make scan() return
 *     '\0' (ambiguous — which key was it?), but anyPressed() still
 *     correctly reports "something is pressed". This project uses that
 *     distinction directly: the LED (driven by anyPressed()) can be lit
 *     while the USART log (driven by the edge-detected scan()) stays
 *     silent, visibly demonstrating a multi-key collision as it happens.
 */

#include <util/delay.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/usart.hpp>
#include <MatrixKeypad.hpp>

using namespace MikroDuino;

static constexpr uint8_t LED = PD5;

static const uint8_t ROW_PINS[4] = { PB0, PB1, PB2, PB3 };
static const uint8_t COL_PINS[4] = { PB4, PB5, PB6, PB7 };

MatrixKeypad<4, 4> keypad(ROW_PINS, COL_PINS, (const char*)Keypad::MAP_4x4);

int main() {
    USART0.begin(9600);
    USART0.writeLine_P(PSTR("MatrixKeypad single-press edge detection"));
    USART0.writeLine_P(PSTR("========================================="));
    USART0.writeLine_P(PSTR("Each key prints exactly once per press, held or not"));
    USART0.writeLine_P(PSTR(""));

    GPIO::output(LED);
    GPIO::clear(LED);

    keypad.begin();

    char prevKey = '\0';

    while (true) {
        char key = keypad.scan();

        if (key && key != prevKey) {   // new press, not a repeat of the held key
            USART0.write_P(PSTR("key: "));
            USART0.write(static_cast<uint8_t>(key));
            USART0.writeLine_P(PSTR(""));
        }
        prevKey = key;

        // anyPressed() has no collision guard: it lights the LED even
        // while two keys held at once make scan() report '\0' above.
        GPIO::write(LED, keypad.anyPressed());

        _delay_ms(20);
    }
}
