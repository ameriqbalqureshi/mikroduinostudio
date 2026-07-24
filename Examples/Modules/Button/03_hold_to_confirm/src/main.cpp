/*
 * Button Hold-to-Confirm — Non-Blocking update() + longPressed() —
 * MikroDuino Module SDK
 *
 * Project 3 of 6 in the examples/Modules/Button series. Introduces
 * longPressed() and the heldMs()-driven "arming" UI pattern used by real
 * products for anything you don't want triggered by an accidental tap
 * (factory reset, pairing mode, arm/disarm) — LED blinks faster and
 * faster the longer the button stays down, then confirms with a solid
 * flash the instant the long-press threshold is crossed.
 *
 * This project also retires projects 1-2's `_delay_ms(1)` busy-wait loop
 * in favour of a proper millis()-scheduled call to update() — because
 * main() finally has real, concurrent work to do (the variable-rate LED
 * animation) that a blocking delay would make impossible.
 *
 * Hardware: identical to projects 1-2 — button on PD2, LED on PB5, USART0
 * at 9600 8N1 on PD1.
 *
 * Why swap the timing source now: Button::update() must be called at a
 * steady ~1 ms cadence (see project 1's header comment) — but nothing
 * says that cadence has to come from blocking the whole CPU with
 * _delay_ms(1). This project reuses the exact hand-rolled Timer0-overflow
 * millis() technique from examples/timer/02_software_millis_clock: an
 * accurate, interrupt-driven millisecond clock that keeps ticking in the
 * background while main() is free to also drive the LED's blink-rate
 * animation, all in the same loop pass.
 *
 * Button concepts introduced:
 *   - longPressed() — a one-shot event, true exactly once when the
 *     button has been held continuously for >= longPressMs. Per the
 *     module header comment, a press that fires longPressed() will NOT
 *     also fire clicked() when released — the two are mutually
 *     exclusive outcomes of one press, decided by how long it lasted.
 *   - setLongPressMs(ms) — reconfigures the long-press threshold after
 *     construction. The constructor default (600 ms) suits quick
 *     "long-press to enter a menu" gestures; a deliberate hold-to-confirm
 *     UI usually wants longer, so this project raises it to 1500 ms —
 *     long enough that it can't be triggered by accident, short enough
 *     not to feel unresponsive.
 *   - heldMs() used continuously (not just at release) to drive real-time
 *     UI feedback — the whole point of it being a side-effect-free STATE
 *     query (project 2) rather than a one-shot event: it can be polled
 *     every single loop pass without consuming anything.
 *
 * Button concepts reused from projects 1-2:
 *   - Button(pin), begin(), update(), pressed(), released(), clicked(),
 *     isDown().
 */

#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/usart.hpp>
#include <mikroduino/timer.hpp>
#include <mikroduino/registers.hpp>
#include <Button.hpp>

using namespace MikroDuino;

static constexpr uint8_t LED = PB5;

Button button(PD2);

// ── millis() clock (identical technique to examples/timer/02_software_millis_clock) ──

static constexpr uint16_t MICROS_PER_OVERFLOW = 1024;   // DIV64, 256 ticks
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

// ── Arming-blink helper ─────────────────────────────────────────────────

static constexpr uint16_t LONG_PRESS_MS   = 1500;
static constexpr uint16_t BLINK_SLOW_MS   = 300;   // period when just pressed
static constexpr uint16_t BLINK_FAST_MS   = 40;    // period right before confirming

// Maps how far into the hold we are (0..longPressMs) to a blink period
// that shrinks from BLINK_SLOW_MS down to BLINK_FAST_MS — the classic
// "getting more urgent" progress indicator.
static uint16_t blinkPeriodFor(uint16_t heldMs) {
    if (heldMs >= LONG_PRESS_MS) return BLINK_FAST_MS;
    uint32_t span = BLINK_SLOW_MS - BLINK_FAST_MS;
    uint32_t drop = (span * heldMs) / LONG_PRESS_MS;
    return static_cast<uint16_t>(BLINK_SLOW_MS - drop);
}

int main() {
    GPIO::output(LED);
    GPIO::clear(LED);

    button.begin();
    button.setLongPressMs(LONG_PRESS_MS);   // override the 600 ms default

    Timer0.mode(TimerMode::Normal);
    Timer0.prescaler(TimerPrescaler::DIV64);
    Timer0.start();
    Timer0.enableOverflow();
    sei();

    USART0.begin(9600);
    USART0.writeLine_P(PSTR("Button hold-to-confirm"));
    USART0.writeLine_P(PSTR("========================"));
    USART0.write_P(PSTR("Long-press threshold set to "));
    USART0.writeInt(LONG_PRESS_MS);
    USART0.writeLine_P(PSTR(" ms via setLongPressMs()."));
    USART0.writeLine_P(PSTR("Hold the button and watch the LED blink faster as it arms."));
    USART0.writeLine_P(PSTR(""));

    uint32_t lastUpdateMs   = millis();
    uint32_t nextBlinkAt    = 0;
    bool     ledOn          = false;
    bool     confirmedFlash = false;
    uint32_t confirmedUntil = 0;

    while (true) {
        uint32_t now = millis();

        // Catch-up loop: call update() exactly once per elapsed
        // millisecond, however many that turns out to be since the last
        // pass — never fewer, never more, regardless of what else this
        // loop iteration spent time on.
        while (now - lastUpdateMs >= 1) {
            button.update();
            ++lastUpdateMs;
        }

        if (button.pressed()) {
            USART0.writeLine_P(PSTR("DOWN — arming..."));
            nextBlinkAt = now;
        }

        if (button.longPressed()) {
            USART0.writeLine_P(PSTR(">>> CONFIRMED (long press) <<<"));
            confirmedFlash = true;
            confirmedUntil = now + 800;
            GPIO::set(LED);
        }

        if (button.clicked()) {
            // Only reachable on a SHORT press+release — longPressed()
            // already consumed the long-press case above, and the two
            // never both fire for the same press (see header comment).
            USART0.writeLine_P(PSTR("Released early — short click, NOT confirmed."));
        }

        if (button.released() && !confirmedFlash) {
            GPIO::clear(LED);
        }

        // Solid confirmation flash takes priority over the arming blink.
        if (confirmedFlash) {
            if (now >= confirmedUntil) {
                confirmedFlash = false;
                GPIO::clear(LED);
            }
        } else if (button.isDown()) {
            // Non-blocking variable-rate blink while arming — this is
            // exactly the kind of concurrent work a _delay_ms(1) busy
            // loop (projects 1-2) could not have made room for.
            if (now >= nextBlinkAt) {
                ledOn = !ledOn;
                GPIO::write(LED, ledOn);
                nextBlinkAt = now + blinkPeriodFor(button.heldMs());
            }
        }
    }
}
