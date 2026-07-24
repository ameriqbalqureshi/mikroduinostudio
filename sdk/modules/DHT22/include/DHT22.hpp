#pragma once
/*
 * MikroDuino DHT22 (AM2302) Temperature & Humidity Sensor
 * 1-wire protocol implementation.
 */

#include <mikroduino/gpio.hpp>
#include <stdint.h>

namespace MikroDuino {

struct DHT22Data {
    float humidity;     // 0.0–100.0 %RH
    float temperature;  // -40.0–80.0 °C
    bool  valid;
};

class DHT22Driver {
public:
    explicit DHT22Driver(uint8_t pin) : _pin(pin) {}

    DHT22Data read();

private:
    uint8_t _pin;

    static constexpr uint8_t DATA_BITS = 40;

    bool readBit(uint32_t timeoutUs = 100);
    uint32_t pulseWidth(bool state, uint32_t timeoutUs);
};

} // namespace MikroDuino
