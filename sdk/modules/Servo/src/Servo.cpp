#include "Servo.hpp"
#include <avr/io.h>

namespace MikroDuino {

Servo::Servo(uint8_t channel, uint16_t minUs, uint16_t maxUs)
    : _channel(channel), _minUs(minUs), _maxUs(maxUs),
      _currentUs(static_cast<uint16_t>((minUs + maxUs) / 2u)) {}

void Servo::begin() {
    // Timer1: Fast PWM with ICR1 as TOP (WGM13:10 = 1110), prescaler 8
    TCCR1A = (1 << WGM11);
    TCCR1B = (1 << WGM13) | (1 << WGM12) | (1 << CS11);
    ICR1   = _T1_TOP;

    if (_channel == 0) {
        DDRB   |= (1 << DDB1);          // PB1 → output
        TCCR1A |= (1 << COM1A1);        // non-inverting Fast PWM on OC1A
        OCR1A   = _usToCmp(_currentUs);
    } else {
        DDRB   |= (1 << DDB2);          // PB2 → output
        TCCR1A |= (1 << COM1B1);
        OCR1B   = _usToCmp(_currentUs);
    }
}

void Servo::detach() {
    if (_channel == 0) TCCR1A &= static_cast<uint8_t>(~(1u << COM1A1));
    else               TCCR1A &= static_cast<uint8_t>(~(1u << COM1B1));
}

void Servo::writeMicroseconds(uint16_t us) {
    if (us < _minUs) us = _minUs;
    if (us > _maxUs) us = _maxUs;
    _currentUs = us;
    uint16_t cmp = _usToCmp(us);
    if (_channel == 0) OCR1A = cmp;
    else               OCR1B = cmp;
}

void Servo::write(uint8_t angleDeg) {
    if (angleDeg > 180) angleDeg = 180;
    uint16_t us = static_cast<uint16_t>(
        _minUs + (static_cast<uint32_t>(angleDeg) * (_maxUs - _minUs)) / 180UL
    );
    writeMicroseconds(us);
}

} // namespace MikroDuino
