/*
 * RotaryEncoder Split Across Two PCINT Banks — dual ISR routing —
 * MikroDuino Module SDK
 *
 * Project 4 of 6 in the examples/Modules/RotaryEncoder series. Projects
 * 1-3 all wired A and B onto the same port (PORTD), so a single
 * ISR(PCINT2_vect) covered both. That's the common case, but nothing
 * about RotaryEncoder actually requires it — the class configures
 * whichever PCINT bank each pin happens to belong to, independently.
 * This project deliberately puts A on PORTD and B on PORTB to exercise
 * the "different banks" case the module's own header comment calls
 * out explicitly, and proves both ISRs safely share one encoder object.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ CH_A    │ PD2   │ Encoder channel A — PORTD -> PCINT2_vect   │
 *   │ CH_B    │ PB0   │ Encoder channel B — PORTB -> PCINT0_vect   │
 *   │ COM     │ —     │ Encoder common pin -> GND                   │
 *   │ HEARTBEAT│ PB5  │ Blinks on its own schedule, independent of  │
 *   │         │       │ encoder activity — proof the loop never      │
 *   │         │       │ blocks waiting on rotation                   │
 *   │ TXD     │ PD1   │ USB-serial adapter, 9600 8N1                │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * PCINT groups on the ATmega328P (see RotaryEncoder.hpp's own header
 * comment for the same table):
 *   PCINT0_vect -> PB0-PB7  (port B)
 *   PCINT1_vect -> PC0-PC6  (port C)
 *   PCINT2_vect -> PD0-PD7  (port D)
 *
 * Why this is safe: _isrHandler() always re-reads BOTH pins' CURRENT
 * level and looks up the prev-to-current transition in its table,
 * regardless of which physical pin's edge woke it up — the same
 * "either ISR calls the same update function" pattern used by the raw
 * INT0/INT1 version in examples/interrupts/05_quadrature_encoder. It
 * doesn't matter whether PCINT0_vect or PCINT2_vect fires first for a
 * given step; whichever one runs first sees the true combined state
 * and decodes it correctly, and the AVR never runs two ISRs at once.
 *
 * RotaryEncoder concepts reused from projects 1-3:
 *   - RotaryEncoder(pinA, pinB), begin(), count().
 *
 * New concept: routing _isrHandler() from MORE than one ISR vector,
 * because begin() enabled PCINT on two different banks for the same
 * object.
 */

#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/usart.hpp>
#include <mikroduino/timer.hpp>
#include <mikroduino/registers.hpp>
#include <RotaryEncoder.hpp>

using namespace MikroDuino;

static constexpr uint8_t HEARTBEAT_LED = PB5;

RotaryEncoder encoder(PD2, PB0);   // A on PORTD, B on PORTB

ISR(PCINT2_vect) { encoder._isrHandler(); }   // covers CH_A (PD2)
ISR(PCINT0_vect) { encoder._isrHandler(); }   // covers CH_B (PB0)

// ── millis() clock (same technique as examples/timer/02_software_millis_clock) ──

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

static constexpr uint16_t HEARTBEAT_PERIOD_MS = 500;

int main() {
    USART0.begin(9600);
    USART0.writeLine_P(PSTR("RotaryEncoder split across two PCINT banks"));
    USART0.writeLine_P(PSTR("============================================"));
    USART0.writeLine_P(PSTR("CH_A on PORTD (PCINT2), CH_B on PORTB (PCINT0)."));
    USART0.writeLine_P(PSTR(""));

    GPIO::output(HEARTBEAT_LED);
    GPIO::clear(HEARTBEAT_LED);

    encoder.begin();   // enables PCINT0 for B and PCINT2 for A

    Timer0.mode(TimerMode::Normal);
    Timer0.prescaler(TimerPrescaler::DIV64);
    Timer0.start();
    Timer0.enableOverflow();
    sei();

    int32_t  lastPrinted   = encoder.count();
    uint32_t nextBeatAt    = millis();
    bool     heartbeatOn   = false;

    while (true) {
        uint32_t now = millis();

        // ---- Encoder position, printed only when it actually changes ----
        int32_t pos = encoder.count();
        if (pos != lastPrinted) {
            USART0.write_P(PSTR("position="));
            USART0.writeInt(pos);
            USART0.write_P(PSTR("   ("));
            USART0.write_P((pos > lastPrinted) ? PSTR("CW") : PSTR("CCW"));
            USART0.writeLine_P(PSTR(")"));
            lastPrinted = pos;
        }

        // ---- Heartbeat, on its own independent schedule ----
        if (now >= nextBeatAt) {
            nextBeatAt = now + HEARTBEAT_PERIOD_MS;
            heartbeatOn = !heartbeatOn;
            GPIO::write(HEARTBEAT_LED, heartbeatOn);
        }
    }
}
