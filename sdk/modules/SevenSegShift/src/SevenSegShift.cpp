#include "SevenSegShift.hpp"
#include <avr/io.h>
#include <avr/interrupt.h>

namespace MikroDuino {

namespace shift595 {

const uint8_t SEG_TABLE[16] = {
    0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07,  // 0-7
    0x7F, 0x6F, 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71    // 8-F
};

} // namespace shift595

// ============================================================================
//  SevSegShiftBase — Timer2 auto-refresh
// ============================================================================

SevSegShiftBase* SevSegShiftBase::_autoInstance = nullptr;

void SevSegShiftBase::beginTimer2(uint16_t prescaler) {
    _autoInstance = this;

    TCCR2A = 0u; // Normal mode (OVF at 0xFF)

    uint8_t cs;
    switch (prescaler) {
        case 8:   cs = (1u << CS21);                        break;
        case 32:  cs = (1u << CS21) | (1u << CS20);         break;
        default:  // fall through to 64
        case 64:  cs = (1u << CS22);                        break;
        case 128: cs = (1u << CS22) | (1u << CS20);         break;
        case 256: cs = (1u << CS22) | (1u << CS21);         break;
    }
    TCCR2B  = cs;
    TIMSK2  = (1u << TOIE2);
    // Call sei() in user code after begin() / beginTimer2()
}

} // namespace MikroDuino

// ============================================================================
//  Timer2 OVF ISR — dispatches to the registered SevenSegShift instance
// ============================================================================
//
// Conflict: SevenSeg's SevenSegMux::beginTimer2() and IRRemote::begin()
//   also define this ISR. Use only one Timer2 AUTO owner per project;
//   drive the others manually (refresh() from a Scheduler task) instead.

ISR(TIMER2_OVF_vect) {
    if (MikroDuino::SevSegShiftBase::_autoInstance)
        MikroDuino::SevSegShiftBase::_autoInstance->_isrTick();
}
