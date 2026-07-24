#pragma once
/*
 * MatrixKeypad<ROWS, COLS> — multiplexed matrix keypad scanner.
 *
 * Wiring convention (works with any ROWS × COLS keypad):
 *   Row pins    → configured as OUTPUT (driven LOW one at a time during scan)
 *   Column pins → configured as INPUT_PULLUP (read LOW when the key in the
 *                 active row is pressed, closing the row-col circuit)
 *
 * Compatible with standard 4×4 and 4×3 membrane keypads (Sunfounder,
 * SparkFun, generic eBay/AliExpress modules).
 *
 * Key map is passed as a flat ROWS×COLS array (row-major). The library
 * returns '\0' when no key is pressed.
 *
 * Usage — 4×4 keypad:
 *   #include <MatrixKeypad.hpp>
 *
 *   const uint8_t ROWS[4] = { PB0, PB1, PB2, PB3 };
 *   const uint8_t COLS[4] = { PB4, PB5, PB6, PB7 };
 *   const char    MAP[4][4] = {
 *       {'1','2','3','A'},
 *       {'4','5','6','B'},
 *       {'7','8','9','C'},
 *       {'*','0','#','D'}
 *   };
 *
 *   MikroDuino::MatrixKeypad<4,4> kp(ROWS, COLS, (const char*)MAP);
 *   kp.begin();
 *
 *   while (true) {
 *       char k = kp.scan();
 *       if (k) { use(k); }
 *       _delay_ms(10);        // call at 10–50 ms intervals for debounce
 *   }
 *
 * scan() returns the same key repeatedly while it is held. To detect
 * single presses, check for a transition:
 *
 *   char prev = '\0';
 *   while (true) {
 *       char k = kp.scan();
 *       if (k && k != prev) { use(k); }  // only fires on new keypress
 *       prev = k;
 *       _delay_ms(20);
 *   }
 */

#include <stdint.h>
#include <util/delay.h>
#include <mikroduino/gpio.hpp>

namespace MikroDuino {

template<uint8_t ROWS, uint8_t COLS>
class MatrixKeypad {
    static_assert(ROWS >= 1 && ROWS <= 8, "MatrixKeypad: ROWS must be 1..8");
    static_assert(COLS >= 1 && COLS <= 8, "MatrixKeypad: COLS must be 1..8");

public:
    // rowPins  : ROWS GPIO pin encodings
    // colPins  : COLS GPIO pin encodings
    // keyMap   : flat row-major char array [ROWS × COLS]
    MatrixKeypad(const uint8_t rowPins[ROWS],
                 const uint8_t colPins[COLS],
                 const char*   keyMap)
        : _keyMap(keyMap)
    {
        for (uint8_t i = 0; i < ROWS; i++) _rowPins[i] = rowPins[i];
        for (uint8_t i = 0; i < COLS; i++) _colPins[i] = colPins[i];
    }

    void begin() {
        // Rows: output HIGH (inactive)
        for (uint8_t r = 0; r < ROWS; r++) {
            GPIO::output(_rowPins[r]);
            GPIO::set(_rowPins[r]);
        }
        // Columns: input with internal pull-up
        for (uint8_t c = 0; c < COLS; c++) GPIO::inputPullup(_colPins[c]);
    }

    // Perform one scan pass. Returns the key character from the map, or '\0'.
    // For clean debounce, call every 10–50 ms. No key is returned when
    // multiple keys are pressed simultaneously (returns '\0').
    char scan() const {
        char found = '\0';
        uint8_t hits = 0;

        for (uint8_t r = 0; r < ROWS; r++) {
            GPIO::clear(_rowPins[r]);   // drive this row LOW
            _delay_us(5);               // settle time for capacitive boards

            for (uint8_t c = 0; c < COLS; c++) {
                if (!GPIO::read(_colPins[c])) { // column is LOW → key pressed
                    found = _keyMap[r * COLS + c];
                    hits++;
                }
            }

            GPIO::set(_rowPins[r]);     // restore row HIGH before next row
        }

        return (hits == 1) ? found : '\0'; // return '\0' on multi-key collision
    }

    // true if any key is currently pressed (no debounce, no collision guard)
    bool anyPressed() const {
        for (uint8_t r = 0; r < ROWS; r++) {
            GPIO::clear(_rowPins[r]);
            _delay_us(5);
            for (uint8_t c = 0; c < COLS; c++) {
                if (!GPIO::read(_colPins[c])) { GPIO::set(_rowPins[r]); return true; }
            }
            GPIO::set(_rowPins[r]);
        }
        return false;
    }

    // Block until a key is pressed and released. Returns the key char.
    char waitKey(uint8_t debounceMs = 20) const {
        char k = '\0';
        while (!(k = scan())) _delay_ms(debounceMs);
        _delay_ms(debounceMs);  // confirm hold
        while (scan()) {}       // wait for release
        return k;
    }

private:
    uint8_t     _rowPins[ROWS];
    uint8_t     _colPins[COLS];
    const char* _keyMap;
};

// Pre-built key maps for common keypad types
namespace Keypad {

// Standard 4×4: 1-9, A-D, *, 0, #
static constexpr char MAP_4x4[4][4] = {
    {'1','2','3','A'},
    {'4','5','6','B'},
    {'7','8','9','C'},
    {'*','0','#','D'}
};

// Standard 4×3 (telephone layout): 1-9, *, 0, #
static constexpr char MAP_4x3[4][3] = {
    {'1','2','3'},
    {'4','5','6'},
    {'7','8','9'},
    {'*','0','#'}
};

} // namespace Keypad

} // namespace MikroDuino
