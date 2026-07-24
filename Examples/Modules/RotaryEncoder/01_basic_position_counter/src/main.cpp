/*
 * RotaryEncoder Basics — count(), begin(), PCINT routing — MikroDuino Module SDK
 *
 * The simplest possible use of the RotaryEncoder module: decode a
 * quadrature encoder's A/B channels in the background via Pin Change
 * Interrupts and print the running position whenever it changes. This
 * is project 1 of 6 in the examples/Modules/RotaryEncoder series, which
 * walks the RotaryEncoder class from a bare position counter up to a
 * capstone LCD settings menu combining RotaryEncoder, LCD, and EEPROM.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ CH_A    │ PD2   │ Encoder channel A, internal pull-up        │
 *   │ CH_B    │ PD3   │ Encoder channel B, internal pull-up        │
 *   │ COM     │ —     │ Encoder common pin -> GND                   │
 *   │ TXD     │ PD1   │ USB-serial adapter, 9600 8N1                │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * Relationship to other quadrature decoders in this SDK:
 *   - examples/interrupts/05_quadrature_encoder builds the SAME decode
 *     table by hand on top of raw INT0/INT1 — worth reading first if
 *     you want to see what this module does internally.
 *   - mikroduino/math/gray_code.hpp's Encoder is a POLLING helper: you
 *     call update(a, b) yourself every loop pass. RotaryEncoder is
 *     interrupt-driven and self-contained instead — it configures
 *     PCINT, decodes in the ISR, and just hands you a running count().
 *
 * Why PCINT instead of INT0/INT1: the raw-interrupt series above is
 * pinned to PD2/PD3 because those are the ONLY two pins with dedicated
 * external interrupts on the ATmega328P. RotaryEncoder uses Pin Change
 * Interrupts instead, which exist on every GPIO pin — this project
 * still wires A/B to PD2/PD3 for a direct comparison, but the module
 * works identically on any pin pair (see project 4 for a pair split
 * across two different PCINT banks).
 *
 * RotaryEncoder concepts introduced:
 *   - RotaryEncoder(pinA, pinB) — no push button on this pin pair, so
 *     the third constructor argument is left at its NO_PIN default.
 *   - begin() — enables the internal pull-ups on A/B and turns on PCINT
 *     for both pins. It deliberately does NOT call sei() — that stays
 *     the caller's decision, made explicit below.
 *   - _isrHandler() — the actual quadrature decode logic. It must be
 *     called from whichever PCINT vector the wired pins belong to; the
 *     library cannot know that at compile time because PCINT vectors
 *     are shared across 8-pin banks. PD2/PD3 both live in PORTD, so a
 *     single ISR(PCINT2_vect) covers this project.
 *   - count() — the running signed position, CW = +, CCW = -. Reads it
 *     under an atomic block internally, so it's always safe to call
 *     from main() even while the ISR could fire mid-read.
 *   - resetCount(v) — re-zeroes (or re-seeds) the position; used here
 *     once at startup just to make the starting point explicit.
 */

#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/usart.hpp>
#include <mikroduino/registers.hpp>
#include <RotaryEncoder.hpp>

using namespace MikroDuino;

RotaryEncoder encoder(PD2, PD3);   // A, B — no button on this pin pair

ISR(PCINT2_vect) { encoder._isrHandler(); }   // PD2/PD3 both live in PORTD

int main() {
    USART0.begin(9600);
    USART0.writeLine_P(PSTR("RotaryEncoder basic position counter"));
    USART0.writeLine_P(PSTR("========================================"));
    USART0.writeLine_P(PSTR("Rotate the encoder — position updates below."));
    USART0.writeLine_P(PSTR(""));

    encoder.begin();
    encoder.resetCount(0);
    sei();   // interrupts stay off until begin() has configured PCINT

    int32_t lastPrinted = encoder.count();

    while (true) {
        int32_t pos = encoder.count();

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
    }
}
