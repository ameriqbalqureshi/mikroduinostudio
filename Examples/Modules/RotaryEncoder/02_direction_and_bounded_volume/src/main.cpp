/*
 * RotaryEncoder Direction + Bounded Volume — direction(), resetCount() —
 * MikroDuino Module SDK
 *
 * Project 2 of 6 in the examples/Modules/RotaryEncoder series. Project 1
 * printed the raw, unbounded count() — fine for a diagnostic readout,
 * but most real controls (a volume knob, a menu index, an LED
 * brightness dial) need to be CLAMPED to a fixed range instead of
 * growing or shrinking forever. This project builds that on top of
 * direction() rather than count(), and shows why the two don't mix
 * without keeping them in sync.
 *
 * Hardware: identical to project 1 (ATmega328P @ 16 MHz):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ CH_A    │ PD2   │ Encoder channel A, internal pull-up        │
 *   │ CH_B    │ PD3   │ Encoder channel B, internal pull-up        │
 *   │ COM     │ —     │ Encoder common pin -> GND                   │
 *   │ TXD     │ PD1   │ USB-serial adapter, 9600 8N1                │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * RotaryEncoder concepts introduced:
 *   - direction() — returns +1 (CW), -1 (CCW), or 0 (no new step) since
 *     the LAST call, then clears its own cache. Unlike count(), it is
 *     an EDGE report, not a running total: call it once per loop pass
 *     and each detent step is reported exactly once, whichever way it
 *     went, regardless of how the application chooses to use it.
 *
 * Why resetCount() matters here: the ISR keeps accumulating the real
 * quadrature count in the background no matter what the application
 * does with direction() — count() and direction() share the SAME
 * internal step, just read out two different ways. If this project
 * only ever consumed direction() and let count() free-run underneath,
 * a long session sitting at the volume=20 ceiling while still turning
 * the knob CW would silently rack up a huge internal count() that
 * bears no relation to the on-screen value. resetCount(volume) below
 * re-seeds the internal counter to match the clamped value every time
 * a step is applied, so the two never drift apart.
 */

#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <mikroduino/usart.hpp>
#include <RotaryEncoder.hpp>

using namespace MikroDuino;

RotaryEncoder encoder(PD2, PD3);   // A, B — no button on this pin pair

ISR(PCINT2_vect) { encoder._isrHandler(); }

static constexpr int8_t VOLUME_MIN = 0;
static constexpr int8_t VOLUME_MAX = 20;

static void printBar(int8_t volume) {
    USART0.write_P(PSTR("volume ["));
    for (int8_t i = 0; i < VOLUME_MAX; ++i)
        USART0.write(i < volume ? '#' : ' ');
    USART0.write_P(PSTR("] "));
    USART0.writeInt(volume);
    USART0.write_P(PSTR("/"));
    USART0.writeInt(VOLUME_MAX);
    USART0.writeLine_P(PSTR(""));
}

int main() {
    USART0.begin(9600);
    USART0.writeLine_P(PSTR("RotaryEncoder direction() + bounded volume"));
    USART0.writeLine_P(PSTR("========================================="));
    USART0.writeLine_P(PSTR("Rotate the encoder to raise/lower the volume."));
    USART0.writeLine_P(PSTR(""));

    encoder.begin();
    sei();

    int8_t volume = 10;   // start at mid-scale so both directions are visible
    encoder.resetCount(volume);
    printBar(volume);

    while (true) {
        int8_t step = encoder.direction();

        if (step != 0) {
            int16_t next = static_cast<int16_t>(volume) + step;
            if (next < VOLUME_MIN) next = VOLUME_MIN;
            if (next > VOLUME_MAX) next = VOLUME_MAX;
            volume = static_cast<int8_t>(next);

            // Keep the internal quadrature counter pinned to the clamped
            // value — see the file header comment for why this matters.
            encoder.resetCount(volume);

            printBar(volume);
        }
    }
}
