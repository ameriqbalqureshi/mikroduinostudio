#pragma once
/*
 * IRRemote / IRReceiver — NEC protocol IR receiver library.
 *
 * Two independent operation modes in this one header:
 *
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │ IRRemote  (static class) — interrupt-driven, non-blocking              │
 * │                                                                         │
 * │  Pin:   PD2 (INT0, default)  or  PD3 (INT1, #define MD_IR_USE_INT1)   │
 * │  Timer: Timer2 (prescaler 8, 0.5 µs/tick) — conflicts with DCMotor    │
 * │         OC2A/OC2B only; OC0A/OC0B are unaffected.                     │
 * │                                                                         │
 * │  IRRemote::begin();    sei();                                           │
 * │  while(true) {                                                          │
 * │      if (IRRemote::available()) {                                       │
 * │          auto c = IRRemote::read();                                    │
 * │          if (c.valid) handle(c.address, c.command);                    │
 * │      }                                                                  │
 * │  }                                                                      │
 * └─────────────────────────────────────────────────────────────────────────┘
 *
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │ IRReceiver (instance class) — software bit-bang, any GPIO pin          │
 * │                                                                         │
 * │  No interrupt needed. No timer. Measures pulse widths by counting      │
 * │  10 µs busy-wait ticks — works at any F_CPU.                          │
 * │                                                                         │
 * │  Provides two call styles:                                              │
 * │                                                                         │
 * │  poll()  — non-blocking. Returns immediately if pin is idle (HIGH).    │
 * │            Decodes if a frame is already starting (pin LOW).           │
 * │            Call at ≤ 1 ms intervals for reliable detection.            │
 * │                                                                         │
 * │  receive(timeoutMs) — blocking. Waits for the next COMPLETE frame      │
 * │            from its start, so call timing does not matter.             │
 * │            Pass timeoutMs=0 for equivalent of poll().                  │
 * │                                                                         │
 * │  IRReceiver ir(PC5);                                                   │
 * │  ir.begin();                                                            │
 * │                                                                         │
 * │  // Style A: polling (like a keypad or button scan)                    │
 * │  while (true) {                                                         │
 * │      auto c = ir.poll();                                                │
 * │      if (c.valid) handle(c.address, c.command);                        │
 * │      _delay_ms(1);                                                      │
 * │  }                                                                      │
 * │                                                                         │
 * │  // Style B: blocking (like waitKey on a keypad)                       │
 * │  auto c = ir.receive(5000);   // wait up to 5 s for any button        │
 * └─────────────────────────────────────────────────────────────────────────┘
 *
 * Both classes decode NEC standard and NEC extended (32-bit) frames.
 * NEC code layout (uint32_t returned in NECCode):
 *   bits  7:0   address  (LSB first)      bits 15:8  ~address
 *   bits 23:16  command  (LSB first)      bits 31:24 ~command
 */

#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <mikroduino/gpio.hpp>

namespace MikroDuino {

// ============================================================================
//  Shared result type
// ============================================================================

struct NECCode {
    uint8_t address;   // 8-bit device address
    uint8_t command;   // 8-bit button code
    bool    valid;     // true = address/command byte-inversions pass
    bool    repeat;    // true = auto-repeat (remote button held)
};

// ============================================================================
//  IRRemote — interrupt-driven, non-blocking (INT0 / INT1)
// ============================================================================

// Timer2 tick = prescaler8 / F_CPU  (0.5 µs at 16 MHz). Kept as a free
// function rather than an IRRemote static member: a static constexpr member
// function cannot be called from another static constexpr member's
// initializer within the same still-incomplete class body (rejected as
// "called in a constant expression" by GCC 5.4's C++17 front end), so the
// conversion helper has to live outside the class it's used to initialize.
namespace _ir_remote_impl {
    constexpr uint16_t usToTicks(uint32_t us) {
        return static_cast<uint16_t>(us * (F_CPU / 1000000UL) / 8UL);
    }
}

class IRRemote {
public:
    // Preserve backward compat — NECCode is also a member type alias.
    using NECCode = MikroDuino::NECCode;

    // Initialise Timer2 and external interrupt. Call sei() afterwards.
    static void begin();

    // true when a complete frame (data or repeat) has been received.
    static bool available() { return _ready; }

    // Return decoded NEC frame and clear the ready flag.
    static NECCode read();

