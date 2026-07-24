#include "RotaryEncoder.hpp"
#include <avr/io.h>

namespace MikroDuino {

// Quadrature state-transition table indexed by (prevAB << 2) | currAB.
// Gray-code CW sequence: 00→01→11→10→00  (+1 per valid CW step)
// CCW is the reverse.
const int8_t RotaryEncoder::_TABLE[16] = {
     0, +1, -1,  0,   // prev=00: 00→no, 01→CW, 10→CCW, 11→err
    -1,  0,  0, +1,   // prev=01: 00→CCW, 01→no, 10→err, 11→CW
    +1,  0,  0, -1,   // prev=10: 00→CW, 01→err, 10→no, 11→CCW
     0, -1, +1,  0    // prev=11: 00→err, 01→CCW, 10→CW, 11→no
};

void RotaryEncoder::_enablePCINT(uint8_t pin) {
    // MikroDuino pin encoding: bits[5:3]=port, bits[2:0]=bit
    uint8_t port = pin >> 3;
    uint8_t bit  = pin & 0x07u;
    switch (port) {
        case 1: PCMSK0 |= (1u << bit); PCICR |= (1u << PCIE0); break; // PB
        case 2: PCMSK1 |= (1u << bit); PCICR |= (1u << PCIE1); break; // PC
        case 3: PCMSK2 |= (1u << bit); PCICR |= (1u << PCIE2); break; // PD
        default: break;
    }
}

void RotaryEncoder::begin() {
    GPIO::inputPullup(_pinA);
    GPIO::inputPullup(_pinB);
    if (_pinBtn != NO_PIN) GPIO::inputPullup(_pinBtn);

    // Snapshot initial A/B state before enabling interrupts
    uint8_t a = GPIO::read(_pinA) ? 1u : 0u;
    uint8_t b = GPIO::read(_pinB) ? 1u : 0u;
    _prevAB = static_cast<uint8_t>((a << 1) | b);

    _enablePCINT(_pinA);
    _enablePCINT(_pinB);
    // sei() is left to the user
}

void RotaryEncoder::_isrHandler() {
    uint8_t a    = GPIO::read(_pinA) ? 1u : 0u;
    uint8_t b    = GPIO::read(_pinB) ? 1u : 0u;
    uint8_t curr = static_cast<uint8_t>((a << 1) | b);
    uint8_t idx  = static_cast<uint8_t>((_prevAB << 2) | curr);
    int8_t  step = _TABLE[idx];
    if (step) {
        _count += step;
        _dir    = step;
    }
    _prevAB = curr;
}

} // namespace MikroDuino
