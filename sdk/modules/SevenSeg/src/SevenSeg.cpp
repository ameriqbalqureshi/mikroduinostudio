#include "SevenSeg.hpp"
#include <avr/io.h>
#include <avr/interrupt.h>

namespace MikroDuino {

namespace detail {

const uint8_t SEG_TABLE[16] = {
    0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07,  // 0-7
    0x7F, 0x6F, 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71    // 8-F
};

} // namespace detail

// ============================================================================
//  SevenSeg — single digit
// ============================================================================

SevenSeg::SevenSeg(const uint8_t segPins[7], uint8_t dpPin, bool commonAnode)
    : _dpPin(dpPin), _commonAnode(commonAnode), _current(0u)
{
    for (uint8_t i = 0u; i < 7u; ++i) _pins[i] = segPins[i];
}

void SevenSeg::begin() {
    for (uint8_t i = 0u; i < 7u; ++i) GPIO::output(_pins[i]);
    if (_dpPin != SEG_NO_PIN) GPIO::output(_dpPin);
    clear();
}

void SevenSeg::show(uint8_t digit) {
    if (digit < 16u)
        _write((_current & 0x80u) | detail::SEG_TABLE[digit]);
}

void SevenSeg::showChar(char c) {
    _write((_current & 0x80u) | detail::charToSeg(c));
}

void SevenSeg::showRaw(uint8_t seg) { _write(seg); }

void SevenSeg::setDP(bool on) {
    if (on) _current |=  0x80u;
    else    _current &= ~0x80u;
    _write(_current);
}

void SevenSeg::clear() { _write(0u); }

void SevenSeg::_write(uint8_t seg) {
    _current = seg;
    for (uint8_t i = 0u; i < 7u; ++i) {
        bool on = static_cast<bool>((seg >> i) & 1u);
        GPIO::write(_pins[i], _commonAnode ? !on : on);
    }
    if (_dpPin != SEG_NO_PIN) {
        bool dpOn = static_cast<bool>((seg >> 7u) & 1u);
        GPIO::write(_dpPin, _commonAnode ? !dpOn : dpOn);
    }
}

// ============================================================================
//  SevenSegMuxBase — Timer2 auto-refresh
// ============================================================================

SevenSegMuxBase* SevenSegMuxBase::_autoInstance = nullptr;

void SevenSegMuxBase::beginTimer2(uint16_t prescaler) {
    _autoInstance = this;

    TCCR2A = 0u; // Normal mode (OVF at 0xFF)

    uint8_t cs;
    switch (prescaler) {
        case 8:   cs = (1u << CS21);                        break;
        case 32:  cs = (1u << CS21) | (1u << CS20);         break;
        default:  // fall through to 64
        case 64:  cs = (1u << CS22);                        break;
        case 128: cs = (1u << CS22) | (1u << CS20);         break;
        case 256: cs = (1u << CS22) | (1u << CS21);         break;
    }
    TCCR2B  = cs;
    TIMSK2  = (1u << TOIE2);
    // Call sei() in user code after begin() / beginTimer2()
}

} // namespace MikroDuino

// ============================================================================
//  Timer2 OVF ISR — dispatches to the registered SevenSegMux instance
// ============================================================================
//
// Conflict: IRRemote::begin() also defines this ISR.  Use only one at a time.
//   If using IRRemote, drive SevenSegMux manually with refresh() from a
//   Scheduler task, or use IRReceiver (GPIO mode) which needs no timer.

ISR(TIMER2_OVF_vect) {
    if (MikroDuino::SevenSegMuxBase::_autoInstance)
        MikroDuino::SevenSegMuxBase::_autoInstance->_isrTick();
}
