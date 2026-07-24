#pragma once
/*
 * Button — full-featured push button with debounce, long press, and double-click.
 *
 * Relationship to Phase-1 Debounce
 * ─────────────────────────────────
 * Debounce (sched/debounce.hpp) is a lightweight signal filter that needs
 * an external millisecond counter. Button wraps a pin, self-reads, self-times,
 * and adds long-press and multi-click events. Use Debounce for non-button
 * signals; use Button for physical push buttons.
 *
 * Timing model
 * ────────────
 * Call update() once per millisecond (or at a fixed interval specified in the
 * constructor). The simplest approach is to call it from the main loop with a
 * 1 ms busy-wait:
 *
 *   while (true) {
 *       btn.update();
 *       if (btn.clicked()) handleClick();
 *       _delay_ms(1);
 *   }
 *
 * Or schedule it from a MikroDuino Scheduler<N> task at a 1 ms period.
 *
 * Events (each returns true exactly once, then clears itself):
 *   pressed()       — debounced press (pin goes active)
 *   released()      — debounced release (pin goes inactive)
 *   clicked()       — short press + release (no long press fired)
 *   doubleClicked() — two clicks within doubleClickMs
 *   longPressed()   — held for ≥ longPressMs (fires once per press)
 *
 * State queries (no side effect):
 *   isDown() — currently pressed (debounced)
 *   heldMs() — how long the button has been held, in ms
 *
 * Usage:
 *   #include <Button.hpp>
 *
 *   MikroDuino::Button btn(PD4);   // active-LOW, internal pull-up
 *   btn.begin();
 *
 *   while (true) {
 *       btn.update();
 *       if (btn.clicked())       Serial::print("click\n");
 *       if (btn.doubleClicked()) Serial::print("double\n");
 *       if (btn.longPressed())   Serial::print("long\n");
 *       _delay_ms(1);
 *   }
 */

#include <stdint.h>
#include <mikroduino/gpio.hpp>

namespace MikroDuino {

class Button {
public:
    // pin          : MikroDuino GPIO pin encoding
    // activeLow    : true (default) = button pulls pin LOW when pressed
    // debounceMs   : hold time before a state change is accepted (default 20 ms)
    // longPressMs  : how long to hold before longPressed() fires (default 600 ms)
    // doubleClickMs: max gap between two clicks for doubleClicked() (default 350 ms)
    Button(uint8_t pin,
           bool     activeLow    = true,
           uint16_t debounceMs   = 20,
           uint16_t longPressMs  = 600,
           uint16_t doubleClickMs = 350)
        : _pin(pin), _activeLow(activeLow),
          _debounceMs(debounceMs), _longPressMs(longPressMs), _doubleMs(doubleClickMs),
          _rawState(false), _stableState(false), _isDown(false),
          _debTimer(0), _downMs(0), _upMs(0),
          _clickCount(0), _longFired(false),
          _evPressed(false), _evReleased(false), _evClick(false),
          _evDouble(false), _evLong(false) {}

    void begin() {
        if (_activeLow) GPIO::inputPullup(_pin);
        else            GPIO::input(_pin);
        _rawState = _readPin();
    }

    // Call once per millisecond (or at a known fixed interval).
    void update() {
        bool raw = _readPin();

        // --- Debounce ---
        if (raw == _rawState) {
            if (_debTimer < _debounceMs) {
                _debTimer++;
                if (_debTimer == _debounceMs && raw != _stableState) {
                    _stableState = raw;
                    if (_stableState) {        // pressed
                        _isDown     = true;
                        _downMs     = 0;
                        _longFired  = false;
                        _evPressed  = true;
                    } else {                   // released
                        _isDown    = false;
                        _evReleased = true;
                        if (!_longFired) {
                            _clickCount++;
                            _upMs = 0;         // start double-click window
                        }
                    }
                }
            }
        } else {
            _rawState = raw;
            _debTimer = 0;
        }

        // --- Long press timing ---
        if (_isDown) {
            _downMs++;
            if (!_longFired && _downMs >= _longPressMs) {
                _longFired = true;
                _evLong    = true;
            }
        }

        // --- Double-click window ---
        if (_clickCount > 0 && !_isDown) {
            if (_upMs < 0xFFFFu) _upMs++;
            if (_upMs >= _doubleMs) {
                if (_clickCount == 1) _evClick  = true;
                else                  _evDouble = true;
                _clickCount = 0;
            }
        }
    }

    // --- One-shot events ---
    bool pressed()       { bool v = _evPressed;  _evPressed  = false; return v; }
    bool released()      { bool v = _evReleased; _evReleased = false; return v; }
    bool clicked()       { bool v = _evClick;    _evClick    = false; return v; }
    bool doubleClicked() { bool v = _evDouble;   _evDouble   = false; return v; }
    bool longPressed()   { bool v = _evLong;     _evLong     = false; return v; }

    // --- State queries (no side effect) ---
    bool     isDown()  const { return _isDown; }
    uint16_t heldMs()  const { return _downMs; }

    void setLongPressMs(uint16_t ms)   { _longPressMs = ms; }
    void setDoubleClickMs(uint16_t ms) { _doubleMs    = ms; }

private:
    uint8_t  _pin;
    bool     _activeLow;
    uint16_t _debounceMs, _longPressMs, _doubleMs;

    bool     _rawState, _stableState, _isDown;
    uint16_t _debTimer;   // counts identical raw samples
    uint16_t _downMs;     // ms held
    uint16_t _upMs;       // ms since last release (double-click window)
    uint8_t  _clickCount;
    bool     _longFired;

    bool _evPressed, _evReleased, _evClick, _evDouble, _evLong;

    // Returns true = button PRESSED (accounts for active polarity)
    bool _readPin() const {
        bool level = GPIO::read(_pin);
        return _activeLow ? !level : level;
    }
};

} // namespace MikroDuino
