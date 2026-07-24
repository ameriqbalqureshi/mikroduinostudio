#include "../include/DHT22.hpp"
#include <util/delay.h>
#include <avr/interrupt.h>

namespace MikroDuino {

DHT22Data DHT22Driver::read() {
    DHT22Data result = { 0.0f, 0.0f, false };

    // Start signal: pull low for 1 ms, then release
    GPIO::output(_pin);
    GPIO::clear(_pin);
    _delay_ms(1);
    GPIO::inputPullup(_pin);
    _delay_us(30);

    // Wait for sensor response (low 80 µs, high 80 µs)
    if (pulseWidth(false, 100) == 0) return result;
    if (pulseWidth(true,  100) == 0) return result;

    // Read 40 bits
    uint8_t data[5] = {};
    for (uint8_t i = 0; i < DATA_BITS; ++i) {
        uint32_t low = pulseWidth(false, 60);
        if (low == 0) return result;

        uint32_t high = pulseWidth(true, 80);
        if (high == 0) return result;

        // bit is 1 if high pulse > 40 µs, 0 otherwise
        if (high > 40) {
            data[i / 8] |= (1 << (7 - (i % 8)));
        }
    }

    // Checksum
    if (((data[0] + data[1] + data[2] + data[3]) & 0xFF) != data[4])
        return result;

    uint16_t rawH = (static_cast<uint16_t>(data[0]) << 8) | data[1];
    uint16_t rawT = (static_cast<uint16_t>(data[2] & 0x7F) << 8) | data[3];

    result.humidity    = rawH * 0.1f;
    result.temperature = rawT * 0.1f;
    if (data[2] & 0x80) result.temperature = -result.temperature;
    result.valid = true;

    return result;
}

uint32_t DHT22Driver::pulseWidth(bool state, uint32_t timeoutUs) {
    uint32_t count = 0;
    while (GPIO::read(_pin) != state) {
        if (++count > timeoutUs * 4) return 0; // ~1 iteration per 250 ns at 16 MHz
    }
    count = 0;
    while (GPIO::read(_pin) == state) {
        if (++count > timeoutUs * 4) return 0;
    }
    return count / 4; // approximate µs
}

} // namespace MikroDuino
