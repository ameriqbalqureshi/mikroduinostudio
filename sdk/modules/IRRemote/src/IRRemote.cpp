#include "IRRemote.hpp"
#include <avr/io.h>
#include <avr/interrupt.h>

// --------------------------------------------------------------------------
// Compile-time pin selection
//
// The raw bit-position literals below (2, 3) are used deliberately instead
// of the PD2/PD3 symbols: IRRemote.hpp pulls in mikroduino/gpio.hpp, which
// #undefs avr-libc's raw PD2/PD3 (bit position 2 / 3) and redefines them as
// MikroDuino's combined port+bit pin encoding (see gpio.hpp) — a different
// numeric value. Direct PIND/PORTD/DDRD register math needs the true
// hardware bit position, which for INT0/INT1 is always 2/3 regardless of
// that encoding, so it's spelled out as a literal rather than relying on
// whichever meaning PD2/PD3 currently has in the including translation unit.
// --------------------------------------------------------------------------
#ifdef MD_IR_USE_INT1
#  define _IR_PIN_LOW()  (!(PIND & (1u << 3)))
#  define _IR_INT_SETUP() do {                                  \
      DDRD  &= ~(1u << DDD3);   /* PD3 input */                \
      PORTD |=  (1u << 3);      /* pull-up */                  \
      EICRA  = (EICRA & ~((1u<<ISC11)|(1u<<ISC10)))            \
             | (1u<<ISC10);     /* any change */                \
      EIMSK |= (1u<<INT1);                                      \
  } while(0)
#  define ISR_VECTOR INT1_vect
#else
#  define _IR_PIN_LOW()  (!(PIND & (1u << 2)))
#  define _IR_INT_SETUP() do {                                  \
      DDRD  &= ~(1u << DDD2);   /* PD2 input */                \
      PORTD |=  (1u << 2);      /* pull-up */                  \
      EICRA  = (EICRA & ~((1u<<ISC01)|(1u<<ISC00)))            \
             | (1u<<ISC00);     /* any change */                \
      EIMSK |= (1u<<INT0);                                      \
  } while(0)
#  define ISR_VECTOR INT0_vect
#endif

// --------------------------------------------------------------------------
// Static member definitions
// --------------------------------------------------------------------------
namespace MikroDuino {

volatile uint8_t  IRRemote::_ovf2    = 0;
volatile uint16_t IRRemote::_lastTime = 0;
volatile uint16_t IRRemote::_pulse[IRRemote::PULSE_COUNT];
volatile uint8_t  IRRemote::_idx     = 0;
volatile uint8_t  IRRemote::_state   = 0;
volatile bool     IRRemote::_ready   = false;
volatile bool     IRRemote::_repeat  = false;

} // namespace MikroDuino

// --------------------------------------------------------------------------
// Timer2 overflow ISR — extends TCNT2 to a 16-bit soft counter
// --------------------------------------------------------------------------
ISR(TIMER2_OVF_vect) {
    MikroDuino::IRRemote::_onTimerOverflow();
}

// --------------------------------------------------------------------------
// External interrupt ISR — fires on every edge of the IR pin
// --------------------------------------------------------------------------
ISR(ISR_VECTOR) {
    MikroDuino::IRRemote::_isrHandler();
}

// --------------------------------------------------------------------------
// Implementation
// --------------------------------------------------------------------------
namespace MikroDuino {

void IRRemote::_onTimerOverflow() {
    ++_ovf2;
}

uint16_t IRRemote::_getTime() {
    // Read TCNT2 + overflow counter atomically.
    // If an overflow occurred between the two reads, re-read TCNT2 with
    // the incremented overflow count.
    uint8_t t = TCNT2;
    uint8_t o = _ovf2;
    if (TIFR2 & (1u << TOV2)) { t = TCNT2; o++; }
    return static_cast<uint16_t>(static_cast<uint16_t>(o) << 8) | t;
}

void IRRemote::begin() {
    // Timer2: normal mode, prescaler 8 → 0.5 µs/tick at 16 MHz
    TCCR2A = 0;
    TCCR2B = (1u << CS21);      // prescaler 8
    TIMSK2 = (1u << TOIE2);     // overflow interrupt

    _ovf2     = 0;
    _lastTime = 0;
    _idx      = 0;
    _state    = 0;
    _ready    = false;
    _repeat   = false;

    _IR_INT_SETUP();
}

void IRRemote::_isrHandler() {
    uint16_t now = _getTime();
    uint16_t dt  = now - _lastTime;
    _lastTime    = now;

    bool pinLow = _IR_PIN_LOW(); // true = pin is LOW after this edge (falling edge)

    if (_state == 0) {
        // IDLE: wait for the first falling edge (start of leader mark)
        if (pinLow) { _idx = 0; _state = 1; }
        return;
    }

    // COLLECTING
    // Timeout guard: >12.5 ms without a valid edge → frame broken
    if (dt > TIMEOUT_TICKS) {
        if (pinLow) { _idx = 0; return; }   // restart from this new falling edge
        _state = 0; return;
    }

    if (_idx < PULSE_COUNT) _pulse[_idx++] = dt;

    // After pulse[0] (9 ms LOW mark) + pulse[1] (space), check for repeat or bad header
    if (_idx == 2) {
        if (_pulse[0] >= HDR_MARK_MIN && _pulse[0] <= HDR_MARK_MAX) {
            if (_pulse[1] >= RPT_SPACE_MIN && _pulse[1] <= RPT_SPACE_MAX) {
                // 2.25 ms space → repeat code
                _repeat = true;
                _ready  = true;
                _state  = 0;
                return;
            }
            // Otherwise: should be a 4.5 ms data-frame space — continue
        } else {
            _state = 0; return;  // invalid leader mark
        }
    }

    if (_idx >= PULSE_COUNT) {
        _ready = true;
        _state = 0;
    }
}

uint32_t IRRemote::_decode() {
    // Validate leader
    if (_pulse[0] < HDR_MARK_MIN  || _pulse[0] > HDR_MARK_MAX)  return 0;
    if (_pulse[1] < HDR_SPACE_MIN || _pulse[1] > HDR_SPACE_MAX) return 0;

    // Decode 32 bits (LSB first)
    // _pulse[2+i*2] = bit-i LOW mark  (~562 µs, don't decode — just a sync pulse)
    // _pulse[3+i*2] = bit-i HIGH space (562 µs → 0, 1687 µs → 1)
    uint32_t code = 0;
    for (uint8_t i = 0; i < 32; i++) {
        if (_pulse[3u + static_cast<uint8_t>(i * 2u)] > BIT_THRESHOLD) {
            code |= (1UL << i);
        }
    }
    return code;
}

IRRemote::NECCode IRRemote::read() {
    NECCode c;
    c.repeat  = _repeat;
    c.valid   = false;
    c.address = 0;
    c.command = 0;

    if (_repeat) {
        c.valid = true;
        _ready  = false;
        _repeat = false;
        return c;
    }

    uint32_t raw = _decode();
    _ready  = false;

    c.address = rawAddress(raw);
    c.command = rawCommand(raw);

    // NEC checksum: address XOR ~address == 0xFF, command XOR ~command == 0xFF
    uint8_t addrInv = static_cast<uint8_t>((raw >> 8) & 0xFF);
    uint8_t cmdInv  = static_cast<uint8_t>((raw >> 24) & 0xFF);
    c.valid = ((c.address ^ addrInv) == 0xFFu) && ((c.command ^ cmdInv) == 0xFFu);

    return c;
}

} // namespace MikroDuino
