#pragma once
/*
 * MikroDuino I2C (TWI) Library
 *
 * Hardware TWI. Master and slave modes. Repeated start. Bus scan.
 * No hidden state machine outside of what you explicitly call.
 */

#include "platform.hpp"
#include "registers.hpp"
#include <avr/io.h>
#include <stdint.h>

namespace MikroDuino {

// TWI status codes
namespace TWIStatus {
    constexpr uint8_t StartSent         = 0x08;
    constexpr uint8_t RepStartSent      = 0x10;
    constexpr uint8_t SLAWAck          = 0x18;
    constexpr uint8_t SLAWNack         = 0x20;
    constexpr uint8_t DataWriteAck     = 0x28;
    constexpr uint8_t DataWriteNack    = 0x30;
    constexpr uint8_t SLARAAck         = 0x40;
    constexpr uint8_t SLARANack        = 0x48;
    constexpr uint8_t DataReadAck      = 0x50;
    constexpr uint8_t DataReadNack     = 0x58;
    constexpr uint8_t SlaveRxSLAW      = 0x60;
    constexpr uint8_t SlaveRxData      = 0x80;
    constexpr uint8_t SlaveTxSLAR      = 0xA8;
    constexpr uint8_t SlaveTxData      = 0xB8;
    constexpr uint8_t BusError         = 0x00;
}

enum class I2CResult : uint8_t {
    Ok,
    Timeout,
    NackAddress,
    NackData,
    BusError,
    ArbitrationLost,
};

class I2CDriver {
public:
    // ---------- Master ----------
    void beginMaster(uint32_t clockHz = 100000UL) {
        // TWBR = (F_CPU / clockHz - 16) / (2 * prescaler)
        // Using prescaler = 1 (TWSR bits = 0)
        TWSR = 0x00;
        TWBR = static_cast<uint8_t>((F_CPU / clockHz - 16UL) / 2UL);
        TWCR = (1<<TWEN);
    }

    // Send START → SLA+W → data bytes → STOP
    I2CResult write(uint8_t address, const uint8_t* data, uint8_t len, bool stop = true) {
        I2CResult res = sendStart();
        if (res != I2CResult::Ok) return res;

        res = sendSLA((address << 1) | 0x00); // write
        if (res != I2CResult::Ok) { sendStop(); return res; }

        for (uint8_t i = 0; i < len; ++i) {
            res = sendByte(data[i]);
            if (res != I2CResult::Ok) { sendStop(); return res; }
        }

        if (stop) sendStop();
        return I2CResult::Ok;
    }

    // Send START → SLA+R → receive bytes → STOP
    I2CResult read(uint8_t address, uint8_t* buf, uint8_t len, bool stop = true) {
        I2CResult res = sendStart();
        if (res != I2CResult::Ok) return res;

        res = sendSLA((address << 1) | 0x01); // read
        if (res != I2CResult::Ok) { sendStop(); return res; }

        for (uint8_t i = 0; i < len; ++i) {
            bool ack = (i < len - 1);
            res = receiveByte(buf[i], ack);
            if (res != I2CResult::Ok) { sendStop(); return res; }
        }

        if (stop) sendStop();
        return I2CResult::Ok;
    }

    // Write then read (combined transaction with repeated start)
    I2CResult writeRead(uint8_t addr, const uint8_t* wBuf, uint8_t wLen,
                        uint8_t* rBuf, uint8_t rLen)
    {
        I2CResult res = write(addr, wBuf, wLen, false); // no stop
        if (res != I2CResult::Ok) return res;
        return read(addr, rBuf, rLen, true);
    }

    // ---- Streaming write (for transfers larger than 255 bytes) ----
    // Open a write transaction. Follow with transmit() calls, then endTransmit().
    I2CResult beginTransmit(uint8_t address) {
        I2CResult res = sendStart();
        if (res != I2CResult::Ok) return res;
        return sendSLA(static_cast<uint8_t>((address << 1) | 0x00));
    }

    // Send one byte within an open transaction.
    I2CResult transmit(uint8_t b) { return sendByte(b); }

    // Close the transaction.
    void endTransmit() { sendStop(); }

    // Scan bus: calls cb(address) for each device that ACKs
    template<typename Callback>
    void scan(Callback cb) {
        for (uint8_t addr = 1; addr < 127; ++addr) {
            I2CResult res = sendStart();
            if (res != I2CResult::Ok) continue;
            if (sendSLA((addr << 1) | 0x00) == I2CResult::Ok) {
                cb(addr);
            }
            sendStop();
        }
    }

    // ---------- Slave ----------
    void beginSlave(uint8_t address, bool generalCall = false) {
        TWAR = static_cast<uint8_t>((address << 1) | (generalCall ? 1 : 0));
        TWCR = (1<<TWEN) | (1<<TWEA) | (1<<TWINT);
    }

    void enableInterrupt()  { BITSET(TWCR, TWIE); }
    void disableInterrupt() { BITCLEAR(TWCR, TWIE); }

    uint8_t status() const { return TWSR & 0xF8u; }

private:
    static constexpr uint16_t TIMEOUT_CYCLES = 10000;

    I2CResult waitReady() {
        uint16_t t = TIMEOUT_CYCLES;
        while (!(TWCR & (1<<TWINT)) && --t);
        return t ? I2CResult::Ok : I2CResult::Timeout;
    }

    I2CResult sendStart() {
        TWCR = (1<<TWINT) | (1<<TWSTA) | (1<<TWEN);
        I2CResult res = waitReady();
        if (res != I2CResult::Ok) return res;
        uint8_t s = status();
        if (s != TWIStatus::StartSent && s != TWIStatus::RepStartSent)
            return I2CResult::BusError;
        return I2CResult::Ok;
    }

    void sendStop() {
        TWCR = (1<<TWINT) | (1<<TWSTO) | (1<<TWEN);
        uint16_t t = TIMEOUT_CYCLES;
        while ((TWCR & (1<<TWSTO)) && --t);
    }

    I2CResult sendSLA(uint8_t sla) {
        TWDR = sla;
        TWCR = (1<<TWINT) | (1<<TWEN);
        I2CResult res = waitReady();
        if (res != I2CResult::Ok) return res;
        uint8_t s = status();
        if (s == TWIStatus::SLAWAck || s == TWIStatus::SLARAAck)
            return I2CResult::Ok;
        if (s == TWIStatus::SLAWNack || s == TWIStatus::SLARANack)
            return I2CResult::NackAddress;
        return I2CResult::BusError;
    }

    I2CResult sendByte(uint8_t data) {
        TWDR = data;
        TWCR = (1<<TWINT) | (1<<TWEN);
        I2CResult res = waitReady();
        if (res != I2CResult::Ok) return res;
        if (status() != TWIStatus::DataWriteAck) return I2CResult::NackData;
        return I2CResult::Ok;
    }

    I2CResult receiveByte(uint8_t& out, bool ack) {
        TWCR = (1<<TWINT) | (1<<TWEN) | (ack ? (1<<TWEA) : 0);
        I2CResult res = waitReady();
        if (res != I2CResult::Ok) return res;
        uint8_t s = status();
        if (s != TWIStatus::DataReadAck && s != TWIStatus::DataReadNack)
            return I2CResult::BusError;
        out = TWDR;
        return I2CResult::Ok;
    }
};

static I2CDriver I2C __attribute__((unused));

} // namespace MikroDuino
