#include "Pulse.hpp"

namespace MikroDuino {

// ── Stopwatch static member ─────────────────────────────────────────────────
volatile uint16_t Stopwatch::_ovf = 0u;

// ── PulseMeter::pulseWidth ──────────────────────────────────────────────────
//
// Sync → leading edge → timer reset → measure trailing edge.
// The sync step discards any pulse already in progress so the result is
// always a complete, clean pulse.
//
uint32_t PulseMeter::pulseWidth(bool polarity, uint32_t timeoutUs) {
    uint32_t maxTicks = detail::usToTicks(timeoutUs);
    _timerStart();

    // Phase 1 & 2 — sync to idle then catch leading edge.
    // Both use the same ovf counter so the combined timeout applies.
    {
        uint32_t ovf = 0u;
        TIFR1 = (1u << TOV1);
        if (!_awaitState(!polarity, ovf, maxTicks)) { _timerStop(); return 0UL; }
        if (!_awaitState( polarity, ovf, maxTicks)) { _timerStop(); return 0UL; }
    }

    // Phase 3 — leading edge has arrived; reset timer and measure active time.
    TCNT1 = 0u;
    uint32_t ticks = _measureState(polarity, maxTicks);   // exits on trailing edge
    _timerStop();

    return (ticks == 0xFFFFFFFFUL) ? 0UL : detail::ticksToUs(ticks);
}

// ── PulseMeter::period ──────────────────────────────────────────────────────
//
// Sync → first edge (start counting) → through idle → second same edge → done.
//
uint32_t PulseMeter::period(bool risingEdge, uint32_t timeoutUs) {
    uint32_t maxTicks = detail::usToTicks(timeoutUs);
    _timerStart();

    // Phase 1 & 2 — sync to idle, catch first edge (no measurement yet).
    {
        uint32_t ovf = 0u;
        TIFR1 = (1u << TOV1);
        if (!_awaitState(!risingEdge, ovf, maxTicks)) { _timerStop(); return 0UL; }
        if (!_awaitState( risingEdge, ovf, maxTicks)) { _timerStop(); return 0UL; }
    }

    // Phase 3 & 4 — first edge captured; reset timer, then walk to next same edge.
    TCNT1 = 0u; TIFR1 = (1u << TOV1);
    uint32_t ovf = 0u;

    if (!_awaitState(!risingEdge, ovf, maxTicks)) { _timerStop(); return 0UL; }
    if (!_awaitState( risingEdge, ovf, maxTicks)) { _timerStop(); return 0UL; }

    // Second same edge — total ticks from first edge to here = one period.
    uint32_t ticks = ovf * 65536UL + TCNT1;
    _timerStop();
    return detail::ticksToUs(ticks);
}

// ── PulseMeter::frequency ───────────────────────────────────────────────────
//
// Count rising edges while Timer1 < windowUs.  TIFR1 is polled every
// iteration so no ISR is needed and no overflow is missed.
//
uint32_t PulseMeter::frequency(uint32_t windowUs) {
    uint32_t maxTicks = detail::usToTicks(windowUs);
    uint32_t maxOvf   = maxTicks >> 16u;
    uint16_t maxRem   = static_cast<uint16_t>(maxTicks & 0xFFFFu);

    _timerStart();
    TIFR1 = (1u << TOV1);

    uint32_t edges     = 0u;
    uint32_t ovf       = 0u;
    bool     prevState = GPIO::read(_pin);

    while (true) {
        if (TIFR1 & (1u << TOV1)) { TIFR1 = (1u << TOV1); ovf++; }
        if (ovf > maxOvf) break;
        if (ovf == maxOvf && TCNT1 >= maxRem) break;

        bool curState = GPIO::read(_pin);
        if (!prevState && curState) edges++;  // count rising edges
        prevState = curState;
    }

    _timerStop();
    if (edges == 0u) return 0UL;

    // Hz = edges / windowUs * 1 000 000
    return static_cast<uint32_t>(
        static_cast<uint64_t>(edges) * 1000000ULL / windowUs);
}

} // namespace MikroDuino
