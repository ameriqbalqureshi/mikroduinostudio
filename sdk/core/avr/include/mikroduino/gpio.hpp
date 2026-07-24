#pragma once
/*
 * MikroDuino GPIO Library
 *
 * Full hardware control. No hidden state.
 * Pin encoding: bits[5:3] = port index (0=A,1=B,...,6=G), bits[2:0] = bit number.
 */

#include "platform.hpp"
#include "registers.hpp"
#include <avr/io.h>
#include <stdint.h>

namespace MikroDuino {

// --------------------------------------------------------------------------
// Pin encoding helpers
// --------------------------------------------------------------------------
constexpr uint8_t _PIN(uint8_t port, uint8_t bit) noexcept {
    return static_cast<uint8_t>((port << 3) | (bit & 0x07u));
}
constexpr uint8_t _PIN_PORT(uint8_t p) noexcept { return p >> 3; }
constexpr uint8_t _PIN_BIT(uint8_t p)  noexcept { return p & 0x07u; }

// --------------------------------------------------------------------------
// Pin constants
// --------------------------------------------------------------------------
// avr/io.h defines PB0-PD7 etc. as raw integer macros (bit positions within
// the port register). We need to undefine them before declaring our own
// constexpr versions that carry both port and bit information.
#undef PA0
#undef PA1
#undef PA2
#undef PA3
#undef PA4
#undef PA5
#undef PA6
#undef PA7

#undef PB0
#undef PB1
#undef PB2
#undef PB3
#undef PB4
#undef PB5
#undef PB6
#undef PB7

#undef PC0
#undef PC1
#undef PC2
#undef PC3
#undef PC4
#undef PC5
#undef PC6
#undef PC7

#undef PD0
#undef PD1
#undef PD2
#undef PD3
#undef PD4
#undef PD5
#undef PD6
#undef PD7

#undef PE0
#undef PE1
#undef PE2
#undef PE3
#undef PE4
#undef PE5
#undef PE6
#undef PE7

#undef PF0
#undef PF1
#undef PF2
#undef PF3
#undef PF4
#undef PF5
#undef PF6
#undef PF7

#undef PG0
#undef PG1
#undef PG2
#undef PG3
#undef PG4

#if MD_HAS_PORTA
constexpr uint8_t PA0 = _PIN(0,0); constexpr uint8_t PA1 = _PIN(0,1);
constexpr uint8_t PA2 = _PIN(0,2); constexpr uint8_t PA3 = _PIN(0,3);
constexpr uint8_t PA4 = _PIN(0,4); constexpr uint8_t PA5 = _PIN(0,5);
constexpr uint8_t PA6 = _PIN(0,6); constexpr uint8_t PA7 = _PIN(0,7);
#endif

constexpr uint8_t PB0 = _PIN(1,0); constexpr uint8_t PB1 = _PIN(1,1);
constexpr uint8_t PB2 = _PIN(1,2); constexpr uint8_t PB3 = _PIN(1,3);
constexpr uint8_t PB4 = _PIN(1,4); constexpr uint8_t PB5 = _PIN(1,5);
constexpr uint8_t PB6 = _PIN(1,6); constexpr uint8_t PB7 = _PIN(1,7);

constexpr uint8_t PC0 = _PIN(2,0); constexpr uint8_t PC1 = _PIN(2,1);
constexpr uint8_t PC2 = _PIN(2,2); constexpr uint8_t PC3 = _PIN(2,3);
constexpr uint8_t PC4 = _PIN(2,4); constexpr uint8_t PC5 = _PIN(2,5);
constexpr uint8_t PC6 = _PIN(2,6); constexpr uint8_t PC7 = _PIN(2,7);

constexpr uint8_t PD0 = _PIN(3,0); constexpr uint8_t PD1 = _PIN(3,1);
constexpr uint8_t PD2 = _PIN(3,2); constexpr uint8_t PD3 = _PIN(3,3);
constexpr uint8_t PD4 = _PIN(3,4); constexpr uint8_t PD5 = _PIN(3,5);
constexpr uint8_t PD6 = _PIN(3,6); constexpr uint8_t PD7 = _PIN(3,7);

#if MD_HAS_PORTE
constexpr uint8_t PE0 = _PIN(4,0); constexpr uint8_t PE1 = _PIN(4,1);
constexpr uint8_t PE2 = _PIN(4,2); constexpr uint8_t PE3 = _PIN(4,3);
constexpr uint8_t PE4 = _PIN(4,4); constexpr uint8_t PE5 = _PIN(4,5);
constexpr uint8_t PE6 = _PIN(4,6); constexpr uint8_t PE7 = _PIN(4,7);
#endif

#if MD_HAS_PORTF
constexpr uint8_t PF0 = _PIN(5,0); constexpr uint8_t PF1 = _PIN(5,1);
constexpr uint8_t PF2 = _PIN(5,2); constexpr uint8_t PF3 = _PIN(5,3);
constexpr uint8_t PF4 = _PIN(5,4); constexpr uint8_t PF5 = _PIN(5,5);
constexpr uint8_t PF6 = _PIN(5,6); constexpr uint8_t PF7 = _PIN(5,7);
#endif

#if MD_HAS_PORTG
constexpr uint8_t PG0 = _PIN(6,0); constexpr uint8_t PG1 = _PIN(6,1);
constexpr uint8_t PG2 = _PIN(6,2); constexpr uint8_t PG3 = _PIN(6,3);
constexpr uint8_t PG4 = _PIN(6,4);
#endif

// --------------------------------------------------------------------------
// Internal: resolve port index to DDR/PORT/PIN registers
// --------------------------------------------------------------------------
namespace _gpio_impl {

MD_INLINE volatile uint8_t* ddr(uint8_t port) {
    switch (port) {
#if MD_HAS_PORTA
        case 0: return &DDRA;
#endif
        case 1: return &DDRB;
        case 2: return &DDRC;
        case 3: return &DDRD;
#if MD_HAS_PORTE
        case 4: return &DDRE;
#endif
#if MD_HAS_PORTF
        case 5: return &DDRF;
#endif
#if MD_HAS_PORTG
        case 6: return &DDRG;
#endif
        default: return nullptr;
    }
}

MD_INLINE volatile uint8_t* port(uint8_t portIdx) {
    switch (portIdx) {
#if MD_HAS_PORTA
        case 0: return &PORTA;
#endif
        case 1: return &PORTB;
        case 2: return &PORTC;
        case 3: return &PORTD;
#if MD_HAS_PORTE
        case 4: return &PORTE;
#endif
#if MD_HAS_PORTF
        case 5: return &PORTF;
#endif
#if MD_HAS_PORTG
        case 6: return &PORTG;
#endif
        default: return nullptr;
    }
}

MD_INLINE volatile uint8_t* pin(uint8_t portIdx) {
    switch (portIdx) {
#if MD_HAS_PORTA
        case 0: return &PINA;
#endif
        case 1: return &PINB;
        case 2: return &PINC;
        case 3: return &PIND;
#if MD_HAS_PORTE
        case 4: return &PINE;
#endif
#if MD_HAS_PORTF
        case 5: return &PINF;
#endif
#if MD_HAS_PORTG
        case 6: return &PING;
#endif
        default: return nullptr;
    }
}

} // namespace _gpio_impl

// --------------------------------------------------------------------------
// GPIO public API
// --------------------------------------------------------------------------
class GPIO {
public:
    GPIO() = delete;

