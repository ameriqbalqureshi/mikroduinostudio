#pragma once
/*
 * SevenSeg / SevenSegMux — 7-segment LED display drivers.
 *
 * Two classes in one header, usable independently:
 *
 * ┌──────────────────────────────────────────────────────────────────────────┐
 * │ SevenSeg — single digit                                                 │
 * │  Drive 7 segment pins + optional DP.  Common cathode or common anode.  │
 * │  SevenSeg display(segs, PD7, false);                                   │
 * │  display.begin();                                                       │
 * │  display.show(5);   display.setDP(true);                               │
 * └──────────────────────────────────────────────────────────────────────────┘
 *
 * ┌──────────────────────────────────────────────────────────────────────────┐
 * │ SevenSegMux<N> — N-digit multiplexed display (N = 2..8)                │
 * │                                                                         │
 * │  Shared segment bus (7 pins a-g), one digit-select pin per digit.      │
 * │  Multiplexing modes:                                                    │
 * │                                                                         │
 * │  MANUAL   Call display.refresh() every ~2 ms (Scheduler or main loop). │
 * │  ISR      Route any timer ISR to display._isrTick().                   │
 * │  AUTO     Call display.beginTimer2() to let the library configure       │
 * │           Timer2 OVF and call refresh() automatically.                 │
 * │                                                                         │
 * │  uint8_t segs[7]    = {PB0,PB1,PB2,PB3,PB4,PB5,PB6}; // a…g         │
 * │  uint8_t digits[4]  = {PC0,PC1,PC2,PC3};                              │
 * │  SevenSegMux<4> display(segs, digits);                                 │
 * │  display.begin();  display.beginTimer2();  sei();                      │
 * │  display.print(-42);                                                   │
 * └──────────────────────────────────────────────────────────────────────────┘
 *
 * Segment pin order (indices 0–6): a, b, c, d, e, f, g
 *
 *     a
 *    ---
 *  f|   |b
 *    -g-
 *  e|   |c
 *    ---
 *     d    · dp (optional, separate pin or bit7 of raw byte)
 *
 * Segment bitmask: bit0=a, bit1=b, bit2=c, bit3=d, bit4=e, bit5=f,
 *                  bit6=g, bit7=dp
 *
 * Common cathode:  segment HIGH = on;  digit pin LOW  = selected.
 * Common anode:    segment LOW  = on;  digit pin HIGH = selected.
 *
 * Timer2 conflict note:
 *   beginTimer2() defines TIMER2_OVF_vect.  Do NOT call it alongside
 *   IRRemote::begin() (which also owns TIMER2_OVF_vect) or DCMotor
 *   with OC2A/OC2B pins.  If you need IRRemote, use IRReceiver (GPIO
 *   mode) instead, which has no timer requirement.  Manual/ISR refresh
 *   modes have no timer conflict.
 */

#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <mikroduino/gpio.hpp>

namespace MikroDuino {

static constexpr uint8_t SEG_NO_PIN = 0xFFu;

// ============================================================================
//  Shared lookup tables (internal; defined once in SevenSeg.cpp)
// ============================================================================
namespace detail {

// Standard 7-segment encoding for 0-9 and A-F (bit0=a … bit6=g).
// Declared extern rather than a C++17 "inline constexpr" namespace-scope
// variable: the toolchain this SDK ships (avr-gcc 5.4.0) doesn't support
// inline variables, so a single definition lives in SevenSeg.cpp instead.
extern const uint8_t SEG_TABLE[16];

// Map a printable character to a segment bitmask.
// Returns 0x00 (blank) for unsupported characters.
// Use showRaw() to display any custom pattern.
static inline uint8_t charToSeg(char c) {
    if (c >= '0' && c <= '9') return SEG_TABLE[static_cast<uint8_t>(c - '0')];
    if (c >= 'A' && c <= 'F') return SEG_TABLE[10u + static_cast<uint8_t>(c - 'A')];
    if (c >= 'a' && c <= 'f') return SEG_TABLE[10u + static_cast<uint8_t>(c - 'a')];
    switch (c) {
        case '-': return 0x40u;   // g only
        case '_': return 0x08u;   // d only
        case 'G': return 0x7Du;   // a,c,d,e,f,g
        case 'H': return 0x76u;   // b,c,e,f,g
        case 'h': return 0x74u;   // c,e,f,g
        case 'I': return 0x06u;   // b,c (= '1')
        case 'J': return 0x0Eu;   // b,c,d
        case 'L': return 0x38u;   // d,e,f
        case 'n': return 0x54u;   // c,e,g
        case 'o': return 0x5Cu;   // c,d,e,g
        case 'P': return 0x73u;   // a,b,e,f,g
        case 'q': return 0x67u;   // a,b,c,f,g
        case 'r': return 0x50u;   // e,g
        case 'S': return 0x6Du;   // a,c,d,f,g (= '5')
        case 't': return 0x78u;   // d,e,f,g
        case 'U': return 0x3Eu;   // b,c,d,e,f
        case 'u': return 0x1Cu;   // c,d,e
        case 'Y': return 0x6Eu;   // b,c,d,f,g
        default:  return 0x00u;   // blank / unknown
    }
}

} // namespace detail

// ============================================================================
//  SevenSeg — single digit
// ============================================================================

class SevenSeg {
public:
    // segPins[7]: GPIO pin for each segment, in order a,b,c,d,e,f,g.
    // dpPin: pin for decimal point (SEG_NO_PIN to omit).
    // commonAnode: true for common-anode displays (segment pins active LOW).
    SevenSeg(const uint8_t segPins[7],
             uint8_t dpPin       = SEG_NO_PIN,
             bool    commonAnode = false);

