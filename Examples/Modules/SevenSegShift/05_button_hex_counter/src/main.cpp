/*
 * SevenSegShift Button Hex Counter — Button + printHex() + Non-Blocking millis() —
 * MikroDuino Module SDK
 *
 * Project 5 of 6 in the examples/Modules/SevenSegShift series. Combines
 * project 3's ISR-refreshed display with the Button module's debounced
 * click/double-click/long-press vocabulary, and switches to the same
 * Timer0-overflow millis() technique examples/Modules/SevenSeg/05 uses,
 * so Button::update() runs on its own accurate 1 ms schedule independent
 * of the display's refresh timer.
 *
 * Hardware: identical 3-pin wiring to projects 1-4 (PD4=data, PD5=clock,
 * PD6=latch), plus one push button:
 *
 *   ┌────────┬───────┬──────────────────────────────────────────┐
 *   │ Button │ PC0   │ Other leg to GND — active-LOW, internal      │
 *   │        │       │ pull-up (Button's default)                   │
 *   └────────┴───────┴──────────────────────────────────────────┘
 *
 * Why TWO timers at once: Timer1 keeps doing exactly what it did in
 * project 3 — a 500 Hz CTC compare-match interrupt that does nothing
 * but call mx.refresh() — while a SEPARATE Timer0 overflow interrupt
 * (identical technique to examples/timer/02_software_millis_clock)
 * maintains a free-running millis() clock the main loop uses to call
 * Button::update() exactly once per real millisecond, as its own
 * documentation requires for debounceMs/longPressMs/doubleClickMs to
 * mean actual milliseconds rather than "however often the main loop
 * happens to spin."
 *
 * Button concepts reused from sdk/modules/Button's own example series:
 *   - Button(pin), begin(), update(), clicked(), doubleClicked(),
 *     longPressed().
 *
 * SevenSegShift<N> concepts reused from projects 1-3:
 *   - SevenSegShift<4>(dataPin, clockPin, latchPin), begin(), refresh()
 *     (ISR-driven, as in project 3), print(uint16_t, leadingZero).
 *
 * New SevenSegShift<N> concept:
 *   - printHex(n) — formats a 16-bit value as exactly N hex digits,
 *     always left-padded with '0' (unlike print()'s optional
 *     leadingZero, printHex() has no blank-padding option — a hex
 *     readout is conventionally shown at fixed width). This project
 *     toggles between print()'s decimal, blank-padded style and
 *     printHex()'s fixed-width hex style on demand.
 *
 * Gestures (one button, PC0):
 *   clicked()       -> counter++ (wraps 0xFFFF -> 0x0000)
 *   doubleClicked() -> toggle display mode, decimal <-> hex
 *   longPressed()   -> reset counter to 0
 */

#include <avr/interrupt.h>
#include <mikroduino/timer.hpp>
#include <mikroduino/registers.hpp>
#include <Button.hpp>
#include <SevenSegShift.hpp>

using namespace MikroDuino;

SevenSegShift<4> mx(PD4, PD5, PD6);
Button           countButton(PC0);

// ── Display refresh, ~500 Hz via Timer1 (identical technique to project 3) ──

ISR(TIMER1_COMPA_vect) {
    mx.refresh();
}

// ── millis() clock (identical technique to examples/timer/02_software_millis_clock) ──

static constexpr uint16_t MICROS_PER_OVERFLOW = 1024;
static constexpr uint16_t MILLIS_INC = MICROS_PER_OVERFLOW / 1000;
static constexpr uint16_t FRACT_INC  = MICROS_PER_OVERFLOW % 1000;
static constexpr uint16_t FRACT_MAX  = 1000;

static volatile uint32_t g_millis = 0;
static volatile uint16_t g_fract  = 0;

ISR(TIMER0_OVF_vect) {
    uint32_t m = g_millis;
    uint16_t f = g_fract;
    m += MILLIS_INC;
    f += FRACT_INC;
    if (f >= FRACT_MAX) { f -= FRACT_MAX; ++m; }
    g_fract  = f;
    g_millis = m;
}

static uint32_t millis() {
    uint32_t snapshot;
    ATOMIC_BLOCK_START;
    snapshot = g_millis;
    ATOMIC_BLOCK_END;
    return snapshot;
}

int main() {
    mx.begin();
    countButton.begin();

    Timer1.mode(TimerMode::CTC);
    Timer1.prescaler(TimerPrescaler::DIV64);
    Timer1.compareA(Timer1.ticksForHz(500));
    Timer1.start();
    Timer1.enableInterruptA();

    Timer0.mode(TimerMode::Normal);
    Timer0.prescaler(TimerPrescaler::DIV64);
    Timer0.start();
    Timer0.enableOverflow();

    sei();

    uint16_t counter  = 0;
    bool     hexMode  = false;
    bool     dirty    = true;

    uint32_t lastUpdateMs = millis();

    while (true) {
        uint32_t now = millis();
        while (now - lastUpdateMs >= 1) {
            countButton.update();
            ++lastUpdateMs;
        }

        if (countButton.clicked()) {
            ++counter;   // wraps 0xFFFF -> 0x0000 naturally
            dirty = true;
        }
        if (countButton.doubleClicked()) {
            hexMode = !hexMode;
            dirty = true;
        }
        if (countButton.longPressed()) {
            counter = 0;
            dirty = true;
        }

        if (dirty) {
            if (hexMode) mx.printHex(counter);
            else         mx.print(counter, false);
            dirty = false;
        }
    }
}