    MD_INLINE static void output(uint8_t pin) {
        volatile uint8_t* r = _gpio_impl::ddr(_PIN_PORT(pin));
        if (r) BITSET(*r, _PIN_BIT(pin));
    }

    MD_INLINE static void input(uint8_t pin) {
        volatile uint8_t* d = _gpio_impl::ddr(_PIN_PORT(pin));
        volatile uint8_t* p = _gpio_impl::port(_PIN_PORT(pin));
        if (d) BITCLEAR(*d, _PIN_BIT(pin));
        if (p) BITCLEAR(*p, _PIN_BIT(pin)); // disable pull-up
    }

    MD_INLINE static void inputPullup(uint8_t pin) {
        volatile uint8_t* d = _gpio_impl::ddr(_PIN_PORT(pin));
        volatile uint8_t* p = _gpio_impl::port(_PIN_PORT(pin));
        if (d) BITCLEAR(*d, _PIN_BIT(pin));
        if (p) BITSET(*p, _PIN_BIT(pin));   // enable pull-up
    }

    MD_INLINE static void set(uint8_t pin) {
        volatile uint8_t* r = _gpio_impl::port(_PIN_PORT(pin));
        if (r) BITSET(*r, _PIN_BIT(pin));
    }

    MD_INLINE static void clear(uint8_t pin) {
        volatile uint8_t* r = _gpio_impl::port(_PIN_PORT(pin));
        if (r) BITCLEAR(*r, _PIN_BIT(pin));
    }

    MD_INLINE static void toggle(uint8_t pin) {
        // On AVR, writing 1 to PINx register toggles the output (fast toggle)
        volatile uint8_t* r = _gpio_impl::pin(_PIN_PORT(pin));
        if (r) BITSET(*r, _PIN_BIT(pin));
    }

    MD_INLINE static bool read(uint8_t pin) {
        volatile uint8_t* r = _gpio_impl::pin(_PIN_PORT(pin));
        return r ? static_cast<bool>(BITREAD(*r, _PIN_BIT(pin))) : false;
    }

    MD_INLINE static void write(uint8_t pin, bool value) {
        if (value) set(pin); else clear(pin);
    }

    // Batch: configure all pins in portMask as output on the given port index
    static void portOutput(uint8_t portIdx, uint8_t mask) {
        volatile uint8_t* r = _gpio_impl::ddr(portIdx);
        if (r) SETMASK(*r, mask);
    }

    static void portInput(uint8_t portIdx, uint8_t mask) {
        volatile uint8_t* r = _gpio_impl::ddr(portIdx);
        if (r) CLEARMASK(*r, mask);
    }

    static void portWrite(uint8_t portIdx, uint8_t value) {
        volatile uint8_t* r = _gpio_impl::port(portIdx);
        if (r) *r = value;
    }

    static uint8_t portRead(uint8_t portIdx) {
        volatile uint8_t* r = _gpio_impl::pin(portIdx);
        return r ? *r : 0;
    }
};

} // namespace MikroDuino
