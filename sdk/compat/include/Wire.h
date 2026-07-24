#pragma once
/*
 * Arduino-compatible Wire (I2C) library for MikroDuino.
 * Wraps MikroDuino::I2CDriver with the standard Arduino Wire API.
 *
 * Buffer size: 32 bytes per transaction (matches Arduino's Wire library).
 * Slave mode ISR callbacks (onReceive / onRequest) are registered but
 * not automatically dispatched — call onReceive() / onRequest() yourself
 * from the appropriate TWI ISR if you need slave-mode operation.
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define BUFFER_LENGTH 32

class TwoWire {
public:
    TwoWire();

    // ── Initialisation ────────────────────────────────────────────────────────
    void begin();                       // master @ 100 kHz
    void begin(uint8_t addr);           // slave with address
    void begin(int addr);
    void end();
    void setClock(uint32_t freq);       // change bus speed (master only)

    // ── Master transmit ───────────────────────────────────────────────────────
    void    beginTransmission(uint8_t addr);
    void    beginTransmission(int addr) { beginTransmission(static_cast<uint8_t>(addr)); }
    uint8_t endTransmission(bool sendStop = true);
    uint8_t endTransmission(uint8_t sendStop) { return endTransmission(static_cast<bool>(sendStop)); }

    // ── Master receive ────────────────────────────────────────────────────────
    uint8_t requestFrom(uint8_t addr, uint8_t qty, uint8_t sendStop);
    uint8_t requestFrom(uint8_t addr, uint8_t qty)         { return requestFrom(addr, qty, static_cast<uint8_t>(true)); }
    uint8_t requestFrom(int addr, int qty)                  { return requestFrom(static_cast<uint8_t>(addr), static_cast<uint8_t>(qty)); }
    uint8_t requestFrom(int addr, int qty, int sendStop)    { return requestFrom(static_cast<uint8_t>(addr), static_cast<uint8_t>(qty), static_cast<uint8_t>(sendStop)); }

    // ── Write (buffered) ──────────────────────────────────────────────────────
    size_t write(uint8_t data);
    size_t write(const uint8_t* data, size_t qty);
    size_t write(const char* s) { return write(reinterpret_cast<const uint8_t*>(s), strlen(s)); }
    size_t write(int v)         { return write(static_cast<uint8_t>(v)); }

    // ── Read ─────────────────────────────────────────────────────────────────
    int  available();
    int  read();
    int  peek();
    void flush();

    // ── Slave callbacks ───────────────────────────────────────────────────────
    void onReceive(void (*cb)(int));
    void onRequest(void (*cb)(void));

private:
    uint8_t  _txAddr;
    uint8_t  _txBuf[BUFFER_LENGTH];
    uint8_t  _txLen;

    uint8_t  _rxBuf[BUFFER_LENGTH];
    uint8_t  _rxLen;
    uint8_t  _rxIdx;

    void (*_onReceiveCb)(int);
    void (*_onRequestCb)(void);
};

extern TwoWire Wire;
