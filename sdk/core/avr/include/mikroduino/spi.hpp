#pragma once
/*
 * MikroDuino SPI Library
 *
 * Full hardware SPI. Master and slave mode.
 * No hidden CS management — user controls chip select.
 */

#include "platform.hpp"
#include "registers.hpp"
#include "gpio.hpp"
#include <avr/io.h>
#include <stdint.h>

namespace MikroDuino {

enum class SPIClockDiv : uint8_t {
    DIV2   = 4,   // SPSR.SPI2X=1, SPCR.SPR=00
    DIV4   = 0,   // SPSR.SPI2X=0, SPCR.SPR=00
    DIV8   = 5,   // SPSR.SPI2X=1, SPCR.SPR=01
    DIV16  = 1,
    DIV32  = 6,
    DIV64  = 2,
    DIV128 = 3,
};

enum class SPIMode : uint8_t {
    Mode0 = 0,  // CPOL=0, CPHA=0
    Mode1 = 1,  // CPOL=0, CPHA=1
    Mode2 = 2,  // CPOL=1, CPHA=0
    Mode3 = 3,  // CPOL=1, CPHA=1
};

enum class SPIBitOrder : uint8_t {
    MSBFirst = 0,
    LSBFirst = 1,
};

class SPIDriver {
public:
    void beginMaster(
        SPIClockDiv clockDiv = SPIClockDiv::DIV16,
        SPIMode mode         = SPIMode::Mode0,
        SPIBitOrder order    = SPIBitOrder::MSBFirst)
    {
        // Configure SPI pins (MOSI, SCK, SS as output; MISO as input)
        GPIO::output(PB3); // MOSI
        GPIO::output(PB5); // SCK
        GPIO::output(PB2); // SS (must be output in master mode)
        GPIO::input(PB4);  // MISO

        applyConfig(true, clockDiv, mode, order);
    }

    void beginSlave(
        SPIMode mode      = SPIMode::Mode0,
        SPIBitOrder order = SPIBitOrder::MSBFirst)
    {
        GPIO::input(PB3);  // MOSI
        GPIO::input(PB5);  // SCK
        GPIO::input(PB2);  // SS
        GPIO::output(PB4); // MISO

        applyConfig(false, SPIClockDiv::DIV4, mode, order);
    }

    void end() {
        BITCLEAR(SPCR, SPE);
    }

    uint8_t transfer(uint8_t data) {
        SPDR = data;
        WAIT_BITSET(SPSR, SPIF);
        return SPDR;
    }

    void transfer(const uint8_t* txBuf, uint8_t* rxBuf, uint16_t len) {
        for (uint16_t i = 0; i < len; ++i) {
            rxBuf[i] = transfer(txBuf[i]);
        }
    }

    void enableInterrupt()  { BITSET(SPCR, SPIE); }
    void disableInterrupt() { BITCLEAR(SPCR, SPIE); }

    bool transferComplete() const { return BITREAD(SPSR, SPIF) != 0; }

private:
    void applyConfig(bool master, SPIClockDiv div, SPIMode mode, SPIBitOrder order) {
        uint8_t spcr = (1<<SPE);
        if (master)  spcr |= (1<<MSTR);
        if (order == SPIBitOrder::LSBFirst) spcr |= (1<<DORD);

        uint8_t divVal = static_cast<uint8_t>(div);
        spcr |= (divVal & 0x03u) << SPR0;

        uint8_t modeVal = static_cast<uint8_t>(mode);
        if (modeVal & 0x01u) spcr |= (1<<CPHA);
        if (modeVal & 0x02u) spcr |= (1<<CPOL);

        SPCR = spcr;
        if (divVal & 0x04u) BITSET(SPSR, SPI2X);
        else                 BITCLEAR(SPSR, SPI2X);
    }
};

static SPIDriver SPI __attribute__((unused));

} // namespace MikroDuino
