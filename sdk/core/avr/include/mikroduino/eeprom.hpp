#pragma once
/*
 * MikroDuino EEPROM Library
 *
 * Direct register access to the internal AVR EEPROM.
 * Provides byte, block, and templated typed read/write.
 * update() variants skip bytes that already match, preserving write cycles.
 * ATmega328P: exposes EEPROMMode (erase+write / erase-only / write-only).
 */

#include "platform.hpp"
#include "registers.hpp"
#include <avr/io.h>
#include <stdint.h>

// Modern avr-libc defines EEPE on all devices, but guard against toolchains
// that only expose the older EEWE / EEMWE spelling (ATmega32/16 headers).
#ifndef EEPE
#  define EEPE  EEWE
#  define EEMPE EEMWE
#endif

namespace MikroDuino {

// --------------------------------------------------------------------------
// Programming mode — only ATmega328P and devices with EEPM bits expose this.
// --------------------------------------------------------------------------
#if defined(EEPM0) && defined(EEPM1)
enum class EEPROMMode : uint8_t {
    EraseWrite = 0b00,  // Atomic erase+write — always safe, default (3.4 ms)
    EraseOnly  = 0b01,  // Erase cell to 0xFF without writing (1.8 ms)
    WriteOnly  = 0b10,  // Write without erasing — cell must already be 0xFF (1.8 ms)
};
#endif

// --------------------------------------------------------------------------
// EEPROMDriver
// --------------------------------------------------------------------------
class EEPROMDriver {
public:
    // Physical EEPROM size for the current target (bytes)
    static constexpr uint16_t capacity() { return static_cast<uint16_t>(MD_EEPROM_SIZE); }

    // True while an EEPROM write cycle is still in progress
    static bool busy() { return BITREAD(EECR, EEPE) != 0; }

    // Block until any in-progress write completes
    static void waitReady() { WAIT_BITCLEAR(EECR, EEPE); }

    // -----------------------------------------------------------------------
    // Single-byte access
    // -----------------------------------------------------------------------

    // Read one byte from addr
    static uint8_t read(uint16_t addr) {
        waitReady();
        EEAR = addr;
        BITSET(EECR, EERE);
        return EEDR;
    }

    // Write one byte — full erase+write cycle regardless of current value
    static void write(uint16_t addr, uint8_t data) {
        _write(addr, data);
    }

    // Write one byte only if it differs from the stored value (saves write cycles)
    static void update(uint16_t addr, uint8_t data) {
        if (read(addr) != data) _write(addr, data);
    }

    // Erase one cell to 0xFF
    static void erase(uint16_t addr) { _write(addr, 0xFF); }

    // -----------------------------------------------------------------------
    // Block access
    // -----------------------------------------------------------------------

    static void readBlock(uint16_t addr, void* buf, uint16_t len) {
        uint8_t* p = static_cast<uint8_t*>(buf);
        for (uint16_t i = 0; i < len; ++i)
            p[i] = read(addr + i);
    }

    static void writeBlock(uint16_t addr, const void* buf, uint16_t len) {
        const uint8_t* p = static_cast<const uint8_t*>(buf);
        for (uint16_t i = 0; i < len; ++i)
            write(addr + i, p[i]);
    }

    // Like writeBlock but skips bytes that already match the stored value
    static void updateBlock(uint16_t addr, const void* buf, uint16_t len) {
        const uint8_t* p = static_cast<const uint8_t*>(buf);
        for (uint16_t i = 0; i < len; ++i)
            update(addr + i, p[i]);
    }

    // -----------------------------------------------------------------------
    // Typed access — works with any trivially-copyable T (structs, scalars, arrays)
    // -----------------------------------------------------------------------

    // Read sizeof(T) bytes from addr into a T and return it
    template<typename T>
    static T get(uint16_t addr) {
        T val;
        readBlock(addr, &val, sizeof(T));
        return val;
    }

    // Write sizeof(T) bytes of val to addr (all bytes, unconditional)
    template<typename T>
    static void put(uint16_t addr, const T& val) {
        writeBlock(addr, &val, sizeof(T));
    }

    // Write sizeof(T) bytes of val to addr, skipping bytes that already match
    template<typename T>
    static void update(uint16_t addr, const T& val) {
        updateBlock(addr, &val, sizeof(T));
    }

    // -----------------------------------------------------------------------
    // EEPROM Ready interrupt
    // -----------------------------------------------------------------------
    static void enableInterrupt()  { BITSET(EECR, EERIE); }
    static void disableInterrupt() { BITCLEAR(EECR, EERIE); }

    // -----------------------------------------------------------------------
    // Programming mode (ATmega328P / devices with EEPM bits only)
    // -----------------------------------------------------------------------
#if defined(EEPM0) && defined(EEPM1)
    static void setProgrammingMode(EEPROMMode mode) {
        FIELD_SET(EECR, (1u << EEPM1) | (1u << EEPM0), EEPM0, static_cast<uint8_t>(mode));
    }
#endif

private:
    static void _write(uint16_t addr, uint8_t data) {
        waitReady();
        // If a self-programming (SPM) instruction is in progress, wait for it.
        // Needed only on devices with a boot loader section; safe to always check.
#if defined(SPMCSR) && defined(SPMEN)
        WAIT_BITCLEAR(SPMCSR, SPMEN);
#elif defined(SPMCR) && defined(SPMEN)
        WAIT_BITCLEAR(SPMCR, SPMEN);
#endif
        EEAR = addr;
        EEDR = data;
        // EEMPE must be set, then EEPE must be set within 4 CPU cycles.
        // Disable interrupts to guarantee the timing window is not violated.
        ATOMIC_BLOCK_START;
        BITSET(EECR, EEMPE);
        BITSET(EECR, EEPE);
        ATOMIC_BLOCK_END;
    }
};

// Convenience global instance — all methods are static so no state is held.
// Use as: MikroDuino::EEPROM.read(addr)  or  using namespace MikroDuino; EEPROM.read(addr)
static EEPROMDriver EEPROM __attribute__((unused));

} // namespace MikroDuino
