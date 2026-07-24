#pragma once
/*
 * MikroDuino DS3231 RTC Module
 * Real-time clock and temperature via I2C.
 */

#include <mikroduino/i2c.hpp>
#include <stdint.h>

namespace MikroDuino {

struct DateTime {
    uint8_t second;     // 0-59
    uint8_t minute;     // 0-59
    uint8_t hour;       // 0-23
    uint8_t dayOfWeek;  // 1-7 (1=Sunday)
    uint8_t day;        // 1-31
    uint8_t month;      // 1-12
    uint8_t year;       // 0-99 (offset from 2000)
};

class DS3231 {
public:
    static constexpr uint8_t I2C_ADDRESS = 0x68;

    explicit DS3231(I2CDriver& i2c = I2C) : _i2c(i2c) {}

    void begin();

    DateTime now();
    void     set(const DateTime& dt);

    // Temperature in 0.25 °C units (divide by 4.0 for °C)
    int16_t temperatureRaw();
    float   temperature();

    bool isRunning();

private:
    I2CDriver& _i2c;

    static uint8_t bcdToDec(uint8_t bcd) { return (bcd >> 4) * 10 + (bcd & 0x0F); }
    static uint8_t decToBcd(uint8_t dec) { return ((dec / 10) << 4) | (dec % 10); }

    uint8_t readReg(uint8_t reg);
    void    writeReg(uint8_t reg, uint8_t value);
};

} // namespace MikroDuino
