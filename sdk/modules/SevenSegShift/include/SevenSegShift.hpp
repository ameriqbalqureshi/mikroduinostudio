#pragma once
/*
 * SevenSegShift<N> — N-digit multiplexed 7-segment display driven entirely
 * through two cascaded 74HC595 shift registers. Only 3 MCU pins needed
 * (DATA/SER, CLOCK/SRCLK, LATCH/RCLK) no matter how many digits.
 *
 * This is the shift-register counterpart of the SevenSeg module's
 * SevenSegMux<N> (sdk/modules/SevenSeg), which wires segment and digit
 * pins directly to the MCU. Use SevenSegMux when you have spare GPIO
 * pins to spare; use SevenSegShift when you only have 3 pins free and a
 * board with 2x 74HC595 (the common "4-digit" / "8-digit 7-segment
 * display module" eBay/AliExpress boards).
 *
 * Wiring
 * ──────
 * Two 74HC595s, cascaded (QH' of the first feeds SER of the second):
 *
 *   MCU dataPin  ──► SER  of chip A
 *   MCU clockPin ──► SRCLK of chip A and chip B (shared)
 *   MCU latchPin ──► RCLK  of chip A and chip B (shared)
 *   QH' of chip A ──► SER of chip B
 *
 *   One chip's 8 outputs (QA..QH) drive the 7 segments + DP (bit0=a..bit6=g,
 *   bit7=dp, same encoding as SevenSeg). The other chip's outputs drive one
 *   digit-select line per digit (via an NPN/PNP transistor per digit on most
 *   boards), bit i = digit i.
 *
 *   Which chip is "first" in the chain (A, closest to the MCU) varies by
 *   board. Tell the constructor which one it is via `order`:
 *     ChainOrder::SEGMENTS_FIRST (default) — chip A drives segments, chip B
 *       drives digit-select. Matches the common breakout wiring.
 *     ChainOrder::DIGITS_FIRST — chip A drives digit-select, chip B drives
 *       segments.
 *   If the display looks garbled or shows the wrong digit lit, swap this
 *   flag first before checking anything else in the wiring.
 *
 * Segment pin order (indices 0-6): a, b, c, d, e, f, g — bit7 = dp
 *
 *     a
 *    ---
 *  f|   |b
 *    -g-
 *  e|   |c
 *    ---
 *     d    · dp
 *
 * Polarity
 * ────────
 *   commonAnode:     segment byte polarity. false (default) = common
 *                     cathode (segment bit 1 = lit). true = common anode
 *                     (segment bit 1 = lit, but the byte sent is inverted
 *                     since the 74HC595 output must idle HIGH to keep an
 *                     anode segment off).
 *   digitActiveHigh: digit-select byte polarity. true (default) = the
 *                     selected digit's transistor is driven by a HIGH
 *                     74HC595 output (common-cathode boards with an NPN
 *                     driver per digit — the typical wiring). Set false
 *                     for boards whose digit driver is active-LOW
 *                     (common-anode boards with a PNP driver per digit).
 *
 * Timing model — same rule as SevenSegMux: refresh() shows one digit and
 * must be called every ~2 ms (Scheduler, main loop, or Timer2 auto mode)
 * so the full N-digit cycle repeats fast enough to look solid to the eye.
 *
 * Usage:
 *   #include <SevenSegShift.hpp>
 *   using namespace MikroDuino;
 *
 *   SevenSegShift<4> display(PD4, PD5, PD6);   // data, clock, latch
 *   display.begin();
 *   display.print(1234);
 *
 *   while (true) {
 *       display.refresh();
 *       _delay_ms(2);
 *   }
 *
 * Timer2 conflict note:
 *   beginTimer2() defines TIMER2_OVF_vect. Do NOT call it alongside
 *   SevenSeg's SevenSegMux::beginTimer2(), IRRemote::begin() (also owns
 *   TIMER2_OVF_vect), or DCMotor with OC2A/OC2B pins. Use manual/ISR
 *   refresh (call refresh() yourself, e.g. from a Scheduler task) if you
 *   need Timer2 for something else.
 */

#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <mikroduino/gpio.hpp>

