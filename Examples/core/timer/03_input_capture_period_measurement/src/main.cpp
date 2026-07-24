/*
 * Timer1 Input Capture — Frequency Measurement — MikroDuino SDK
 *
 * Project 3 of 6 in the examples/timer series. Two timers cooperate on
 * one board: Timer0 (CTC, like project 1 but toggling a plain GPIO pin
 * by hand instead of just counting) generates a known-frequency test
 * square wave, and Timer1's INPUT CAPTURE hardware measures that same
 * wave's period completely independently — no polling, no counting
 * edges in software, just reading a register the instant an edge
 * arrives.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌─────────────┬───────┬──────────────────────────────────────────┐
 *   │ Signal      │ Pin   │ Wiring                                    │
 *   ├─────────────┼───────┼──────────────────────────────────────────┤
 *   │ TEST_SIGNAL │ PD6   │ Generated 1 kHz square wave (Timer0 CTC,  │
 *   │             │       │ toggled by hand in the ISR — see below)   │
 *   │ ICP1        │ PB0   │ Timer1's input capture pin — jumper a     │
 *   │             │       │ wire from PD6 to PB0 to feed the test     │
 *   │             │       │ signal into the capture hardware           │
 *   │ TXD         │ PD1   │ USB-serial adapter                        │
 *   └─────────────┴───────┴──────────────────────────────────────────┘
 *
 * In a real project PB0 would instead come from an external sensor —
 * a tachometer pickup, a frequency-modulated sensor output, an
 * encoder channel — anything producing a repeating edge whose timing
 * carries information. Generating the test signal locally means this
 * project needs nothing but a single jumper wire to run and verify.
 *
 * Why Timer0 toggles PD6 "by hand": Timer0Driver's CTC mode only
 * configures the WGM/prescaler bits — unlike PWM1Driver (the pwm/
 * examples series), it never touches the COM0x bits that would let
 * hardware drive a physical pin automatically. So the ISR below does
 * the pin toggle itself with a plain GPIO::toggle() call. This is a
 * useful technique in its own right: a hardware-timed compare interrupt
 * gives an exact, jitter-free period even when you toggle the pin "by
 * hand" in software, because the ISR itself fires at an exact hardware-
 * clocked instant — only the toggle instruction's few CPU cycles of
 * latency are software-timed, not the interval between toggles.
 *
 * Timer concepts introduced:
 *   - Timer1.enableCapture() / disableCapture() — sets/clears ICIE1 in
 *     TIMSK1. With it set, TIMER1_CAPT_vect fires the instant a
 *     configured edge arrives on ICP1 (PB0), and hardware has ALREADY
 *     latched the counter's current value into ICR1 by the time the ISR
 *     runs — no race with software polling can miss the exact tick.
 *   - Direct TCCR1B bit access for ICES1 (Input Capture Edge Select) and
 *     ICNC1 (Input Capture Noise Canceler) — Timer1Driver has no
 *     wrapper methods for either, so this project reaches for
 *     BITSET()/BITCLEAR() directly, the same "driver covers the common
 *     case, raw registers cover the rest" pattern used throughout this
 *     whole SDK (see e.g. the i2c/spi series reaching for raw registers
 *     alongside I2CDriver/SPIDriver).
 *   - IMPORTANT ordering detail: Timer1Driver::start() writes TCCR1A and
 *     TCCR1B WHOLESALE (not OR'd with the existing value), so ICES1/
 *     ICNC1 must be set AFTER calling Timer1.start(), never before —
 *     setting them first would just get silently overwritten.
 *   - ICR1 read directly (not through the driver) inside the ISR — the
 *     hardware-captured tick count at the moment the edge arrived.
 *     (Timer1.inputCapture(value) is a SETTER for ICR1, used only when
 *     ICR1 serves as the TOP register in certain PWM/CTC waveform modes
 *     — the opposite direction from this project, which reads a
 *     hardware-written ICR1 instead.)
 *   - Timer1.prescalerValue() reused from project 1 to convert a
 *     measured tick delta back into a real frequency in Hz.
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/timer.hpp>
#include <mikroduino/usart.hpp>
#include <mikroduino/registers.hpp>

using namespace MikroDuino;

static constexpr uint8_t TEST_SIGNAL_OUT = PD6;

// ── Timer0: CTC test-signal generator (1 kHz square wave) ─────────────────
//
// At DIV64 (4 us/tick), a compare value of 124 gives a compare match
// every 125 ticks = 500 us. Toggling the pin on every match therefore
// flips it every 500 us -> one full high+low cycle every 1 ms -> 1 kHz.
ISR(TIMER0_COMPA_vect) {
    GPIO::toggle(TEST_SIGNAL_OUT);
}

// ── Timer1: input capture period measurement ───────────────────────────────

static volatile uint16_t g_measuredTicks = 0;
static volatile bool     g_measurementReady = false;

ISR(TIMER1_CAPT_vect) {
    static uint16_t lastCapture = 0;

    uint16_t captured = ICR1;   // hardware already latched this at the edge

    // Unsigned subtraction wraps correctly even if the 16-bit counter
    // rolled over between captures — no special-case needed.
    g_measuredTicks = static_cast<uint16_t>(captured - lastCapture);
    g_measurementReady = true;

    lastCapture = captured;
}

int main() {
    GPIO::output(TEST_SIGNAL_OUT);

    USART0.begin(9600);
    USART0.writeLine_P(PSTR("Timer1 input capture: frequency measurement"));
    USART0.writeLine_P(PSTR("=============================================="));
    USART0.writeLine_P(PSTR("Jumper PD6 -> PB0 (ICP1) to feed the test signal in."));
    USART0.writeLine_P(PSTR(""));

    // Start the 1 kHz test signal generator.
    Timer0.mode(TimerMode::CTC);
    Timer0.prescaler(TimerPrescaler::DIV64);
    Timer0.compareA(124);
    Timer0.start();
    Timer0.enableInterruptA();

    // Start Timer1 as a free-running counter for capture timing. DIV8
    // (0.5 us/tick) gives ~2000 ticks per expected 1 ms period -> good
    // measurement resolution without risking a 16-bit overflow between
    // captures (65536 ticks would be a ~32 ms period, far above 1 ms).
    Timer1.mode(TimerMode::Normal);
    Timer1.prescaler(TimerPrescaler::DIV8);
    Timer1.start();

    // ICES1/ICNC1 must be set AFTER start() — see header comment.
    BITSET(TCCR1B, ICES1);   // capture on rising edge
    BITSET(TCCR1B, ICNC1);   // noise canceler: require 4 consistent samples first

    Timer1.enableCapture();

    sei();

    while (true) {
        if (g_measurementReady) {
            uint16_t ticks;
            ATOMIC_BLOCK_START;
            ticks = g_measuredTicks;
            g_measurementReady = false;
            ATOMIC_BLOCK_END;

            // frequency = timer_clock / ticks_per_period
            //           = (F_CPU / prescaler) / ticks
            uint32_t timerClockHz = F_CPU / Timer1.prescalerValue();
            uint32_t measuredHz = (ticks > 0) ? (timerClockHz / ticks) : 0;

            USART0.write_P(PSTR("period = "));
            USART0.writeInt(static_cast<int32_t>(ticks));
            USART0.write_P(PSTR(" ticks  ->  "));
            USART0.writeInt(static_cast<int32_t>(measuredHz));
            USART0.writeLine_P(PSTR(" Hz  (expect ~1000 Hz)"));
        }
    }
}
