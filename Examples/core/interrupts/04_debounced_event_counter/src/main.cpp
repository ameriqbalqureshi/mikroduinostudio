/*
 * ISR-Side Debouncing: Debounced vs Raw Button Counters — MikroDuino SDK
 *
 * Project 4 of 6 in the examples/interrupts series. Wires up two
 * identical buttons side by side — one counted with a debounce lockout,
 * one counted completely raw — so the effect of mechanical contact
 * bounce on an edge-triggered interrupt is directly visible instead of
 * just described. Press each button once, the same way, and compare the
 * two totals.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌──────────────┬───────┬──────────────────────────────────────────┐
 *   │ Button0 (deb) │ PD2  │ INT0 — other leg to GND, internal pull-up │
 *   │ LED0          │ PB5  │ Toggles only on a debounced, accepted press│
 *   │ Button1 (raw) │ PD3  │ INT1 — other leg to GND, internal pull-up │
 *   │ LED1          │ PB4  │ Toggles on EVERY raw edge, bounce included │
 *   │ TXD           │ PD1  │ USB-serial adapter                        │
 *   └──────────────┴───────┴──────────────────────────────────────────┘
 *
 * The problem: a mechanical switch doesn't transition cleanly from open
 * to closed. The contacts physically bounce for a few milliseconds,
 * producing a rapid train of several extra high-low-high edges before
 * settling — invisible to a human pressing the button, but very visible
 * to hardware fast enough to catch every one of them, which an edge-
 * triggered interrupt is by definition. Button1 (raw) below will very
 * likely over-count a single press by several — try it.
 *
 * The fix: give each accepted trigger a "lockout window" (50 ms here) —
 * any further edges arriving before the window expires are ignored, AND
 * (this part matters) the window is extended by every edge seen, even
 * ignored ones, so a noisy bounce train that's still actively bouncing
 * when the window would otherwise have expired keeps getting pushed
 * back until the signal is truly quiet. This is the ISR-resident
 * equivalent of the multi-consecutive-sample debounce used in the timer
 * series' capstone (ISR(TIMER1_COMPB_vect)); this project's version
 * uses elapsed TIME instead of a sample count, made possible by having
 * an accurate millis() clock on hand.
 *
 * Interrupt concepts reused from projects 1-3:
 *   - Interrupt.attach() with IntSense::Falling on two independent
 *     sources, Interrupt.enableGlobal().
 *
 * Timer concept reused from the timer series (project 2/4/6):
 *   - The same hand-rolled Timer0-overflow millis() clock, providing the
 *     time base the debounce lockout is measured against. Calling
 *     millis() from inside onButton0Debounced() (an ISR itself) is safe
 *     here: AVR ISRs run with global interrupts disabled by default (no
 *     sei() is called inside either handler), so the Timer0 overflow ISR
 *     cannot preempt mid-read — the ATOMIC_BLOCK_START/END inside
 *     millis() simply finds interrupts already off and leaves them that
 *     way.
 */

#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/interrupt.hpp>
#include <mikroduino/usart.hpp>
#include <mikroduino/timer.hpp>
#include <mikroduino/registers.hpp>

using namespace MikroDuino;

static constexpr uint8_t LED0 = PB5;
static constexpr uint8_t LED1 = PB4;

// ── millis() clock (identical technique to the timer series) ──────────────

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

// ── Debounced counter (INT0) ────────────────────────────────────────────

static constexpr uint16_t DEBOUNCE_MS = 50;

static volatile uint32_t g_lastAcceptedEdgeMs = 0;
static volatile uint16_t g_debouncedCount = 0;

void onButton0Debounced() {
    uint32_t now = millis();

    if (now - g_lastAcceptedEdgeMs >= DEBOUNCE_MS) {
        ++g_debouncedCount;
        GPIO::toggle(LED0);
    }
    // Extend the lockout on EVERY edge, accepted or not — a bounce train
    // still in progress keeps pushing the window forward instead of
    // letting a mid-bounce edge sneak through once the original window
    // expires.
    g_lastAcceptedEdgeMs = now;
}

// ── Raw counter (INT1) — no debounce at all, on purpose ───────────────────

static volatile uint16_t g_rawCount = 0;

void onButton1Raw() {
    ++g_rawCount;
    GPIO::toggle(LED1);
}

static uint16_t readCount16(volatile uint16_t& counter) {
    uint16_t v;
    ATOMIC_BLOCK_START;
    v = counter;
    ATOMIC_BLOCK_END;
    return v;
}

static void delay_ms(uint16_t ms) { while (ms--) _delay_ms(1); }

int main() {
    GPIO::output(LED0);
    GPIO::output(LED1);
    GPIO::clear(LED0);
    GPIO::clear(LED1);
    GPIO::inputPullup(PD2);
    GPIO::inputPullup(PD3);

    Timer0.mode(TimerMode::Normal);
    Timer0.prescaler(TimerPrescaler::DIV64);
    Timer0.start();
    Timer0.enableOverflow();

    USART0.begin(9600);
    USART0.writeLine_P(PSTR("Debounced (INT0) vs raw (INT1) button counters"));
    USART0.writeLine_P(PSTR("=================================================="));
    USART0.writeLine_P(PSTR("Press each button once, the same way, and compare the totals."));
    USART0.writeLine_P(PSTR(""));

    Interrupt.attach(IntSource::INT0, onButton0Debounced, IntSense::Falling);
    Interrupt.attach(IntSource::INT1, onButton1Raw, IntSense::Falling);
    Interrupt.enableGlobal();

    uint16_t lastPrintedDebounced = 0xFFFF;
    uint16_t lastPrintedRaw       = 0xFFFF;

    while (true) {
        uint16_t debounced = readCount16(g_debouncedCount);
        uint16_t raw       = readCount16(g_rawCount);

        if (debounced != lastPrintedDebounced || raw != lastPrintedRaw) {
            USART0.write_P(PSTR("debounced="));
            USART0.writeInt(static_cast<int32_t>(debounced));
            USART0.write_P(PSTR("   raw="));
            USART0.writeInt(static_cast<int32_t>(raw));

            if (raw > debounced) {
                USART0.write_P(PSTR("   <- raw over-counted by "));
                USART0.writeInt(static_cast<int32_t>(raw - debounced));
                USART0.write_P(PSTR(" (contact bounce)"));
            }
            USART0.writeLine_P(PSTR(""));

            lastPrintedDebounced = debounced;
            lastPrintedRaw = raw;
        }

        delay_ms(20);
    }
}
