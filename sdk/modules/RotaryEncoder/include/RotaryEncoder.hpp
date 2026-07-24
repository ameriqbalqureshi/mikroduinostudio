#pragma once
/*
 * RotaryEncoder — interrupt-driven quadrature encoder with optional push button.
 *
 * Decodes a 2-channel (A/B) incremental rotary encoder using Pin Change
 * Interrupts (PCINT). Works on any digital I/O pins. The ISR must be routed
 * to _isrHandler() by the user because PCINT vectors are shared across banks
 * of 8 pins and the library cannot know which bank applies at compile time.
 *
 * Relationship to Phase-4A Encoder (math/gray_code.hpp)
 * ───────────────────────────────────────────────────────
 * Encoder in gray_code.hpp is a polling helper: the user calls update() and
 * passes the current A/B pin states. RotaryEncoder is interrupt-driven and
 * self-contained: it configures PCINT, counts automatically in the ISR,
 * and exposes an atomic count + optional debounced push button.
 *
 * Interrupt setup
 * ───────────────
 * begin() enables PCINT for pinA and pinB. The user must provide the ISR
 * body for the appropriate PCINT vector (PCINT0_vect / PCINT1_vect /
 * PCINT2_vect) and route it to _isrHandler():
 *
 *   MikroDuino::RotaryEncoder enc(PD2, PD3, PD4); // A, B, button
 *   enc.begin();
 *
 *   ISR(PCINT2_vect) { enc._isrHandler(); }  // PD pins are in PCINT2 group
 *
 *   sei();
 *
 * PCINT groups on ATmega328P:
 *   PCINT0_vect  → PB0–PB7  (port B)
 *   PCINT1_vect  → PC0–PC6  (port C)
 *   PCINT2_vect  → PD0–PD7  (port D)
 *
 * If A and B are on different banks, route both vectors:
 *   ISR(PCINT0_vect) { enc._isrHandler(); }
 *   ISR(PCINT2_vect) { enc._isrHandler(); }
 *
 * Push button
 * ───────────
 * Call updateButton() once per millisecond (same pattern as Button class).
 * The button is debounced internally; use buttonPressed() / buttonReleased()
 * / buttonIsDown() to query state.
 *
 * Usage — menu encoder with click:
 *   MikroDuino::RotaryEncoder enc(PD2, PD3, PD4);
 *   enc.begin();
 *   ISR(PCINT2_vect) { enc._isrHandler(); }
 *   sei();
 *
 *   int32_t lastCount = 0;
 *   while (true) {
 *       int32_t pos = enc.count();
 *       if (pos != lastCount) { redrawMenu(pos); lastCount = pos; }
 *       enc.updateButton();
 *       if (enc.buttonPressed()) selectMenuItem();
 *       _delay_ms(1);
 *   }
 */

#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/registers.hpp>  // ATOMIC_BLOCK_START / END

namespace MikroDuino {

class RotaryEncoder {
public:
    static constexpr uint8_t NO_PIN = 0xFF;

    // pinA, pinB    : encoder signal pins (any GPIO; pull-up enabled internally)
    // pinBtn        : optional push button; NO_PIN if not used
    // debounceMs    : button debounce window (default 20 ms)
    RotaryEncoder(uint8_t pinA, uint8_t pinB,
                  uint8_t pinBtn    = NO_PIN,
                  uint8_t debounceMs = 20)
        : _pinA(pinA), _pinB(pinB), _pinBtn(pinBtn),
          _debounceMs(debounceMs),
          _count(0), _dir(0), _prevAB(0),
          _btnDown(false), _btnRaw(false), _btnDebTimer(0),
          _btnHeldMs(0), _evBtnPressed(false), _evBtnReleased(false) {}

    // Configure GPIO and enable PCINT for pinA and pinB.
    // Does NOT call sei() — the user controls interrupt enablement.
    void begin();

    // --- Count access (interrupt-safe) ---

    // Current encoder count; CW increments, CCW decrements.
    int32_t count() const {
        int32_t v;
        ATOMIC_BLOCK_START;
        v = _count;
        ATOMIC_BLOCK_END;
        return v;
    }

    void resetCount(int32_t v = 0) {
        ATOMIC_BLOCK_START;
        _count = v;
        ATOMIC_BLOCK_END;
    }

    // Returns the direction of the last detected step: +1 CW, -1 CCW, 0 none.
    // Reading clears the direction cache.
    int8_t direction() {
        int8_t v;
        ATOMIC_BLOCK_START;
        v    = _dir;
        _dir = 0;
        ATOMIC_BLOCK_END;
        return v;
    }

    // --- Push button (call updateButton() every 1 ms) ---

    void updateButton() {
        if (_pinBtn == NO_PIN) return;
        bool raw = !GPIO::read(_pinBtn); // active-LOW (internal pull-up)

        if (raw == _btnRaw) {
            if (_btnDebTimer < _debounceMs) _btnDebTimer++;
            if (_btnDebTimer == _debounceMs && raw != _btnDown) {
                _btnDown = raw;
                if (_btnDown) { _evBtnPressed  = true; _btnHeldMs = 0; }
                else          { _evBtnReleased = true; }
            }
        } else {
            _btnRaw      = raw;
            _btnDebTimer = 0;
        }
        if (_btnDown && _btnHeldMs < 0xFFFFu) _btnHeldMs++;
    }

    bool buttonPressed()  { bool v = _evBtnPressed;  _evBtnPressed  = false; return v; }
    bool buttonReleased() { bool v = _evBtnReleased; _evBtnReleased = false; return v; }
    bool buttonIsDown()  const { return _btnDown; }
    uint16_t buttonHeldMs() const { return _btnHeldMs; }

    // --- ISR entry point ---
    // Place in the appropriate PCINT ISR vector:
    //   ISR(PCINT2_vect) { enc._isrHandler(); }
    void _isrHandler();

private:
    uint8_t _pinA, _pinB, _pinBtn;
    uint8_t _debounceMs;

    volatile int32_t _count;
    volatile int8_t  _dir;
    uint8_t          _prevAB;

    // Button debounce state (non-volatile: only touched from main thread via updateButton())
    bool     _btnDown, _btnRaw;
    uint8_t  _btnDebTimer;
    uint16_t _btnHeldMs;
    bool     _evBtnPressed, _evBtnReleased;

    // Quadrature decode TABLE indexed by (prev<<2)|curr.
    // Each entry: +1 = CW step, -1 = CCW step, 0 = no step / invalid.
    static const int8_t _TABLE[16];

    static void _enablePCINT(uint8_t pin);
};

} // namespace MikroDuino
