/*
 * IRRemote PWM Brightness Ramp — Detecting "Button Released" from NEC's
 * Repeat Frames — MikroDuino Module SDK
 *
 * Project 4 of 6 in the examples/Modules/IRRemote series. Uses VOL+/VOL-
 * to ramp an LED's brightness up/down smoothly WHILE HELD, which exposes
 * something NEC's protocol never actually tells you: a button "release".
 * This project's real subject is how to infer one anyway.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ IR OUT  │ PD2   │ INT0, same fixed pin as project 3            │
 *   │ LED     │ PB1   │ OC1A — hardware PWM brightness              │
 *   │ TXD     │ PD1   │ USB-serial adapter, 9600 8N1                │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * The problem this project solves: NEC only ever sends a full 32-bit
 * frame ONCE, the instant a button is pressed. For as long as the
 * button stays down, the remote instead sends a short, fixed "still
 * held" repeat signal roughly every 108 ms — and the INSTANT the button
 * is released, the remote simply stops sending anything. There is no
 * "key up" frame to catch. So "is the button still held" can only ever
 * be answered by "how long ago was the last frame (press or repeat) for
 * this button?" — if that gap ever exceeds one repeat interval by a
 * safe margin, the button must have been released, because a still-held
 * button would have sent another repeat by now.
 *
 * IRRemote concepts reused from project 3:
 *   - IRRemote::begin(), sei(), available(), read() — identical usage.
 *   - c.repeat distinguishes a brand new press from a "still held" frame
 *     — but this time, repeats are the whole point instead of being
 *     ignored: every accepted VOL+ frame (press OR repeat) nudges
 *     brightness up by one step, and every accepted VOL- frame nudges it
 *     down, so holding the button produces a smooth ramp for free,
 *     driven entirely by however fast the remote itself re-sends
 *     repeats — no separate ramp-speed timer needed.
 *
 * New concept: RELEASE_TIMEOUT_MS-based release detection. Every
 * accepted VOL+/VOL- frame stamps `lastFrameAt = millis()`. A plain
 * millis()-gated check elsewhere in the loop compares `now -
 * lastFrameAt` against RELEASE_TIMEOUT_MS (200 ms — a bit more than
 * NEC's ~108 ms repeat interval, to absorb one missed repeat without
 * false-triggering): once that gap is exceeded, the button is declared
 * released and a final "settled at N%" message prints. This project
 * uses that purely for a status message; project 5 reuses the exact
 * same technique for something that actually matters — automatically
 * stopping a motor when the driver lets go of the remote.
 *
 * New concept: PWM1 (sdk/core/avr/include/mikroduino/pwm.hpp) — 16-bit
 * hardware PWM via Timer1. PWM1.begin(frequencyHz) computes and loads
 * Timer1's prescaler/TOP automatically; dutyA(percent) sets OC1A's (PB1)
 * duty cycle 0-100 and enables the pin as an output the first time it's
 * called. Timer1 is untouched by IRRemote (which uses Timer2) or by
 * this project's own millis() clock (which uses Timer0), so all three
 * peripherals — IR decoding, PWM brightness, and timekeeping — run
 * fully independently.
 */

#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/usart.hpp>
#include <mikroduino/pwm.hpp>
#include <mikroduino/timer.hpp>
#include <mikroduino/registers.hpp>
#include <IRRemote.hpp>

using namespace MikroDuino;

static constexpr uint8_t DEVICE_ADDRESS = 0x00;
static constexpr uint8_t CMD_VOL_UP     = 0x15;   // reference 21-key remote
static constexpr uint8_t CMD_VOL_DOWN   = 0x07;

static constexpr uint8_t  BRIGHTNESS_STEP        = 5;    // % per repeat frame
static constexpr uint16_t RELEASE_TIMEOUT_MS      = 200;  // > NEC's ~108 ms repeat gap

// ── millis() clock (identical technique to examples/timer/02_software_millis_clock) ──

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

enum HeldButton : uint8_t { NONE, VOL_UP, VOL_DOWN };

static int16_t clampPct(int16_t v) {
    if (v > 100) return 100;
    if (v < 0)   return 0;
    return v;
}

int main() {
    USART0.begin(9600);
    USART0.writeLine_P(PSTR("IRRemote PWM brightness ramp"));
    USART0.writeLine_P(PSTR("============================="));
    USART0.writeLine_P(PSTR("Hold VOL+/VOL- to ramp the LED brighter/dimmer."));
    USART0.writeLine_P(PSTR(""));

    IRRemote::begin();

    PWM1.begin(2000);   // 2 kHz — well above visible flicker for an LED

    Timer0.mode(TimerMode::Normal);
    Timer0.prescaler(TimerPrescaler::DIV64);
    Timer0.start();
    Timer0.enableOverflow();
    sei();

    int16_t     brightness   = 0;
    HeldButton  held         = NONE;
    uint32_t    lastFrameAt  = millis();

    PWM1.dutyA(static_cast<uint8_t>(brightness));

    while (true) {
        uint32_t now = millis();

        if (IRRemote::available()) {
            NECCode c = IRRemote::read();

            if (c.valid && c.address == DEVICE_ADDRESS) {
                bool isVolUp   = (c.command == CMD_VOL_UP);
                bool isVolDown = (c.command == CMD_VOL_DOWN);

                if (!c.repeat) {
                    // Brand new press: always accept, replacing whatever
                    // was held before (e.g. switching directly from
                    // VOL+ to VOL- without an intervening release).
                    held = isVolUp ? VOL_UP : isVolDown ? VOL_DOWN : NONE;
                }
                // A repeat only continues ramping if it matches the
                // button this project already believes is held — a
                // repeat frame for a DIFFERENT command never happens on
                // real NEC hardware, but this guard costs nothing.
                bool accept = (!c.repeat && held != NONE) ||
                              (c.repeat && ((held == VOL_UP && isVolUp) ||
                                            (held == VOL_DOWN && isVolDown)));

                if (accept) {
                    brightness = clampPct(static_cast<int16_t>(
                        brightness + (held == VOL_UP ? BRIGHTNESS_STEP : -BRIGHTNESS_STEP)));
                    PWM1.dutyA(static_cast<uint8_t>(brightness));
                    lastFrameAt = now;

                    USART0.write_P(PSTR("brightness="));
                    USART0.writeInt(brightness);
                    USART0.writeLine_P(PSTR("%"));
                }
            }
        }

        // ---- Release detection: no frame for RELEASE_TIMEOUT_MS -> released ----
        if (held != NONE && (now - lastFrameAt) > RELEASE_TIMEOUT_MS) {
            USART0.write_P(PSTR("released - settled at "));
            USART0.writeInt(brightness);
            USART0.writeLine_P(PSTR("%"));
            held = NONE;
        }
    }
}