namespace MikroDuino {

// ============================================================================
//  Shared lookup table (internal; defined once in SevenSegShift.cpp).
//  Kept in its own namespace (not SevenSeg's MikroDuino::detail) so this
//  module never collides at link time with SevenSeg if both are used in
//  the same project.
// ============================================================================
namespace shift595 {

extern const uint8_t SEG_TABLE[16];

// Map a printable character to a segment bitmask.
// Returns 0x00 (blank) for unsupported characters.
static inline uint8_t charToSeg(char c) {
    if (c >= '0' && c <= '9') return SEG_TABLE[static_cast<uint8_t>(c - '0')];
    if (c >= 'A' && c <= 'F') return SEG_TABLE[10u + static_cast<uint8_t>(c - 'A')];
    if (c >= 'a' && c <= 'f') return SEG_TABLE[10u + static_cast<uint8_t>(c - 'a')];
    switch (c) {
        case '-': return 0x40u;   // g only
        case '_': return 0x08u;   // d only
        case 'G': return 0x7Du;
        case 'H': return 0x76u;
        case 'h': return 0x74u;
        case 'I': return 0x06u;
        case 'J': return 0x0Eu;
        case 'L': return 0x38u;
        case 'n': return 0x54u;
        case 'o': return 0x5Cu;
        case 'P': return 0x73u;
        case 'q': return 0x67u;
        case 'r': return 0x50u;
        case 'S': return 0x6Du;
        case 't': return 0x78u;
        case 'U': return 0x3Eu;
        case 'u': return 0x1Cu;
        case 'Y': return 0x6Eu;
        default:  return 0x00u;
    }
}

} // namespace shift595

// ============================================================================
//  SevSegShiftBase — non-template base for Timer2 auto-refresh
// ============================================================================

class SevSegShiftBase {
public:
    // Configure Timer2 in normal (free-running) mode and enable its OVF
    // interrupt to call refresh() automatically.
    //
    // prescaler choices: 8, 32, 64 (default), 128, 256
    //   At 16 MHz, prescaler 64 → ~1 ms per OVF → each of N digits shown
    //   at ~1000/N Hz (250 Hz per digit at N=4).
    //
    // Must call sei() after this function.
    void beginTimer2(uint16_t prescaler = 64);

    virtual void _isrTick() = 0;   // called by TIMER2_OVF_vect

    static SevSegShiftBase* _autoInstance; // ISR dispatch target

protected:
    SevSegShiftBase()  = default;
    ~SevSegShiftBase() = default;
};

// ============================================================================
//  SevenSegShift<N> — N-digit multiplexed display over 2x 74HC595
// ============================================================================

template<uint8_t N>
class SevenSegShift : public SevSegShiftBase {
    static_assert(N >= 2u && N <= 8u, "SevenSegShift: N must be 2-8 (one bit per digit in an 8-bit shift register)");

public:
    enum class ChainOrder : uint8_t { SEGMENTS_FIRST, DIGITS_FIRST };

    // dataPin/clockPin/latchPin: SER, SRCLK, RCLK — shared across both 74HC595s.
    // commonAnode:      segment byte polarity (see header comment above).
    // digitActiveHigh:  digit-select byte polarity (see header comment above).
    // order:            which 74HC595 is first in the chain (see header comment above).
    SevenSegShift(uint8_t dataPin, uint8_t clockPin, uint8_t latchPin,
                  bool commonAnode      = false,
                  bool digitActiveHigh  = true,
                  ChainOrder order      = ChainOrder::SEGMENTS_FIRST)
        : _data(dataPin), _clock(clockPin), _latch(latchPin),
          _commonAnode(commonAnode), _digitActiveHigh(digitActiveHigh),
          _order(order), _cur(0u)
    {
        for (uint8_t i = 0u; i < N; ++i) _buf[i] = 0u;
    }

    // Configure the 3 control pins as outputs and blank the display.
    void begin() {
        GPIO::output(_data);
        GPIO::output(_clock);
        GPIO::output(_latch);
        GPIO::clear(_clock);
        GPIO::clear(_latch);
        clear();
        _shiftFrame(0u, 0u);
    }

    // ── Buffer writes ────────────────────────────────────────────────────────

