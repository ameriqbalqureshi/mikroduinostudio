/*
 * ISR vector dispatch for InterruptManager.
 * fn0/fn1/fn2 are defined here (one definition) and declared extern in interrupt.hpp.
 * This file provides the actual INT0/INT1/INT2 ISR vector bodies,
 * which must live in exactly one compiled translation unit.
 * Include this file in sourceFiles only when using InterruptManager::attach().
 */

#include "../include/mikroduino/interrupt.hpp"
#include <avr/interrupt.h>

// Single definitions — extern declarations live in interrupt.hpp
namespace MikroDuino {
namespace _int_handlers {
    ISRHandler fn0 = nullptr;
    ISRHandler fn1 = nullptr;
#if MD_EXT_INT_COUNT >= 3
    ISRHandler fn2 = nullptr;
#endif
}
}

ISR(INT0_vect) {
    if (MikroDuino::_int_handlers::fn0)
        MikroDuino::_int_handlers::fn0();
}

ISR(INT1_vect) {
    if (MikroDuino::_int_handlers::fn1)
        MikroDuino::_int_handlers::fn1();
}

#if MD_EXT_INT_COUNT >= 3
ISR(INT2_vect) {
    if (MikroDuino::_int_handlers::fn2)
        MikroDuino::_int_handlers::fn2();
}
#endif