    static uint8_t rawAddress(uint32_t raw) { return static_cast<uint8_t>(raw); }
    static uint8_t rawCommand (uint32_t raw) { return static_cast<uint8_t>(raw >> 16); }

    // Internal — called by ISR.
    static void _isrHandler();
    static void _onTimerOverflow();

private:
    static constexpr uint8_t PULSE_COUNT = 67;

    static constexpr uint16_t HDR_MARK_MIN  = _ir_remote_impl::usToTicks(6750);
    static constexpr uint16_t HDR_MARK_MAX  = _ir_remote_impl::usToTicks(11250);
    static constexpr uint16_t HDR_SPACE_MIN = _ir_remote_impl::usToTicks(3375);
    static constexpr uint16_t HDR_SPACE_MAX = _ir_remote_impl::usToTicks(5625);
    static constexpr uint16_t RPT_SPACE_MIN = _ir_remote_impl::usToTicks(1688);
    static constexpr uint16_t RPT_SPACE_MAX = _ir_remote_impl::usToTicks(2813);
    static constexpr uint16_t BIT_THRESHOLD = _ir_remote_impl::usToTicks(1125);
    static constexpr uint16_t TIMEOUT_TICKS = _ir_remote_impl::usToTicks(12500);

    static volatile uint8_t  _ovf2;
    static volatile uint16_t _lastTime;
    static volatile uint16_t _pulse[PULSE_COUNT];
    static volatile uint8_t  _idx;
    static volatile uint8_t  _state;
    static volatile bool     _ready;
    static volatile bool     _repeat;

    static uint16_t _getTime();
    static uint32_t _decode();
};

// ============================================================================
//  IRReceiver — software bit-bang, any GPIO pin
// ============================================================================

class IRReceiver {
public:
    // gpioPin: any MikroDuino GPIO pin encoding (PBx, PCx, PDx, …)
    explicit IRReceiver(uint8_t gpioPin) : _pin(gpioPin) {}

    // Set pin as INPUT_PULLUP.  No timer or interrupt configured.
    void begin() { GPIO::inputPullup(_pin); }

    // ── Non-blocking poll ────────────────────────────────────────────────────
    // Returns immediately if pin is HIGH (idle).
    // If pin is already LOW (frame in progress), decodes to completion (~70 ms).
    // Call at ≤ 1 ms intervals for reliable frame capture.
    NECCode poll() const {
        if (GPIO::read(_pin)) return _empty(); // idle
        return _decodeFrom(_pin);              // frame starting now
    }

    // ── Blocking receive ─────────────────────────────────────────────────────
    // Syncs to idle, then waits up to timeoutMs ms for the next frame START.
    // Because it waits for the true beginning of the header mark, decoding is
    // reliable regardless of how often you call receive().
    //
    //   ir.receive(0)   → equivalent to poll()
    //   ir.receive(150) → block up to 150 ms (common for one remote keypress)
    //   ir.receive(60000) → up to ~60 s (practical "wait forever")
    NECCode receive(uint16_t timeoutMs = 150) const {
        if (timeoutMs == 0) return poll();

        // Sync to idle: if the pin is already in a frame, wait for it to finish.
        // Max wait: 20 ms covers worst-case NEC frame tail.
        {
            uint16_t s = 0;
            while (!GPIO::read(_pin) && s < 2000u) { _delay_us(10.0); s++; }
            if (s >= 2000u) return _empty(); // pin stuck LOW (wiring issue?)
        }

        // Wait for the next frame to begin (pin goes LOW).
        // timeoutMs * 100 = ticks of 10 µs (cap at 60 000 to avoid overflow).
        uint16_t to = (timeoutMs > 600u) ? 60000u
                                          : static_cast<uint16_t>(timeoutMs * 100u);
        {
            uint16_t w = 0;
            while (GPIO::read(_pin)) {
                if (w++ >= to) return _empty(); // timeout
                _delay_us(10.0);
            }
        }

        return _decodeFrom(_pin);
    }

private:
    uint8_t _pin;

