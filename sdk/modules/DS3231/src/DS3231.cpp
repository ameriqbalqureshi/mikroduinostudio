#include "../include/DS3231.hpp"

namespace MikroDuino {

void DS3231::begin() {
    // Ensure oscillator is running (clear OSF bit if set)
    uint8_t status = readReg(0x0F);
    if (status & 0x80) {
        writeReg(0x0F, status & ~0x80);
    }
}

DateTime DS3231::now() {
    uint8_t buf[7];
    uint8_t reg = 0x00;
    _i2c.writeRead(I2C_ADDRESS, &reg, 1, buf, 7);

    DateTime dt;
    dt.second    = bcdToDec(buf[0] & 0x7F);
    dt.minute    = bcdToDec(buf[1]);
    dt.hour      = bcdToDec(buf[2] & 0x3F); // 24h mode
    dt.dayOfWeek = buf[3];
    dt.day       = bcdToDec(buf[4]);
    dt.month     = bcdToDec(buf[5] & 0x7F);
    dt.year      = bcdToDec(buf[6]);
    return dt;
}

void DS3231::set(const DateTime& dt) {
    uint8_t buf[8];
    buf[0] = 0x00; // register address
    buf[1] = decToBcd(dt.second);
    buf[2] = decToBcd(dt.minute);
    buf[3] = decToBcd(dt.hour);   // 24h mode
    buf[4] = dt.dayOfWeek;
    buf[5] = decToBcd(dt.day);
    buf[6] = decToBcd(dt.month);
    buf[7] = decToBcd(dt.year);
    _i2c.write(I2C_ADDRESS, buf, 8);
}

int16_t DS3231::temperatureRaw() {
    uint8_t reg = 0x11;
    uint8_t buf[2];
    _i2c.writeRead(I2C_ADDRESS, &reg, 1, buf, 2);
    // MSB is integer part (signed), LSB bits 7:6 are fractional (0.25°C each)
    int16_t raw = static_cast<int16_t>((static_cast<int8_t>(buf[0]) << 2) | (buf[1] >> 6));
    return raw; // multiply by 0.25 for °C
}

float DS3231::temperature() {
    return temperatureRaw() * 0.25f;
}

bool DS3231::isRunning() {
    return !(readReg(0x0F) & 0x80);
}

uint8_t DS3231::readReg(uint8_t reg) {
    uint8_t val = 0;
    _i2c.writeRead(I2C_ADDRESS, &reg, 1, &val, 1);
    return val;
}

void DS3231::writeReg(uint8_t reg, uint8_t value) {
    uint8_t buf[2] = { reg, value };
    _i2c.write(I2C_ADDRESS, buf, 2);
}

} // namespace MikroDuino
