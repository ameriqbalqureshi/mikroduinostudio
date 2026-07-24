/*
 * Quadrature Rotary Encoder Decoder — MikroDuino SDK
 *
 * Project 5 of 6 in the examples/interrupts series. Uses INT0 and INT1
 * COOPERATIVELY for the first time in this series — not as two
 * independent buttons, but as the two phase-shifted channels (A and B)
 * of a single incremental rotary encoder, decoded together to track
 * position AND direction.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ CH_A    │ PD2   │ INT0 — encoder channel A, internal pull-up│
 *   │ CH_B    │ PD3   │ INT1 — encoder channel B, internal pull-up│
 *   │ COM     │ —     │ Encoder common pin -> GND                  │
 *   │ TXD     │ PD1   │ USB-serial adapter                        │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * How a quadrature encoder encodes direction: channels A and B are two
 * square waves 90 degrees out of phase. Which one leads the other tells
 * you the direction of rotation — reading either channel alone can only
 * tell you THAT something moved, never which way. A standard mechanical
 * detent encoder produces one full A/B quadrature cycle (4 distinct
 * 2-bit states) per detent "click".
 *
 * The decode technique: treat the current {A, B} reading as a 2-bit
 * state (0-3) and look up the transition from the PREVIOUS state to the
 * current one in a 16-entry table (4 possible "from" states x 4 possible
 * "to" states) that encodes +1, -1, or 0 (an invalid/bounced transition,
 * ignored) for every combination. Both ISRs call the SAME update
 * function — it doesn't matter whether an A edge or a B edge triggered
 * it, because the function always re-reads BOTH pins' current level
 * rather than assuming which one changed.
 *
 * Interrupt concepts reused from projects 1-4:
 *   - Interrupt.attach() on both INT0 and INT1, IntSense::Change this
 *     time (project 3 introduced Change; this is its natural use case —
 *     every transition of either channel, not just one edge direction,
 *     carries position information).
 *   - Two ISRs sharing state safely: quadratureUpdate() runs from
 *     whichever of ISR(INT0_vect)/ISR(INT1_vect) fires, both with global
 *     interrupts off (the AVR default), so the two can never actually
 *     run concurrently with each other — only interleave strictly one
 *     at a time — which is what makes g_prevState safe to read-then-
 *     write without any extra protection inside the handler.
 *
 * g_position is a 32-bit volatile read from main() (not an ISR), so —
 * same reasoning as the timer series' millis() and this series' project
 * 3/4 counters — it's read inside ATOMIC_BLOCK_START/END to avoid a torn
 * multi-byte read.
 */

#include <avr/pgmspace.h>
#include <util/delay.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/interrupt.hpp>
#include <mikroduino/usart.hpp>
#include <mikroduino/registers.hpp>

using namespace MikroDuino;

static constexpr uint8_t CH_A = PD2;   // INT0
static constexpr uint8_t CH_B = PD3;   // INT1

// Quadrature transition table. Index = (prevState << 2) | newState,
// where each state is (A << 1) | B, 0-3. Value is the position delta for
// that transition: +1, -1, or 0 for a transition that shouldn't happen
// in clean quadrature (treated as bounce/noise and ignored).
static const int8_t QUAD_TABLE[16] PROGMEM = {
     0, -1, +1,  0,
    +1,  0,  0, -1,
    -1,  0,  0, +1,
     0, +1, -1,  0,
};

static volatile int32_t g_position  = 0;
static volatile uint8_t g_prevState = 0;

static void quadratureUpdate() {
    uint8_t a = GPIO::read(CH_A) ? 1 : 0;
    uint8_t b = GPIO::read(CH_B) ? 1 : 0;
    uint8_t newState = static_cast<uint8_t>((a << 1) | b);

    uint8_t index = static_cast<uint8_t>((g_prevState << 2) | newState);
    int8_t delta = static_cast<int8_t>(pgm_read_byte(&QUAD_TABLE[index]));

    g_position += delta;
    g_prevState = newState;
}

void onChannelAEdge() { quadratureUpdate(); }
void onChannelBEdge() { quadratureUpdate(); }

static int32_t readPosition() {
    int32_t snapshot;
    ATOMIC_BLOCK_START;
    snapshot = g_position;
    ATOMIC_BLOCK_END;
    return snapshot;
}

static void delay_ms(uint16_t ms) { while (ms--) _delay_ms(1); }

int main() {
    GPIO::inputPullup(CH_A);
    GPIO::inputPullup(CH_B);

    USART0.begin(9600);
    USART0.writeLine_P(PSTR("Quadrature rotary encoder decoder"));
    USART0.writeLine_P(PSTR("===================================="));
    USART0.writeLine_P(PSTR("Rotate the encoder — position updates below."));
    USART0.writeLine_P(PSTR(""));

    // Seed g_prevState with the encoder's current resting state, so the
    // very first edge after boot decodes against reality instead of an
    // assumed 00 state.
    g_prevState = static_cast<uint8_t>((GPIO::read(CH_A) ? 2 : 0) | (GPIO::read(CH_B) ? 1 : 0));

    Interrupt.attach(IntSource::INT0, onChannelAEdge, IntSense::Change);
    Interrupt.attach(IntSource::INT1, onChannelBEdge, IntSense::Change);
    Interrupt.enableGlobal();

    int32_t lastPrinted = 0;

    while (true) {
        int32_t pos = readPosition();

        if (pos != lastPrinted) {
            USART0.write_P(PSTR("position="));
            USART0.writeInt(pos);

            // Most mechanical detent encoders produce 4 raw quadrature
            // counts per physical "click" — dividing by 4 gives a more
            // human-meaningful click count alongside the raw value.
            USART0.write_P(PSTR("   clicks~="));
            USART0.writeInt(pos / 4);

            USART0.write_P(PSTR("   ("));
            USART0.write_P((pos > lastPrinted) ? PSTR("CW") : PSTR("CCW"));
            USART0.writeLine_P(PSTR(")"));

            lastPrinted = pos;
        }

        delay_ms(50);
    }
}
