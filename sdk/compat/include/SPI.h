#pragma once
/*
 * Arduino-compatible SPI library for MikroDuino.
 * Wraps MikroDuino::SPIDriver. Drop-in replacement for Arduino's SPI.h.
 *
 * Most third-party libraries control their own CS pin. This wrapper
 * does not manage CS — set it high/low in your sketch as usual.
 */

#include <stdint.h>
#include <stddef.h>

#ifndef LSBFIRST
#define LSBFIRST 0
#define MSBFIRST 1
#endif

// ── SPI mode constants ────────────────────────────────────────────────────────
#define SPI_MODE0 0x00   // CPOL=0, CPHA=0
#define SPI_MODE1 0x04   // CPOL=0, CPHA=1
#define SPI_MODE2 0x08   // CPOL=1, CPHA=0
#define SPI_MODE3 0x0C   // CPOL=1, CPHA=1

// ── Clock divider constants ───────────────────────────────────────────────────
#define SPI_CLOCK_DIV2    0x04
#define SPI_CLOCK_DIV4    0x00
#define SPI_CLOCK_DIV8    0x05
#define SPI_CLOCK_DIV16   0x01
#define SPI_CLOCK_DIV32   0x06
#define SPI_CLOCK_DIV64   0x02
#define SPI_CLOCK_DIV128  0x03

// ── SPISettings ───────────────────────────────────────────────────────────────

struct SPISettings {
    SPISettings() : _clock(1000000UL), _bitOrder(MSBFIRST), _dataMode(SPI_MODE0) {}

    SPISettings(uint32_t clock, uint8_t bitOrder, uint8_t dataMode)
        : _clock(clock), _bitOrder(bitOrder), _dataMode(dataMode) {}

    uint32_t _clock;
    uint8_t  _bitOrder;
    uint8_t  _dataMode;
};

// ── SPIClass ──────────────────────────────────────────────────────────────────

class SPIClass {
public:
    SPIClass() : _settings() {}

    void begin();
    void end();

    // Arduino SPI transaction API
    void beginTransaction(SPISettings settings);
    void endTransaction();

    // Transfer one byte, return received byte
    uint8_t  transfer(uint8_t data);
    // Transfer two bytes (MSB first), return received word
    uint16_t transfer16(uint16_t data);
    // In-place transfer: each buf[i] is sent and overwritten with the received byte
    void     transfer(void* buf, size_t count);

    // Legacy API (pre-transaction)
    void setBitOrder(uint8_t bitOrder);
    void setDataMode(uint8_t dataMode);
    void setClockDivider(uint8_t divider);
    void setClockDivider(uint8_t divider, uint8_t clockDiv2x);

private:
    SPISettings _settings;

    void _apply(SPISettings& s);
};

extern SPIClass SPI;
