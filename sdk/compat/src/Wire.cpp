/*
 * Wire.cpp — Arduino Wire (I2C) compatibility layer for MikroDuino.
 * Maps TwoWire to MikroDuino::I2CDriver.
 *
 * endTransmission() return codes (same as Arduino):
 *   0 = success
 *   1 = transmit buffer overflow
 *   2 = NACK on address
 *   3 = NACK on data
 *   4 = other error
 *   5 = timeout
 */

#include "../include/Wire.h"
#include <mikroduino/i2c.hpp>

using namespace MikroDuino;

TwoWire Wire;

TwoWire::TwoWire()
    : _txAddr(0), _txLen(0), _rxLen(0), _rxIdx(0),
      _onReceiveCb(nullptr), _onRequestCb(nullptr) {}

// ── Initialisation ────────────────────────────────────────────────────────────

void TwoWire::begin() {
    I2C.beginMaster(100000UL);
}

void TwoWire::begin(uint8_t addr) {
    I2C.beginSlave(addr);
}

void TwoWire::begin(int addr) {
    begin(static_cast<uint8_t>(addr));
}

void TwoWire::end() {
    // No shutdown method on I2CDriver — disable TWI by clearing TWEN
    TWCR = 0;
}

void TwoWire::setClock(uint32_t freq) {
    I2C.beginMaster(freq);
}

// ── Master transmit ───────────────────────────────────────────────────────────

void TwoWire::beginTransmission(uint8_t addr) {
    _txAddr = addr;
    _txLen  = 0;
}

uint8_t TwoWire::endTransmission(bool sendStop) {
    if (_txLen > BUFFER_LENGTH) return 1;   // overflow

    I2CResult r = I2C.write(_txAddr, _txBuf, _txLen, sendStop);
    _txLen = 0;

    switch (r) {
        case I2CResult::Ok:          return 0;
        case I2CResult::NackAddress: return 2;
        case I2CResult::NackData:    return 3;
        case I2CResult::Timeout:     return 5;
        default:                     return 4;
    }
}

// ── Master receive ────────────────────────────────────────────────────────────

uint8_t TwoWire::requestFrom(uint8_t addr, uint8_t qty, uint8_t sendStop) {
    if (qty > BUFFER_LENGTH) qty = BUFFER_LENGTH;

    I2CResult r = I2C.read(addr, _rxBuf, qty, static_cast<bool>(sendStop));
    if (r != I2CResult::Ok) {
        _rxLen = 0;
        _rxIdx = 0;
        return 0;
    }
    _rxLen = qty;
    _rxIdx = 0;
    return qty;
}

// ── Write (buffered into _txBuf) ──────────────────────────────────────────────

size_t TwoWire::write(uint8_t data) {
    if (_txLen >= BUFFER_LENGTH) return 0;
    _txBuf[_txLen++] = data;
    return 1;
}

size_t TwoWire::write(const uint8_t* data, size_t qty) {
    size_t written = 0;
    for (size_t i = 0; i < qty && _txLen < BUFFER_LENGTH; ++i) {
        _txBuf[_txLen++] = data[i];
        ++written;
    }
    return written;
}

// ── Read (from _rxBuf filled by requestFrom) ──────────────────────────────────

int TwoWire::available() {
    return static_cast<int>(_rxLen) - static_cast<int>(_rxIdx);
}

int TwoWire::read() {
    if (_rxIdx >= _rxLen) return -1;
    return _rxBuf[_rxIdx++];
}

int TwoWire::peek() {
    if (_rxIdx >= _rxLen) return -1;
    return _rxBuf[_rxIdx];
}

void TwoWire::flush() {
    _rxIdx = _rxLen;    // discard unread data
}

// ── Slave callbacks ───────────────────────────────────────────────────────────

void TwoWire::onReceive(void (*cb)(int))  { _onReceiveCb = cb; }
void TwoWire::onRequest(void (*cb)(void)) { _onRequestCb = cb; }
