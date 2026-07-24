/*
 * PWM Two-Channel Crossfade — MikroDuino SDK
 *
 * Project 2 of 6 in the examples/pwm series. Adds a second LED on
 * PWM1Driver's other Timer1 channel and introduces independent per-
 * channel control: simultaneous duty cycles, and stopping/restarting one
 * channel without disturbing the other.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ Signal  │ Pin   │ Wiring                                    │
 *   ├─────────┼───────┼──────────────────────────────────────────┤
 *   │ OC1A    │ PB1   │ LED_A + 220 Ω resistor -> GND               │
 *   │ OC1B    │ PB2   │ LED_B + 220 Ω resistor -> GND               │
 *   │ TXD     │ PD1   │ USB-serial adapter                        │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * Both channels share the SAME Timer1 hardware — one prescaler, one TOP
 * value, one PWM frequency for both — set once by begin(). What's
 * independent per channel is only the compare value (how much of that
 * shared period each pin is high for), which is exactly what dutyA() /
 * dutyB() / rawA() / rawB() each control separately.
 *
 * PWM concepts introduced:
 *   - PWM1.dutyB(percent) — the OC1B (PB2) equivalent of dutyA(). Called
 *     independently of dutyA(); the two channels don't interact except
 *     through sharing the same underlying PWM frequency.
 *   - PWM1.stopA() / PWM1.stopB() — disables that channel's hardware
 *     compare output (clears COM1x bits) and returns its pin to a plain
 *     GPIO input. The timer itself keeps running and the OTHER channel
 *     is completely unaffected — this is the practical proof that the
 *     two channels are independent outputs of one shared timer, not two
 *     separate timers.
 *   - Calling dutyA()/dutyB() again after a stop*() automatically
 *     re-enables that channel's compare output (see enableChannelA() /
 *     enableChannelB() in pwm.hpp) — there's no separate "restart" call.
 *
 * PWM concept reused from project 1:
 *   - PWM1.begin(frequencyHz, type), PWM1.dutyA(percent).
 */

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <mikroduino/pwm.hpp>
#include <mikroduino/usart.hpp>

using namespace MikroDuino;

static void delay_ms(uint16_t ms) { while (ms--) _delay_ms(1); }

int main() {
    PWM1.begin(500, PWMType::FastPWM);   // one shared 500 Hz period for both channels

    USART0.begin(9600);
    USART0.writeLine_P(PSTR("PWM two-channel crossfade (OC1A + OC1B)"));
    USART0.writeLine_P(PSTR("========================================="));

    while (true) {
        // -----------------------------------------------------------------
        // Independent fixed levels: A at 75%, B at 25%, then swapped.
        // Proves the two channels really are set separately.
        // -----------------------------------------------------------------
        USART0.writeLine_P(PSTR("A=75% B=25%"));
        PWM1.dutyA(75);
        PWM1.dutyB(25);
        delay_ms(1500);

        USART0.writeLine_P(PSTR("A=25% B=75%"));
        PWM1.dutyA(25);
        PWM1.dutyB(75);
        delay_ms(1500);

        // -----------------------------------------------------------------
        // Crossfade: as A rises, B falls by the same amount, so the two
        // LEDs' combined brightness looks roughly constant while the
        // "which one is brighter" balance sweeps from B to A.
        // -----------------------------------------------------------------
        USART0.writeLine_P(PSTR("Crossfade A up / B down"));
        for (uint8_t d = 0; d <= 100; ++d) {
            PWM1.dutyA(d);
            PWM1.dutyB(static_cast<uint8_t>(100 - d));
            delay_ms(15);
        }

        USART0.writeLine_P(PSTR("Crossfade A down / B up"));
        for (uint8_t d = 100; d != 0; --d) {
            PWM1.dutyA(d);
            PWM1.dutyB(static_cast<uint8_t>(100 - d));
            delay_ms(15);
        }

        // -----------------------------------------------------------------
        // stopA() / stopB(): demonstrate that each channel can be halted
        // independently. LED_B should go dark and stay dark while LED_A
        // keeps pulsing — the timer and channel A are untouched by
        // stopping channel B.
        // -----------------------------------------------------------------
        USART0.writeLine_P(PSTR("stopB(): channel B off, channel A keeps running at 60%"));
        PWM1.dutyA(60);
        PWM1.stopB();
        delay_ms(2000);

        USART0.writeLine_P(PSTR("dutyB(60) again: channel B resumes automatically"));
        PWM1.dutyB(60);   // re-enables OC1B's compare output — no separate "start" call
        delay_ms(1500);

        USART0.writeLine_P(PSTR("stopA(): channel A off, channel B keeps running at 60%"));
        PWM1.stopA();
        delay_ms(2000);

        USART0.writeLine_P(PSTR("dutyA(60) again: channel A resumes automatically"));
        PWM1.dutyA(60);
        delay_ms(1500);

        USART0.writeLine_P(PSTR(""));
    }
}
