/*
 * SPI.cpp — Arduino SPI compatibility layer for MikroDuino.
 * Maps SPIClass to MikroDuino::SPIDriver.
 */

#include "../include/SPI.h"
#include <mikroduino/spi.hpp>

using namespace MikroDuino;

SPIClass SPI;

// ── Internal mapping helpers ──────────────────────────────────────────────────

static SPIClockDiv _resolveDiv(uint32_t clock) {
    // Select the fastest divider that keeps SPI clock <= requested clock
    uint32_t f = F_CPU;
    if (clock >= f / 2)   return SPIClockDiv::DIV2;
    if (clock >= f / 4)   return SPIClockDiv::DIV4;
    if (clock >= f / 8)   return SPIClockDiv::DIV8;
    if (clock >= f / 16)  return SPIClockDiv::DIV16;
    if (clock >= f / 32)  return SPIClockDiv::DIV32;
    if (clock >= f / 64)  return SPIClockDiv::DIV64;
    return SPIClockDiv::DIV128;
}

static SPIClockDiv _divFromConst(uint8_t div) {
    switch (div) {
        case SPI_CLOCK_DIV2:   return SPIClockDiv::DIV2;
        case SPI_CLOCK_DIV4:   return SPIClockDiv::DIV4;
        case SPI_CLOCK_DIV8:   return SPIClockDiv::DIV8;
        case SPI_CLOCK_DIV16:  return SPIClockDiv::DIV16;
        case SPI_CLOCK_DIV32:  return SPIClockDiv::DIV32;
        case SPI_CLOCK_DIV64:  return SPIClockDiv::DIV64;
        case SPI_CLOCK_DIV128: return SPIClockDiv::DIV128;
        default:               return SPIClockDiv::DIV16;
    }
}

static SPIMode _modeFrom(uint8_t mode) {
    switch (mode) {
        case SPI_MODE0: return SPIMode::Mode0;
        case SPI_MODE1: return SPIMode::Mode1;
        case SPI_MODE2: return SPIMode::Mode2;
        case SPI_MODE3: return SPIMode::Mode3;
        default:        return SPIMode::Mode0;
    }
}

static SPIBitOrder _orderFrom(uint8_t order) {
    return (order == LSBFIRST) ? SPIBitOrder::LSBFirst : SPIBitOrder::MSBFirst;
}

// ── SPIClass ──────────────────────────────────────────────────────────────────

void SPIClass::_apply(SPISettings& s) {
    MikroDuino::SPI.beginMaster(
        _resolveDiv(s._clock),
        _modeFrom(s._dataMode),
        _orderFrom(s._bitOrder)
    );
}

void SPIClass::begin() {
    MikroDuino::SPI.beginMaster(
        SPIClockDiv::DIV16,
        SPIMode::Mode0,
        SPIBitOrder::MSBFirst
    );
}

void SPIClass::end() {
    MikroDuino::SPI.end();
}

void SPIClass::beginTransaction(SPISettings settings) {
    _settings = settings;
    _apply(_settings);
}

void SPIClass::endTransaction() {
    // No state to release — the hardware SPI stays configured until begin() or end()
}

uint8_t SPIClass::transfer(uint8_t data) {
    return MikroDuino::SPI.transfer(data);
}

uint16_t SPIClass::transfer16(uint16_t data) {
    // MSB first: high byte, then low byte
    uint16_t high = MikroDuino::SPI.transfer(static_cast<uint8_t>(data >> 8));
    uint16_t low  = MikroDuino::SPI.transfer(static_cast<uint8_t>(data & 0xFF));
    return static_cast<uint16_t>((high << 8) | low);
}

void SPIClass::transfer(void* buf, size_t count) {
    uint8_t* p = static_cast<uint8_t*>(buf);
    for (size_t i = 0; i < count; ++i)
        p[i] = MikroDuino::SPI.transfer(p[i]);
}

void SPIClass::setBitOrder(uint8_t bitOrder) {
    _settings._bitOrder = bitOrder;
    _apply(_settings);
}

void SPIClass::setDataMode(uint8_t dataMode) {
    _settings._dataMode = dataMode;
    _apply(_settings);
}

void SPIClass::setClockDivider(uint8_t divider) {
    MikroDuino::SPI.beginMaster(
        _divFromConst(divider),
        _modeFrom(_settings._dataMode),
        _orderFrom(_settings._bitOrder)
    );
}

void SPIClass::setClockDivider(uint8_t /*divider*/, uint8_t /*clockDiv2x*/) {
    // Legacy two-arg form — ignore and use single-arg version
    setClockDivider(_settings._dataMode);
}