    // Write a decimal (0-9) or hex (0-15) digit at position pos.
    // DP state at that position is preserved.
    void setDigit(uint8_t pos, uint8_t val) {
        if (pos < N && val < 16u)
            _buf[pos] = (_buf[pos] & 0x80u) | shift595::SEG_TABLE[val];
    }

    // Write a character at pos. DP state preserved.
    void setChar(uint8_t pos, char c) {
        if (pos < N)
            _buf[pos] = (_buf[pos] & 0x80u) | shift595::charToSeg(c);
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
        uint16_t u = static_cast<uint16_t>(neg ? -static_cast<int32_t>(n) : n);
        int8_t pos = static_cast<int8_t>(N) - 1;
        do {
            if (pos >= 0) _buf[static_cast<uint8_t>(pos--)] = shift595::SEG_TABLE[u % 10u];
            u /= 10u;
        } while (u > 0u);
        if (leadingZero) {
            int8_t stop = neg ? 1 : 0;
            while (pos >= stop) _buf[static_cast<uint8_t>(pos--)] = shift595::SEG_TABLE[0];
        }
        if (neg && pos >= 0) _buf[static_cast<uint8_t>(pos)] = 0x40u; // '-'
    }

    // Display an unsigned integer, right-justified.
    void print(uint16_t n, bool leadingZero = false) {
        clear();
        int8_t pos = static_cast<int8_t>(N) - 1;
        do {
            if (pos >= 0) _buf[static_cast<uint8_t>(pos--)] = shift595::SEG_TABLE[n % 10u];
            n /= 10u;
        } while (n > 0u);
        if (leadingZero)
            while (pos >= 0) _buf[static_cast<uint8_t>(pos--)] = shift595::SEG_TABLE[0];
    }

    // Display a 16-bit value as up to N hex digits, left-padded with '0'.
    void printHex(uint16_t n) {
        for (int8_t i = static_cast<int8_t>(N) - 1; i >= 0; --i) {
            _buf[static_cast<uint8_t>(i)] = shift595::SEG_TABLE[n & 0x0Fu];
            n >>= 4u;
        }
    }

#ifndef MD_NO_FLOAT
    // Display a floating-point value with 'decimals' fractional digits.
    // The decimal point is placed on the appropriate digit automatically.
    // Requires N >= 2. Uses software float — ~1 KB flash overhead.
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

    // Advance to the next digit: shifts out a fresh segment byte + digit-select
    // byte (16 bits total) and latches them. Call every ~2 ms from the main
    // loop or a Scheduler task. The Timer2 ISR calls this automatically when
    // beginTimer2() has been used.
    void refresh() {
        uint8_t segByte = _buf[_cur];
        if (_commonAnode) segByte = static_cast<uint8_t>(~segByte);

        uint8_t digitByte = static_cast<uint8_t>(1u << _cur);
        if (!_digitActiveHigh) digitByte = static_cast<uint8_t>(~digitByte);

        _shiftFrame(segByte, digitByte);

        if (++_cur >= N) _cur = 0u;
    }

    void _isrTick() override { refresh(); }

private:
    uint8_t _data, _clock, _latch;
    bool    _commonAnode;
    bool    _digitActiveHigh;
    ChainOrder _order;
    uint8_t _buf[N];   // segment data per digit, bit7 = DP
    uint8_t _cur;      // digit currently being strobed

    // Shift one byte out MSB-first: bit7 sent first ends up at QH after
    // 8 clocks, bit0 sent last ends up at QA — natural bit-to-pin mapping.
    void _shiftByte(uint8_t v) {
        for (int8_t b = 7; b >= 0; --b) {
            GPIO::write(_data, static_cast<bool>((v >> b) & 1u));
            GPIO::set(_clock);
            GPIO::clear(_clock);
        }
    }

    // Shift both bytes (16 bits total across the two cascaded chips) then
    // pulse latch once so the new frame appears atomically on the outputs.
    void _shiftFrame(uint8_t segByte, uint8_t digitByte) {
        GPIO::clear(_latch);
        if (_order == ChainOrder::SEGMENTS_FIRST) {
            _shiftByte(segByte);
            _shiftByte(digitByte);
        } else {
            _shiftByte(digitByte);
            _shiftByte(segByte);
        }
        GPIO::set(_latch);
    }
};

} // namespace MikroDuino
