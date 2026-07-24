/*
 * Timer0 Overflow — Hand-Rolled millis() Clock — MikroDuino SDK
 *
 * Project 2 of 6 in the examples/timer series. Builds a from-scratch
 * millisecond clock out of Timer0's OVERFLOW interrupt (rather than
 * project 1's CTC compare match), and uses it to blink an LED without a
 * single _delay_ms() call — the classic "blink without delay" pattern,
 * here built on raw MikroDuino timer primitives instead of Arduino's
 * built-in millis().
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ LED     │ PB5   │ Blinks at a fixed interval, timed by       │
 *   │         │       │ software millis(), not _delay_ms()         │
 *   │ Button  │ PD2   │ Other leg to GND, internal pull-up — press │
 *   │         │       │ pauses/resumes the millis clock             │
 *   │ TXD     │ PD1   │ USB-serial adapter                        │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * Why overflow instead of CTC this time: Timer0 is only 8 bits, so its
 * NORMAL-mode counter always overflows at the same natural point — 256
 * ticks — with no compare register needed at all. That's simpler to set
 * up than CTC, but it comes with a real cost this project exists to show:
 * 256 ticks at a 64x prescale is 256 * 64 / 16,000,000 s = 1.024 ms per
 * overflow, NOT a clean 1.000 ms. Naively incrementing a millis counter
 * by 1 on every overflow would drift by 2.4% (about 1.4 seconds per
 * minute) — very noticeable over any real runtime.
 *
 * The fix, taken directly from how Arduino's own wiring.c implements
 * millis(), is a fractional-microsecond accumulator: track the leftover
 * 24 microseconds (1024 us - 1000 us) each overflow doesn't quite
 * account for, and roll an extra millisecond in whenever that leftover
 * accumulates past 1000 us. This keeps the clock accurate on average
 * even though no single overflow is exactly 1 ms.
 *
 * Timer concepts introduced:
 *   - Timer0.mode(TimerMode::Normal) — the counter simply counts 0-255
 *     and wraps, no compare register involved.
 *   - Timer0.enableOverflow() / Timer0.disableOverflow() — sets/clears
 *     TOIE0 in TIMSK0. With it set, TIMER0_OVF_vect fires every time the
 *     counter wraps from 255 back to 0. This project's pause button uses
 *     disableOverflow()/enableOverflow() to freeze and resume the millis
 *     clock live — the ISR simply stops firing while disabled, and the
 *     accumulated millis/fractional state is untouched, so timing picks
 *     up exactly where it left off on resume.
 *   - ISR(TIMER0_OVF_vect) — the fractional accumulator lives here.
 *
 * Timer concept reused from project 1:
 *   - Timer0.prescaler(TimerPrescaler::DIV64), Timer0.start().
 *
 * A note on volatile multi-byte reads: g_millis is a 32-bit value
 * updated by an ISR that can interrupt main() mid-read on an 8-bit CPU.
 * Reading it without protection could observe a half-updated value.
 * millis() below wraps the read in ATOMIC_BLOCK_START/END (from
 * registers.hpp) to guarantee a consistent snapshot — the same pattern
 * used anywhere a multi-byte volatile is shared between an ISR and
 * main().
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/timer.hpp>
#include <mikroduino/usart.hpp>
#include <mikroduino/registers.hpp>

using namespace MikroDuino;

static constexpr uint8_t LED    = PB5;
static constexpr uint8_t BUTTON = PD2;

// At DIV64 on a 16 MHz clock, one Timer0 overflow (256 ticks) takes
// 256 * 64 / 16 = 1024 microseconds.
static constexpr uint16_t MICROS_PER_OVERFLOW = 1024;
static constexpr uint16_t MILLIS_INC = MICROS_PER_OVERFLOW / 1000;   // 1
static constexpr uint16_t FRACT_INC  = MICROS_PER_OVERFLOW % 1000;   // 24
static constexpr uint16_t FRACT_MAX  = 1000;

static volatile uint32_t g_millis = 0;
static volatile uint16_t g_fract  = 0;

ISR(TIMER0_OVF_vect) {
    uint32_t m = g_millis;
    uint16_t f = g_fract;

    m += MILLIS_INC;
    f += FRACT_INC;
    if (f >= FRACT_MAX) {
        f -= FRACT_MAX;
        ++m;   // the accumulated fraction just crossed a whole millisecond
    }

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
    GPIO::output(LED);
    GPIO::inputPullup(BUTTON);

    USART0.begin(9600);
    USART0.writeLine_P(PSTR("Timer0 overflow: hand-rolled millis() clock"));
    USART0.writeLine_P(PSTR("=============================================="));
    USART0.writeLine_P(PSTR("Non-blocking blink, timed entirely by software millis()."));
    USART0.writeLine_P(PSTR("Press the button to pause/resume the clock."));

    Timer0.mode(TimerMode::Normal);
    Timer0.prescaler(TimerPrescaler::DIV64);
    Timer0.start();
    Timer0.enableOverflow();
    sei();

    static constexpr uint32_t BLINK_INTERVAL_MS = 400;
    uint32_t nextBlinkAt = BLINK_INTERVAL_MS;
    bool ledState = false;

    static constexpr uint32_t STATUS_INTERVAL_MS = 2000;
    uint32_t nextStatusAt = STATUS_INTERVAL_MS;

    bool clockRunning = true;
    bool lastButtonState = true;

    while (true) {
        uint32_t now = millis();   // one snapshot per loop iteration is enough

        // -----------------------------------------------------------------
        // Non-blocking blink: instead of _delay_ms(200), just check
        // whether enough time has passed and, if so, toggle and schedule
        // the NEXT toggle. main() never blocks here — it's free to also
        // service the button and status print in the same loop pass.
        // -----------------------------------------------------------------
        if (clockRunning && now >= nextBlinkAt) {
            ledState = !ledState;
            GPIO::write(LED, ledState);
            nextBlinkAt = now + BLINK_INTERVAL_MS;
        }

        // -----------------------------------------------------------------
        // Pause/resume button — falling-edge detect with a short settle
        // delay is normally used for debounce (see the gpio/usart/i2c/spi
        // series), but a plain _delay_ms() here would defeat the entire
        // point of this project, so instead this just requires the pin to
        // read low on two consecutive loop passes before acting — good
        // enough debounce without blocking the millis clock even briefly.
        // -----------------------------------------------------------------
        static uint8_t lowStreak = 0;
        bool nowLow = (GPIO::read(BUTTON) == false);
        lowStreak = nowLow ? static_cast<uint8_t>(lowStreak + 1) : 0;

        if (lowStreak == 2 && lastButtonState) {
            clockRunning = !clockRunning;
            if (clockRunning) {
                Timer0.enableOverflow();
                USART0.writeLine_P(PSTR(">> millis() RESUMED"));
            } else {
                Timer0.disableOverflow();
                USART0.writeLine_P(PSTR(">> millis() PAUSED"));
            }
        }
        lastButtonState = !nowLow;

        // -----------------------------------------------------------------
        // Periodic status line — also scheduled off millis(), not a
        // separate delay. Because it's driven by the same frozen-while-
        // paused clock as the blink, this line stops updating too while
        // paused — proof that pausing Timer0's overflow interrupt really
        // does freeze every millis()-scheduled activity at once, not just
        // the LED.
        // -----------------------------------------------------------------
        if (now >= nextStatusAt) {
            USART0.write_P(PSTR("millis()="));
            USART0.writeInt(static_cast<int32_t>(now));
            USART0.write_P(PSTR("  state="));
            USART0.writeLine_P(clockRunning ? PSTR("running") : PSTR("paused"));
            nextStatusAt = now + STATUS_INTERVAL_MS;
        }
    }
}