    // ── Thresholds in 10 µs counts ──────────────────────────────────────────
    // _delay_us(10.0) is a compile-time-constant arg → precisely 10 µs at
    // any F_CPU.  GPIO::read() adds ≈ 0.5–1 µs overhead per count, but the
    // ±33 % acceptance windows below absorb it at 8 and 16 MHz.
    static constexpr uint16_t HDR_MARK_MIN  = 600u;  // 6 ms
    static constexpr uint16_t HDR_MARK_MAX  = 1200u; // 12 ms
    static constexpr uint16_t HDR_SPACE_MIN = 300u;  // 3 ms  (data or repeat)
    static constexpr uint16_t HDR_SPACE_MAX = 620u;  // 6.2 ms
    static constexpr uint16_t RPT_SPACE_MIN = 150u;  // 1.5 ms → repeat
    static constexpr uint16_t RPT_SPACE_MAX = 300u;  // 3 ms
    static constexpr uint16_t BIT_THRESHOLD = 100u;  // ~1 ms  → bit=1 if >100
    static constexpr uint16_t BIT_MARK_MAX  = 150u;  // 1.5 ms per bit mark
    static constexpr uint16_t BIT_SPACE_MAX = 250u;  // 2.5 ms per bit space

    static NECCode _empty() {
        NECCode c; c.address=0; c.command=0; c.valid=false; c.repeat=false;
        return c;
    }

    // ── Core pulse-width helper ──────────────────────────────────────────────
    // Phase 1: wait for pin to reach waitState (up to waitMax × 10 µs).
    // Phase 2: count consecutive ticks pin stays in waitState (up to measMax).
    // Returns tick count, or 0xFFFF on any timeout / overflow.
    static uint16_t _measure(uint8_t pin, bool waitState,
                              uint16_t waitMax, uint16_t measMax) {
        uint16_t w = 0;
        while (GPIO::read(pin) != waitState) {
            if (w++ >= waitMax) return 0xFFFFu;
            _delay_us(10.0);
        }
        uint16_t n = 0;
        while (GPIO::read(pin) == waitState) {
            _delay_us(10.0);
            if (++n >= measMax) return 0xFFFFu;
        }
        return n;
    }

    // ── NEC frame decode — enter with pin already LOW (header mark started) ──
    static NECCode _decodeFrom(uint8_t pin) {
        // Header mark (LOW). waitMax=0: pin is already LOW on entry.
        uint16_t hdrMark = _measure(pin, false, 0u, HDR_MARK_MAX + 50u);
        if (hdrMark < HDR_MARK_MIN || hdrMark > HDR_MARK_MAX) return _empty();

        // Header space (HIGH) — may be data (4.5 ms) or repeat (2.25 ms).
        uint16_t hdrSpace = _measure(pin, true, 200u, HDR_SPACE_MAX + 50u);
        if (hdrSpace == 0xFFFFu || hdrSpace < RPT_SPACE_MIN) return _empty();
        if (hdrSpace <= RPT_SPACE_MAX) {
            // Repeat code: 9 ms mark + 2.25 ms space + 562 µs mark
            NECCode r; r.address=0; r.command=0; r.repeat=true; r.valid=true;
            _measure(pin, false, 200u, BIT_MARK_MAX); // consume 562 µs stop mark
            return r;
        }
        if (hdrSpace > HDR_SPACE_MAX) return _empty();

        // 32 data bits, LSB first
        uint32_t code = 0;
        for (uint8_t i = 0; i < 32; i++) {
            // Bit mark (~562 µs LOW): consume, don't classify
            if (_measure(pin, false, 200u, BIT_MARK_MAX) == 0xFFFFu) return _empty();
            // Bit space (HIGH): <100 counts → 0, ≥100 → 1
            uint16_t sp = _measure(pin, true, 200u, BIT_SPACE_MAX);
            if (sp == 0xFFFFu) return _empty();
            if (sp >= BIT_THRESHOLD) code |= (1UL << i);
        }

        // Stop mark (~562 µs LOW): consume cleanly so caller can call again
        _measure(pin, false, 200u, BIT_MARK_MAX);

        NECCode r;
        r.address = IRRemote::rawAddress(code);
        r.command  = IRRemote::rawCommand(code);
        r.repeat   = false;
        uint8_t ai = static_cast<uint8_t>((code >>  8) & 0xFFu);
        uint8_t ci = static_cast<uint8_t>((code >> 24) & 0xFFu);
        r.valid = ((r.address ^ ai) == 0xFFu) && ((r.command ^ ci) == 0xFFu);
        return r;
    }
};

} // namespace MikroDuino