    void begin();

    // Display a single digit 0–9 or hex digit 0–15 (A–F).
    void show(uint8_t digit);

    // Display a character from the supported set ('0'-'9', 'A'-'F',
    // 'a'-'f', '-', '_', 'H', 'h', 'L', 'n', 'o', 'P', 'r', 't',
    // 'U', 'u', 'Y' …).  Unknown characters display as blank.
    void showChar(char c);

    // Write a raw segment bitmask (bit0=a … bit6=g, bit7=dp).
    void showRaw(uint8_t seg);

    // Turn the decimal point on or off without changing the digit.
    void setDP(bool on);

    void clear();

private:
    uint8_t _pins[7];
    uint8_t _dpPin;
    bool    _commonAnode;
    uint8_t _current;   // last written byte (bit7 = DP state)

    void _write(uint8_t seg);
};

// ============================================================================
//  SevenSegMuxBase — non-template base for Timer2 auto-refresh
// ============================================================================

class SevenSegMuxBase {
public:
    // Configure Timer2 in normal (free-running) mode and enable its OVF
    // interrupt to call refresh() automatically.
    //
    // prescaler choices: 8, 32, 64 (default), 128, 256
    //   At 16 MHz, prescaler 64 → ~1 ms per OVF → each of N digits shown
    //   at ~1000/N Hz (250 Hz per digit at N=4).
    //
    // Must call sei() after this function.
    // Conflict: shares TIMER2_OVF_vect with IRRemote::begin().
    //   If using IRRemote, call manual refresh() from Scheduler instead.
    void beginTimer2(uint16_t prescaler = 64);

    virtual void _isrTick() = 0;   // called by TIMER2_OVF_vect

    static SevenSegMuxBase* _autoInstance; // ISR dispatch target

protected:
    SevenSegMuxBase()  = default;
    ~SevenSegMuxBase() = default;
};

// ============================================================================
//  SevenSegMux<N> — N-digit multiplexed display (N = 2..8)
// ============================================================================

template<uint8_t N>
class SevenSegMux : public SevenSegMuxBase {
    static_assert(N >= 2u && N <= 8u, "SevenSegMux: N must be 2–8");

public:
    // segPins[7]:   a,b,c,d,e,f,g — shared bus across all digits.
    // digitPins[N]: one common pin per digit (cathode for CC, anode for CA).
    // commonAnode:  true for common-anode displays.
    // dpPin:        shared decimal-point pin (SEG_NO_PIN to omit).
    SevenSegMux(const uint8_t segPins[7],
                const uint8_t digitPins[N],
                bool    commonAnode = false,
                uint8_t dpPin       = SEG_NO_PIN)
        : _dpPin(dpPin), _commonAnode(commonAnode), _cur(0u)
    {
        for (uint8_t i = 0u; i < 7u; ++i) _segPins[i]   = segPins[i];
        for (uint8_t i = 0u; i < N;  ++i) _digitPins[i] = digitPins[i];
        for (uint8_t i = 0u; i < N;  ++i) _buf[i] = 0u;
    }

    // Configure all pins as outputs and blank the display.
    void begin() {
        for (uint8_t i = 0u; i < 7u; ++i) GPIO::output(_segPins[i]);
        if (_dpPin != SEG_NO_PIN) GPIO::output(_dpPin);
        for (uint8_t i = 0u; i < N; ++i) {
            GPIO::output(_digitPins[i]);
            _deselect(i);
        }
        clear();
        _writeSeg(0u);
    }

    // ── Buffer writes ────────────────────────────────────────────────────────

    // Write a decimal (0–9) or hex (0–15) digit at position pos.
    // DP state at that position is preserved.
    void setDigit(uint8_t pos, uint8_t val) {
        if (pos < N && val < 16u)
            _buf[pos] = (_buf[pos] & 0x80u) | detail::SEG_TABLE[val];
    }

    // Write a character at pos. DP state preserved.
    void setChar(uint8_t pos, char c) {
        if (pos < N)
            _buf[pos] = (_buf[pos] & 0x80u) | detail::charToSeg(c);
    }

