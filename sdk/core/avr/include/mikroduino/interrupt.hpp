#pragma once
/*
 * MikroDuino Interrupt Library
 *
 * External interrupt attachment with ISC sense mode control.
 * Global enable/disable. Everything explicit.
 */

#include "platform.hpp"
#include "registers.hpp"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>

// --------------------------------------------------------------------------
// avr/io.h defines INT0, INT1, INT2 … as integer macros (EIMSK bit positions).
// We need those same names for the IntSource enum, so we:
//   1. Save the bit positions as constexpr integers.
//   2. Undef the AVR macros.
//   3. Define the enum — INT0_vect, INT1_vect etc. (the ISR vectors) are
//      distinct tokens and are NOT affected by these undefs.
// --------------------------------------------------------------------------
namespace _avr_int_bits {
#ifdef INT0
    static constexpr uint8_t b0 = INT0;
#else
    static constexpr uint8_t b0 = 0;
#endif
#ifdef INT1
    static constexpr uint8_t b1 = INT1;
#else
    static constexpr uint8_t b1 = 1;
#endif
#ifdef INT2
    static constexpr uint8_t b2 = INT2;
#else
    static constexpr uint8_t b2 = 2;
#endif
#ifdef INT3
    static constexpr uint8_t b3 = INT3;
#else
    static constexpr uint8_t b3 = 3;
#endif
#ifdef INT4
    static constexpr uint8_t b4 = INT4;
#else
    static constexpr uint8_t b4 = 4;
#endif
#ifdef INT5
    static constexpr uint8_t b5 = INT5;
#else
    static constexpr uint8_t b5 = 5;
#endif
#ifdef INT6
    static constexpr uint8_t b6 = INT6;
#else
    static constexpr uint8_t b6 = 6;
#endif
#ifdef INT7
    static constexpr uint8_t b7 = INT7;
#else
    static constexpr uint8_t b7 = 7;
#endif
} // namespace _avr_int_bits

#undef INT0
#undef INT1
#undef INT2
#undef INT3
#undef INT4
#undef INT5
#undef INT6
#undef INT7

namespace MikroDuino {

enum class IntSource : uint8_t {
    INT0 = 0,
    INT1 = 1,
#if MD_EXT_INT_COUNT >= 3
    INT2 = 2,
#endif
#if MD_EXT_INT_COUNT >= 8
    INT3 = 3,
    INT4 = 4,
    INT5 = 5,
    INT6 = 6,
    INT7 = 7,
#endif
};

enum class IntSense : uint8_t {
    Low     = 0,  // Low level triggers
    Change  = 1,  // Any logical change
    Falling = 2,  // Falling edge
    Rising  = 3,  // Rising edge
};

using ISRHandler = void(*)();

// --------------------------------------------------------------------------
// User-registered handlers (called from ISR vectors in interrupt.cpp)
// --------------------------------------------------------------------------
namespace _int_handlers {
    extern ISRHandler fn0;
    extern ISRHandler fn1;
#if MD_EXT_INT_COUNT >= 3
    extern ISRHandler fn2;
#endif
}

class InterruptManager {
public:

    static void enableGlobal()  { sei(); }
    static void disableGlobal() { cli(); }

    static void attach(IntSource src, ISRHandler handler, IntSense sense = IntSense::Rising) {
        storeHandler(src, handler);
        setSense(src, sense);
        enable(src);
    }

    static void detach(IntSource src) {
        disable(src);
        storeHandler(src, nullptr);
    }

    static void enable(IntSource src) {
        switch (src) {
            case IntSource::INT0: BITSET(EIMSK, _avr_int_bits::b0); break;
            case IntSource::INT1: BITSET(EIMSK, _avr_int_bits::b1); break;
#if MD_EXT_INT_COUNT >= 3
            case IntSource::INT2: BITSET(EIMSK, _avr_int_bits::b2); break;
#endif
            default: break;
        }
    }

    static void disable(IntSource src) {
        switch (src) {
            case IntSource::INT0: BITCLEAR(EIMSK, _avr_int_bits::b0); break;
            case IntSource::INT1: BITCLEAR(EIMSK, _avr_int_bits::b1); break;
#if MD_EXT_INT_COUNT >= 3
            case IntSource::INT2: BITCLEAR(EIMSK, _avr_int_bits::b2); break;
#endif
            default: break;
        }
    }

    static void setSense(IntSource src, IntSense sense) {
        uint8_t senseVal = static_cast<uint8_t>(sense);
        switch (src) {
            case IntSource::INT0:
                FIELD_SET(EICRA, 0x03u, ISC00, senseVal); break;
            case IntSource::INT1:
                FIELD_SET(EICRA, 0x0Cu, ISC10, senseVal); break;
#if MD_EXT_INT_COUNT >= 3
            case IntSource::INT2:
                // ATmega32/16: INT2 sense is a single bit in MCUCSR
                BITWRITE(MCUCSR, ISC2, (senseVal & 0x01u)); break;
#endif
            default: break;
        }
    }

private:
    static void storeHandler(IntSource src, ISRHandler h) {
        switch (src) {
            case IntSource::INT0: _int_handlers::fn0 = h; break;
            case IntSource::INT1: _int_handlers::fn1 = h; break;
#if MD_EXT_INT_COUNT >= 3
            case IntSource::INT2: _int_handlers::fn2 = h; break;
#endif
            default: break;
        }
    }
};

// --------------------------------------------------------------------------
// Global-scope shorthand
// --------------------------------------------------------------------------
static InterruptManager Interrupt __attribute__((unused));

} // namespace MikroDuino
