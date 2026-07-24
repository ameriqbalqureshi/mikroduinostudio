/*
 * ADC Example 3 (Intermediate) — Serial Environmental Dashboard — MikroDuino SDK
 *
 * Builds on Examples 1-2 by reading several channels in one pass and
 * demonstrating setReference() (needed for the internal temperature sensor)
 * and read8() (a fast, lower-resolution mode). Results are streamed as a
 * human-readable dashboard over USART instead of just driving LEDs.
 *
 * Real-world application: a compact multi-sensor environmental logger of the
 * kind used in a greenhouse controller or weather station — potentiometer
 * standing in for a soil-moisture probe, an LDR for ambient light, and the
 * ATmega328P's own internal temperature sensor for chip/ambient temperature.
 *
 * Hardware (ATmega328P TQFP / Arduino Nano @ 16 MHz):
 *
 *   ┌────────────────┬───────┬─────────────────────────────────────────────┐
 *   │ Signal         │ Pin   │ Wiring                                      │
 *   ├────────────────┼───────┼─────────────────────────────────────────────┤
 *   │ Moisture (pot) │ A6    │ Potentiometer wiper (stand-in for a soil    │
 *   │                │       │ moisture probe); ends to GND and VCC        │
 *   │ Light (LDR)    │ A7    │ LDR + 10k voltage divider                   │
 *   │ Temperature    │ (A8)  │ Internal ATmega328P sensor, no external pin │
 *   │ USART0 TX      │ PD1   │ To PC via USB-serial, 9600 8N1              │
 *   └────────────────┴───────┴─────────────────────────────────────────────┘
 *
 * ADC features used in this example (new ones vs. Examples 1-2 in bold):
 *   ADC_Driver.begin(ref, prescaler, leftAdjust)
 *   ADC_Driver.read(channel)             — used for the two external sensors
 *   ADC_Driver.setReference(ref)   [NEW] — switches ADMUX's reference bits
 *                                          without touching channel/adjust,
 *                                          needed because the internal temp
 *                                          sensor requires the 1.1V reference
 *   ADC_Driver.read8(channel)      [NEW] — fast 8-bit-only conversion (needs
 *                                          leftAdjust=true); used here to show
 *                                          how much faster a low-res read can
 *                                          be sampled for coarse trend data
 *   ADC_Driver.end()               [NEW] — powers the ADC down between the
 *                                          two reference regimes
 */

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/adc.hpp>
#include <mikroduino/usart.hpp>

using namespace MikroDuino;

#define WP(s)   USART0.write_P(PSTR(s))
#define WLP(s)  USART0.writeLine_P(PSTR(s))

// ── ADC channel assignments ────────────────────────────────────────────────
static constexpr uint8_t CH_MOISTURE = 6;   // A6 — potentiometer (soil-moisture stand-in)
static constexpr uint8_t CH_LIGHT    = 7;   // A7 — LDR divider
static constexpr uint8_t CH_TEMP     = 8;   // internal temperature sensor (328P only)

// Typical ATmega328P internal-sensor formula: ~1 mV/degC, offset ~289 (raw @ 1.1V ref)
static int16_t raw_to_celsius(uint16_t raw) {
    return static_cast<int16_t>(((int32_t)raw * 1100L / 1023L) - 289L);
}

static void print_header() {
    WLP("");
    WLP("===================================================");
    WLP("  MikroDuino Environmental Dashboard  (Example 3/6)");
    WLP("===================================================");
    WLP("moisture% | light% | temp(C) | fastLight8(0-255)");
    WLP("---------------------------------------------------");
}

int main() {
    USART0.begin(9600);
    print_header();

    while (true) {
        // ── 1. Two external sensors on the AVCC (5V) reference ────────────
        // begin() re-establishes AVCC + DIV128 + right-aligned 10-bit mode.
        ADC_Driver.begin(ADCRef::AVCC, ADCPrescaler::DIV128, false);

        uint16_t moisture_raw = ADC_Driver.read(CH_MOISTURE);
        uint16_t light_raw    = ADC_Driver.read(CH_LIGHT);

        uint16_t moisture_pct = static_cast<uint16_t>((static_cast<uint32_t>(moisture_raw) * 100u) / 1023u);
        uint16_t light_pct    = static_cast<uint16_t>((static_cast<uint32_t>(light_raw)    * 100u) / 1023u);

        // ── 2. read8() — a quick low-resolution re-sample of the light  ───
        // channel. Switching to leftAdjust=true makes ADCH alone hold the
        // top 8 bits of the result, so read8() skips reading ADCL entirely.
        // Useful when you only need a coarse trend value and want the
        // fastest possible successive-approximation read.
        ADC_Driver.end();
        ADC_Driver.begin(ADCRef::AVCC, ADCPrescaler::DIV128, true);  // leftAdjust = true
        uint8_t light_fast = ADC_Driver.read8(CH_LIGHT);

        // ── 3. Internal temperature sensor — requires the internal 1.1V ───
        // reference. setReference() swaps ADMUX's REFS bits only, leaving
        // the channel and left-adjust settings from step 2 untouched, so we
        // still need a fresh begin() here to restore right-aligned 10-bit
        // mode for an accurate 10-bit temperature reading.
        ADC_Driver.end();
        ADC_Driver.begin(ADCRef::Internal, ADCPrescaler::DIV128, false);
        _delay_ms(10);                    // bandgap reference settling time
        ADC_Driver.read(CH_TEMP);         // dummy read — let the mux settle
        uint16_t temp_raw = ADC_Driver.read(CH_TEMP);
        int16_t  temp_c   = raw_to_celsius(temp_raw);

        // setReference() demonstrated explicitly: flip back to AVCC for the
        // next loop iteration's external-sensor reads without a full begin().
        ADC_Driver.setReference(ADCRef::AVCC);
        _delay_ms(1);

        // ── Print one dashboard row ─────────────────────────────────────
        WP("   "); USART0.writeInt(static_cast<int32_t>(moisture_pct));
        WP("%    |  ");  USART0.writeInt(static_cast<int32_t>(light_pct));
        WP("%   |   ");  USART0.writeInt(temp_c);
        WP("    |   ");  USART0.writeInt(static_cast<int32_t>(light_fast));
        WLP("");

        _delay_ms(1000);   // once-a-second dashboard row, like a data logger
    }
}