    // Write a raw segment bitmask (bit0=a … bit6=g, bit7=dp).
    void setRaw(uint8_t pos, uint8_t seg) {
        if (pos < N) _buf[pos] = seg;
    }

    // Turn decimal point on or off at pos without changing the digit.
    void setDP(uint8_t pos, bool on) {
        if (pos >= N) return;
        if (on) _buf[pos] |=  0x80u;
        else    _buf[pos] &= ~0x80u;
    }

    void clear() { for (uint8_t i = 0u; i < N; ++i) _buf[i] = 0u; }

    // ── Number formatting ────────────────────────────────────────────────────

    // Display a signed integer, right-justified.
    // leadingZero: fill unused positions with '0' instead of blank.
    void print(int16_t n, bool leadingZero = false) {
        clear();
        bool neg = (n < 0);
        // Widen before negating to handle INT16_MIN correctly.
        uint16_t u = static_cast<uint16_t>(neg ? -static_cast<int32_t>(n) : n);
        int8_t pos = static_cast<int8_t>(N) - 1;
        do {
            if (pos >= 0) _buf[static_cast<uint8_t>(pos--)] = detail::SEG_TABLE[u % 10u];
            u /= 10u;
        } while (u > 0u);
        if (leadingZero) {
            int8_t stop = neg ? 1 : 0;
            while (pos >= stop) _buf[static_cast<uint8_t>(pos--)] = detail::SEG_TABLE[0];
        }
        if (neg && pos >= 0) _buf[static_cast<uint8_t>(pos)] = 0x40u; // '-'
    }

    // Display an unsigned integer, right-justified.
    void print(uint16_t n, bool leadingZero = false) {
        clear();
        int8_t pos = static_cast<int8_t>(N) - 1;
        do {
            if (pos >= 0) _buf[static_cast<uint8_t>(pos--)] = detail::SEG_TABLE[n % 10u];
            n /= 10u;
        } while (n > 0u);
        if (leadingZero)
            while (pos >= 0) _buf[static_cast<uint8_t>(pos--)] = detail::SEG_TABLE[0];
    }

    // Display a 16-bit value as up to N hex digits, left-padded with '0'.
    void printHex(uint16_t n) {
        for (int8_t i = static_cast<int8_t>(N) - 1; i >= 0; --i) {
            _buf[static_cast<uint8_t>(i)] = detail::SEG_TABLE[n & 0x0Fu];
            n >>= 4u;
        }
    }

#ifndef MD_NO_FLOAT
    // Display a floating-point value with 'decimals' fractional digits.
    // The decimal point is placed on the appropriate digit automatically.
    // Requires N >= 2.  Uses software float — ~1 KB flash overhead.
    void printFloat(float val, uint8_t decimals = 1u) {
        float scale = 1.0f;
        for (uint8_t i = 0u; i < decimals; ++i) scale *= 10.0f;
        int16_t scaled = static_cast<int16_t>(val * scale + (val >= 0.0f ? 0.5f : -0.5f));
        print(scaled, false);
        if (decimals > 0u && decimals < N)
            setDP(static_cast<uint8_t>(N - 1u - decimals), true);
    }
#endif

    // ── Refresh ──────────────────────────────────────────────────────────────

    // Advance to the next digit.  Call every ~2 ms from the main loop
    // or a Scheduler task.  The Timer2 ISR calls this automatically
    // when beginTimer2() has been used.
    void refresh() {
        // Deselect all digits first (ghost prevention).
        for (uint8_t i = 0u; i < N; ++i) _deselect(i);

        // Drive segment lines for the current digit.
        _writeSeg(_buf[_cur]);

        // Enable the current digit.
        _select(_cur);

        if (++_cur >= N) _cur = 0u;
    }

    void _isrTick() override { refresh(); }

private:
    uint8_t _segPins[7];
    uint8_t _digitPins[N];
    uint8_t _dpPin;
    bool    _commonAnode;
    uint8_t _buf[N];   // segment data per digit, bit7 = DP
    uint8_t _cur;      // digit currently being strobed

    // Drive the shared segment bus and DP pin for segment bitmask seg.
    void _writeSeg(uint8_t seg) {
        for (uint8_t s = 0u; s < 7u; ++s) {
            bool on = static_cast<bool>((seg >> s) & 1u);
            GPIO::write(_segPins[s], _commonAnode ? !on : on);
        }
        if (_dpPin != SEG_NO_PIN) {
            bool dpOn = static_cast<bool>((seg >> 7u) & 1u);
            GPIO::write(_dpPin, _commonAnode ? !dpOn : dpOn);
        }
    }

    void _select(uint8_t d) {
        // CC: digit common (cathode) → LOW to source current through segments.
        // CA: digit common (anode)  → HIGH.
        GPIO::write(_digitPins[d], _commonAnode);
    }

    void _deselect(uint8_t d) {
        GPIO::write(_digitPins[d], !_commonAnode);
    }
};

} // namespace MikroDuino
