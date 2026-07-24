/*
 * IRSensor Analog Mode — Raw Reflectance + Threshold Presets — MikroDuino
 * Module SDK
 *
 * Project 2 of 6 in the examples/Modules/IRSensor series. Project 1 read a
 * digital comparator board's HIGH/LOW output; this project switches to
 * ANALOG mode on a bare phototransistor reflectance sensor (TCRT5000,
 * QRE1113) whose output voltage — and so the ADC value IRSensor reads —
 * rises smoothly as a surface gets closer / more reflective, instead of
 * snapping between two states. The project cycles through three threshold
 * presets every few seconds against the SAME raw reading, so the effect
 * of setThreshold() on detected() is visible directly rather than just
 * described.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ IR OUT  │ PC0   │ ADC0 — bare phototransistor / photodiode   │
 *   │         │       │ output, NOT a comparator board             │
 *   │ LED     │ PB5   │ Lit while detected() is true                │
 *   │ TXD     │ PD1   │ USB-serial adapter, 9600 8N1, for readings │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * IRSensor concepts introduced:
 *   - ADCDriver — ANALOG mode needs an initialised ADC peripheral; IRSensor
 *     does not own or configure one itself, it only reads from a pointer
 *     you give it. begin() must be called on the ADCDriver before
 *     IRSensor.readRaw()/detected() are used.
 *   - IRSensor(pin, ANALOG, activeLow, threshold, &adc) — pin must be an
 *     ADC-capable pin (PC0-PC5 on ATmega328P). activeLow has no effect in
 *     ANALOG mode; it is ignored and kept only so the constructor's
 *     argument order matches DIGITAL mode.
 *   - readRaw() — in ANALOG mode returns the raw 0-1023 ADC conversion
 *     with no interpretation applied.
 *   - detected() — in ANALOG mode, true when readRaw() > threshold. Same
 *     method name and return type as project 1's digital polling, but the
 *     decision is now a comparison against a tunable number instead of a
 *     fixed comparator circuit.
 *   - setThreshold(t) — changes the ANALOG-mode comparison point at
 *     runtime. This project calls it on a timer to sweep through three
 *     presets (SENSITIVE / NORMAL / CONSERVATIVE) against a single
 *     unchanging raw reading, so a higher threshold visibly needs a
 *     closer / more reflective object before detected() flips true.
 */

#include <util/delay.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/usart.hpp>
#include <mikroduino/adc.hpp>
#include <IRSensor.hpp>

using namespace MikroDuino;

static constexpr uint8_t LED = PB5;

static constexpr uint16_t THRESHOLD_SENSITIVE    = 300;
static constexpr uint16_t THRESHOLD_NORMAL       = 512;
static constexpr uint16_t THRESHOLD_CONSERVATIVE = 750;

ADCDriver adc;

// ANALOG mode: pin must be an ADC channel; activeLow is unused here.
IRSensor ir(PC0, IRSensor::ANALOG, true, THRESHOLD_NORMAL, &adc);

static void printPresetName(uint16_t threshold) {
    if (threshold == THRESHOLD_SENSITIVE)         USART0.write_P(PSTR("SENSITIVE"));
    else if (threshold == THRESHOLD_CONSERVATIVE) USART0.write_P(PSTR("CONSERVATIVE"));
    else                                            USART0.write_P(PSTR("NORMAL"));
}

int main() {
    USART0.begin(9600);
    USART0.writeLine_P(PSTR("IRSensor analog threshold presets"));
    USART0.writeLine_P(PSTR("=================================="));
    USART0.writeLine_P(PSTR("raw / threshold preset / detected, cycling presets every 3 s"));
    USART0.writeLine_P(PSTR(""));

    GPIO::output(LED);
    GPIO::clear(LED);

    adc.begin();
    ir.begin();

    static const uint16_t PRESETS[3] = { THRESHOLD_SENSITIVE, THRESHOLD_NORMAL, THRESHOLD_CONSERVATIVE };
    uint8_t  presetIndex   = 0;
    uint16_t elapsedInStep = 0;   // counts 200 ms ticks

    while (true) {
        uint16_t raw = ir.readRaw();
        bool     isDetected = ir.detected();

        USART0.write_P(PSTR("raw="));
        USART0.writeInt(static_cast<int32_t>(raw));
        USART0.write_P(PSTR("  threshold="));
        printPresetName(PRESETS[presetIndex]);
        USART0.write_P(PSTR(" ("));
        USART0.writeInt(static_cast<int32_t>(PRESETS[presetIndex]));
        USART0.write_P(PSTR(")"));

        if (isDetected) {
            USART0.writeLine_P(PSTR("  [DETECTED]"));
            GPIO::set(LED);
        } else {
            USART0.writeLine_P(PSTR("  [clear]"));
            GPIO::clear(LED);
        }

        _delay_ms(200);
        elapsedInStep++;

        // Every 3 s (15 * 200 ms), rotate to the next threshold preset.
        if (elapsedInStep >= 15) {
            elapsedInStep = 0;
            presetIndex = static_cast<uint8_t>((presetIndex + 1) % 3);
            ir.setThreshold(PRESETS[presetIndex]);
        }
    }
}
