/*
 * Timer1 External Clock Source — Hardware Pulse Counter — MikroDuino SDK
 *
 * Project 4 of 6 in the examples/timer series. Configures Timer1 to
 * count EXTERNAL pulses arriving on its T1 pin directly in hardware —
 * no ISR per pulse, no polling per pulse, zero CPU cost per edge no
 * matter how fast they arrive (up to the timer's own maximum count
 * rate). This is exactly how a real tachometer, flow meter, or Geiger
 * counter front-end is built.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌─────────────┬───────┬──────────────────────────────────────────┐
 *   │ Signal      │ Pin   │ Wiring                                    │
 *   ├─────────────┼───────┼──────────────────────────────────────────┤
 *   │ T1          │ PD5   │ Timer1's external clock input — pulses    │
 *   │             │       │ arriving here are counted entirely by     │
 *   │             │       │ hardware while this runs                  │
 *   │ TEST_PULSE  │ PD6   │ Self-test ~1 kHz square wave, toggled in  │
 *   │             │       │ software (see below) — jumper PD6 -> PD5  │
 *   │             │       │ to test without a real sensor              │
 *   │ TXD         │ PD1   │ USB-serial adapter                        │
 *   └─────────────┴───────┴──────────────────────────────────────────┘
 *
 * In a real project, PD5 (T1) would connect to a Hall-effect sensor, an
 * IR slot sensor on a spinning wheel, a flow meter's reed switch, or
 * similar — anything producing one pulse per event to count. The
 * TEST_PULSE generator here exists purely so this project can be
 * verified with nothing but a jumper wire: it's a plain software toggle
 * in the main loop (a fixed _delay_us(500) between GPIO::toggle() calls,
 * NOT a hardware timer), deliberately simple and deliberately NOT the
 * point of this project.
 *
 * Timer concepts introduced:
 *   - Timer1.prescaler(TimerPrescaler::ExtRise) — instead of dividing
 *     F_CPU, this switches Timer1's clock source entirely to the T1 pin
 *     itself, incrementing once per rising edge seen there. (ExtFall is
 *     the falling-edge equivalent.) Timer1.mode(TimerMode::Normal) is
 *     used alongside it — no waveform generation is wanted, just a
 *     free-running count driven by an external signal instead of the
 *     internal clock.
 *   - Timer1.reset() — zeroes TCNT1, used here to start each fresh
 *     measurement window at exactly zero counted pulses.
 *   - Timer1.count() — read the number of pulses counted since the last
 *     reset(). Over a known window length (1 second, timed by the
 *     Timer0-based millis() below), this IS the frequency in Hz
 *     directly — no tick-to-Hz conversion needed, unlike project 3's
 *     internal-clock-based measurement, because every count here IS one
 *     external event, not an internal timer tick.
 *
 * Timer concept reused from project 2:
 *   - The same Timer0 overflow-interrupt millis() clock (mode(Normal),
 *     prescaler(DIV64), enableOverflow(), fractional-microsecond
 *     accumulator) provides the 1-second measurement window here. It
 *     keeps ticking accurately in the background even while main()
 *     spends most of its time in the TEST_PULSE generator's blocking
 *     _delay_us() calls — proof that an interrupt-driven clock, unlike
 *     millis() built on manual delay-counting, is immune to whatever
 *     else main() happens to be busy doing.
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/timer.hpp>
#include <mikroduino/usart.hpp>
#include <mikroduino/registers.hpp>

using namespace MikroDuino;

static constexpr uint8_t TEST_PULSE_OUT = PD6;

// ── Timer0: millis() clock (identical technique to project 2) ─────────────

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

int main() {
    GPIO::output(TEST_PULSE_OUT);

    USART0.begin(9600);
    USART0.writeLine_P(PSTR("Timer1 external clock source: hardware pulse counter"));
    USART0.writeLine_P(PSTR("======================================================="));
    USART0.writeLine_P(PSTR("Jumper PD6 -> PD5 (T1) for the ~1 kHz self-test signal."));
    USART0.writeLine_P(PSTR(""));

    // Start the millis() clock (background, unaffected by main()'s
    // blocking delay loop below).
    Timer0.mode(TimerMode::Normal);
    Timer0.prescaler(TimerPrescaler::DIV64);
    Timer0.start();
    Timer0.enableOverflow();

    // Configure Timer1 to count external rising edges on T1 (PD5)
    // instead of running off the internal clock.
    Timer1.mode(TimerMode::Normal);
    Timer1.prescaler(TimerPrescaler::ExtRise);
    Timer1.start();
    Timer1.reset();

    sei();

    uint32_t windowStart = millis();
    static constexpr uint32_t WINDOW_MS = 1000;

    while (true) {
        // Self-test pulse generator: a plain software square wave, ~1 kHz
        // (500 us high + 500 us low). This is the ONLY reason main()
        // blocks here — Timer1's pulse counting keeps working in pure
        // hardware regardless, and Timer0's millis() ISR keeps ticking
        // in the background too.
        GPIO::toggle(TEST_PULSE_OUT);
        _delay_us(500);

        uint32_t now = millis();
        if (now - windowStart >= WINDOW_MS) {
            uint16_t pulses = Timer1.count();
            Timer1.reset();
            windowStart = now;

            USART0.write_P(PSTR("pulses/sec = "));
            USART0.writeInt(static_cast<int32_t>(pulses));
            USART0.writeLine_P(PSTR("  (expect ~1000 with the test jumper connected)"));
        }
    }
}
