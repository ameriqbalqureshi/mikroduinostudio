/*
 * RotaryEncoder Integrated Button — updateButton(), buttonPressed(),
 * buttonHeldMs() — MikroDuino Module SDK
 *
 * Project 3 of 6 in the examples/Modules/RotaryEncoder series. Adds the
 * encoder's own THIRD pin — its optional integrated push button — on
 * top of project 2's bounded counter. Most physical rotary encoders
 * (the kind with a knob you can also click straight down) expose this
 * as a normal momentary switch to ground; RotaryEncoder debounces it
 * internally exactly like the standalone Button module, without
 * needing a second object.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ CH_A    │ PD2   │ Encoder channel A, internal pull-up        │
 *   │ CH_B    │ PD3   │ Encoder channel B, internal pull-up        │
 *   │ SW      │ PD4   │ Encoder's integrated push button (click),  │
 *   │         │       │ active-LOW, internal pull-up                │
 *   │ LED     │ PB5   │ Warns once a hold is long enough to arm a  │
 *   │         │       │ reset, then flashes 3x when it fires        │
 *   │ COM     │ —     │ Encoder common pin -> GND                   │
 *   │ TXD     │ PD1   │ USB-serial adapter, 9600 8N1                │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * Gestures:
 *   short click (press + release before the hold threshold) -> print
 *     the current count on demand
 *   hold >= RESET_HOLD_MS -> zero the counter; the LED lights once the
 *     hold is more than half-armed as an early warning, then flashes
 *     3 times to confirm the reset fired
 *
 * RotaryEncoder concepts introduced:
 *   - RotaryEncoder(pinA, pinB, pinBtn) — the third constructor
 *     argument wires up the integrated button; begin() enables its
 *     pull-up alongside A/B's.
 *   - updateButton() — must be called once per millisecond, same
 *     contract as the standalone Button module's update(). This
 *     project drives it from a Timer0-overflow millis() clock (same
 *     technique as examples/timer/02_software_millis_clock) instead of
 *     a blocking _delay_ms(1), so encoder rotation keeps being decoded
 *     by the PCINT ISR the whole time regardless of button state.
 *   - buttonPressed() / buttonReleased() — one-shot debounced edge
 *     events, each true exactly once per physical transition.
 *   - buttonIsDown() / buttonHeldMs() — live state queries (no side
 *     effect) used here to drive the arm-then-fire reset gesture.
 */

#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/usart.hpp>
#include <mikroduino/timer.hpp>
#include <mikroduino/registers.hpp>
#include <RotaryEncoder.hpp>

using namespace MikroDuino;

static constexpr uint8_t LED = PB5;

RotaryEncoder encoder(PD2, PD3, PD4);   // A, B, integrated button

ISR(PCINT2_vect) { encoder._isrHandler(); }

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

static constexpr uint16_t RESET_HOLD_MS = 1200;
static constexpr uint16_t WARN_HOLD_MS  = RESET_HOLD_MS / 2;

int main() {
    USART0.begin(9600);
    USART0.writeLine_P(PSTR("RotaryEncoder integrated button"));
    USART0.writeLine_P(PSTR("================================="));
    USART0.writeLine_P(PSTR("Click: print count.  Hold >1.2s: reset to zero."));
    USART0.writeLine_P(PSTR(""));

    GPIO::output(LED);
    GPIO::clear(LED);

    encoder.begin();

    Timer0.mode(TimerMode::Normal);
    Timer0.prescaler(TimerPrescaler::DIV64);
    Timer0.start();
    Timer0.enableOverflow();
    sei();

    uint32_t lastUpdateMs = millis();
    bool     resetArmed   = false;   // guards against firing more than once per hold

    while (true) {
        uint32_t now = millis();

        while (now - lastUpdateMs >= 1) {
            encoder.updateButton();
            ++lastUpdateMs;
        }

        // Drain the debounced edges every pass so neither flag sits stale.
        encoder.buttonPressed();
        if (encoder.buttonReleased() && !resetArmed) {
            USART0.write_P(PSTR("count="));
            USART0.writeInt(encoder.count());
            USART0.writeLine_P(PSTR(""));
        }

        if (encoder.buttonIsDown()) {
            uint16_t held = encoder.buttonHeldMs();
            GPIO::write(LED, held >= WARN_HOLD_MS);

            if (!resetArmed && held >= RESET_HOLD_MS) {
                resetArmed = true;   // fire once per hold, not once per loop pass
                encoder.resetCount(0);

                for (uint8_t i = 0; i < 3; ++i) {
                    GPIO::clear(LED); _delay_ms(80);
                    GPIO::set(LED);   _delay_ms(80);
                }
                GPIO::clear(LED);

                USART0.writeLine_P(PSTR("RESET -> count=0"));
            }
        } else {
            resetArmed = false;   // released: next hold can arm again
            GPIO::clear(LED);
        }
    }
}
