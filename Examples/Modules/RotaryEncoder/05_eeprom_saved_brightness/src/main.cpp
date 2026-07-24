/*
 * RotaryEncoder-Controlled LED Brightness, Saved to EEPROM — PWM1 +
 * EEPROMDriver — MikroDuino Module SDK
 *
 * Project 5 of 6 in the examples/Modules/RotaryEncoder series. A
 * genuinely useful pattern: an LED dimmer where the knob's position
 * drives hardware PWM directly, and a click on the encoder's own
 * button commits the current brightness to EEPROM so it survives a
 * power cycle — the encoder does NOT reset to a default brightness
 * every time the board resets, the way a plain potentiometer would
 * appear to (a pot's physical position IS its value; an incremental
 * encoder has no physical "position" at all until firmware gives it
 * one, which is exactly what this project restores on boot).
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ CH_A    │ PD2   │ Encoder channel A, internal pull-up        │
 *   │ CH_B    │ PD3   │ Encoder channel B, internal pull-up        │
 *   │ SW      │ PD4   │ Encoder's integrated push button            │
 *   │ LED     │ PB1   │ PWM1 channel A (OC1A) — brightness output   │
 *   │ COM     │ —     │ Encoder common pin -> GND                   │
 *   │ TXD     │ PD1   │ USB-serial adapter, 9600 8N1                │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * Gestures:
 *   rotate      -> adjust brightness in +/-5% steps, clamped 0-100%,
 *                  applied to the LED immediately (not saved yet)
 *   short click -> commit the CURRENT brightness to EEPROM
 *   hold >= 1s  -> restore the factory-default 50% brightness, apply
 *                  it, and save that too
 *
 * RotaryEncoder concepts reused from projects 2-3:
 *   - direction() for the +/-5% step-per-detent brightness control
 *     (project 2).
 *   - The integrated button — updateButton(), buttonPressed(),
 *     buttonIsDown(), buttonHeldMs() (project 3) — for the save and
 *     factory-reset gestures.
 *
 * New concepts:
 *   - PWM1Driver (mikroduino/pwm.hpp) — begin(frequencyHz) configures
 *     Timer1 for hardware PWM; dutyA(percent) sets OC1A's (PB1) duty
 *     cycle directly in 0-100%, no manual OCR1A math required.
 *   - EEPROMDriver (mikroduino/eeprom.hpp) with the same 0xA5-magic-
 *     byte "first boot vs. real data" pattern used by
 *     examples/Modules/LCD/06 and examples/Modules/Button/06: on a
 *     truly blank chip, EEPROM reads back as 0xFF, which the magic
 *     byte check catches so the LED starts at a sane default instead
 *     of an sensible-looking-but-uninitialised value.
 */

#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/usart.hpp>
#include <mikroduino/timer.hpp>
#include <mikroduino/pwm.hpp>
#include <mikroduino/eeprom.hpp>
#include <mikroduino/registers.hpp>
#include <RotaryEncoder.hpp>

using namespace MikroDuino;

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

// ── Persisted brightness ─────────────────────────────────────────────────

struct BrightnessSettings {
    uint8_t magic;
    uint8_t percent;   // 0-100
};

static constexpr uint16_t EEPROM_ADDR    = 0x000;
static constexpr uint8_t  MAGIC          = 0xA5;
static constexpr uint8_t  DEFAULT_PCT    = 50;
static constexpr uint16_t RESET_HOLD_MS  = 1000;

static void applyBrightness(uint8_t percent) {
    PWM1.dutyA(percent);
}

static void saveBrightness(uint8_t percent) {
    BrightnessSettings s{ MAGIC, percent };
    EEPROM.update(EEPROM_ADDR, s);   // skips the write if unchanged
}

static uint8_t loadOrInitBrightness() {
    BrightnessSettings s = EEPROM.get<BrightnessSettings>(EEPROM_ADDR);
    if (s.magic != MAGIC) {
        s.magic   = MAGIC;
        s.percent = DEFAULT_PCT;
        saveBrightness(s.percent);
    }
    return s.percent;
}

int main() {
    USART0.begin(9600);
    USART0.writeLine_P(PSTR("RotaryEncoder EEPROM-saved LED brightness"));
    USART0.writeLine_P(PSTR("==========================================="));
    USART0.writeLine_P(PSTR("Rotate: adjust.  Click: save.  Hold 1s: factory reset."));
    USART0.writeLine_P(PSTR(""));

    encoder.begin();
    PWM1.begin(1000);   // 1 kHz — well above flicker threshold for an LED

    Timer0.mode(TimerMode::Normal);
    Timer0.prescaler(TimerPrescaler::DIV64);
    Timer0.start();
    Timer0.enableOverflow();
    sei();

    uint8_t brightness = loadOrInitBrightness();
    applyBrightness(brightness);
    USART0.write_P(PSTR("Loaded brightness: "));
    USART0.writeInt(brightness);
    USART0.writeLine_P(PSTR("%"));

    uint32_t lastUpdateMs = millis();
    bool     resetArmed   = false;

    while (true) {
        uint32_t now = millis();

        while (now - lastUpdateMs >= 1) {
            encoder.updateButton();
            ++lastUpdateMs;
        }

        // ---- Rotation: live-adjust, not yet saved ----
        int8_t step = encoder.direction();
        if (step != 0) {
            int16_t next = static_cast<int16_t>(brightness) + (step * 5);
            if (next < 0)   next = 0;
            if (next > 100) next = 100;
            brightness = static_cast<uint8_t>(next);
            applyBrightness(brightness);

            USART0.write_P(PSTR("brightness="));
            USART0.writeInt(brightness);
            USART0.writeLine_P(PSTR("%"));
        }

        // ---- Short click: commit current brightness to EEPROM ----
        encoder.buttonPressed();
        if (encoder.buttonReleased() && !resetArmed) {
            saveBrightness(brightness);
            USART0.writeLine_P(PSTR("Saved."));
        }

        // ---- Hold >= 1s: factory reset ----
        if (encoder.buttonIsDown()) {
            if (!resetArmed && encoder.buttonHeldMs() >= RESET_HOLD_MS) {
                resetArmed = true;
                brightness = DEFAULT_PCT;
                applyBrightness(brightness);
                saveBrightness(brightness);
                USART0.writeLine_P(PSTR("Factory reset -> 50%, saved."));
            }
        } else {
            resetArmed = false;
        }
    }
}
