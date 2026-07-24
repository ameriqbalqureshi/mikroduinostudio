/*
 * PWM Tone Generator — Piezo Buzzer Melody — MikroDuino SDK
 *
 * Project 4 of 6 in the examples/pwm series. Every project before this
 * one used PWM's DUTY CYCLE to control something (LED brightness) while
 * holding frequency fixed. This project does the opposite: it holds duty
 * cycle fixed at 50% and varies FREQUENCY instead, which is what a piezo
 * buzzer needs to produce different musical pitches. Same PWM1Driver
 * API, a completely different physical effect, just by changing which
 * parameter of the same square wave gets varied.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ Signal  │ Pin   │ Wiring                                    │
 *   ├─────────┼───────┼──────────────────────────────────────────┤
 *   │ OC1A    │ PB1   │ Passive piezo buzzer + -> PB1, - -> GND    │
 *   │ TXD     │ PD1   │ USB-serial adapter                        │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * "Passive" piezo matters here: passive buzzers have no internal
 * oscillator and reproduce whatever square wave frequency you drive them
 * with as an audible tone — exactly what PWM1.begin(frequencyHz, ...)
 * provides. (An "active" buzzer has its own fixed-frequency driver chip
 * built in and only needs a plain on/off GPIO signal — driving one with
 * PWM would just make it click at whatever rate you re-trigger it.)
 *
 * PWM concepts introduced:
 *   - Using PWM1.begin(frequencyHz, ...) itself as the "play this pitch"
 *     call, rather than calling it once at setup — each note in the
 *     melody re-runs begin() at a new frequency, which re-derives a new
 *     prescaler/TOP for Timer1 and therefore genuinely changes the pitch
 *     the piezo reproduces.
 *   - dutyA(50) as a MOSTLY-fixed setting here: 50% is simply "on for
 *     exactly half of each cycle", which for an audio square wave gives
 *     the loudest, most symmetric tone. Unlike the LED projects, the
 *     duty value itself doesn't encode any information the ear can
 *     easily perceive at these frequencies — the CHANGING FREQUENCY is
 *     what carries the melody, not the duty cycle.
 *   - PWM1.stopA() used for rests (silence) between notes, instead of
 *     dutyA(0) — stopping the channel actually releases PB1 back to a
 *     floating GPIO input, which is a cleaner "silent" than a still-
 *     driven 0% duty output.
 *
 * PWM concepts reused from projects 1-3:
 *   - PWM1.begin(frequencyHz, PWMType::FastPWM), PWM1.dutyA(percent).
 */

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <mikroduino/pwm.hpp>
#include <mikroduino/usart.hpp>

using namespace MikroDuino;

static void delay_ms(uint16_t ms) { while (ms--) _delay_ms(1); }

// Standard equal-tempered note frequencies (Hz), rounded to the nearest
// integer — plenty accurate for a piezo buzzer's ear-perceived pitch.
namespace Note {
    constexpr uint16_t C4 = 262, D4 = 294, E4 = 330, F4 = 349;
    constexpr uint16_t G4 = 392, A4 = 440, B4 = 494, C5 = 523;
    constexpr uint16_t REST = 0;   // silence, not a frequency
}

// "Twinkle Twinkle Little Star", opening phrase. Two parallel PROGMEM
// arrays (frequency, duration) rather than an array of structs — simpler
// to read back with pgm_read_word() than reaching into packed struct
// members stored in flash.
static const uint16_t MELODY_FREQ[] PROGMEM = {
    Note::C4, Note::C4, Note::G4, Note::G4,
    Note::A4, Note::A4, Note::G4, Note::REST,
    Note::F4, Note::F4, Note::E4, Note::E4,
    Note::D4, Note::D4, Note::C4, Note::REST,
};
static const uint16_t MELODY_DUR[] PROGMEM = {
    300, 300, 300, 300,
    300, 300, 600, 300,
    300, 300, 300, 300,
    300, 300, 600, 300,
};
static constexpr uint8_t MELODY_LEN = sizeof(MELODY_FREQ) / sizeof(MELODY_FREQ[0]);

// Play one note: a real frequency drives the piezo at 50% duty for
// `durationMs`; REST (0 Hz) instead stops the channel for that long.
static void playNote(uint16_t freqHz, uint16_t durationMs) {
    if (freqHz == Note::REST) {
        PWM1.stopA();
    } else {
        PWM1.begin(freqHz, PWMType::FastPWM);   // re-derives Timer1's prescaler/TOP for this pitch
        PWM1.dutyA(50);
    }
    delay_ms(durationMs);

    PWM1.stopA();       // brief silent gap between notes, so repeated
    delay_ms(20);        // pitches (the two C4's, two A4's, ...) are
                          // audibly separate notes rather than one
                          // continuous tone.
}

int main() {
    USART0.begin(9600);
    USART0.writeLine_P(PSTR("PWM piezo tone melody"));
    USART0.writeLine_P(PSTR("======================="));

    while (true) {
        USART0.writeLine_P(PSTR("Playing: Twinkle Twinkle Little Star (opening phrase)"));

        for (uint8_t i = 0; i < MELODY_LEN; ++i) {
            uint16_t freq = pgm_read_word(&MELODY_FREQ[i]);
            uint16_t dur  = pgm_read_word(&MELODY_DUR[i]);

            USART0.write_P(PSTR("  note "));
            USART0.writeInt(i);
            USART0.write_P(PSTR(": "));
            if (freq == Note::REST) {
                USART0.write_P(PSTR("rest"));
            } else {
                USART0.writeInt(freq);
                USART0.write_P(PSTR(" Hz"));
            }
            USART0.write_P(PSTR(" for "));
            USART0.writeInt(dur);
            USART0.writeLine_P(PSTR(" ms"));

            playNote(freq, dur);
        }

        USART0.writeLine_P(PSTR(""));
        delay_ms(1500);
    }
}
