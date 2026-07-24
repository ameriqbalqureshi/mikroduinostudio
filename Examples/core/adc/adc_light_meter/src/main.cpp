/*
 * ADC Example 1 (Beginner) — Analog Light Meter — MikroDuino SDK
 *
 * The simplest possible use of the ADC driver: begin() once, then read()
 * in a loop. This is the "blink" of analog input — start here before
 * moving on to the other ADC examples in this series.
 *
 * Real-world application: a light meter / photocell brightness indicator,
 * the kind of circuit used in automatic desk lamps, streetlight controllers,
 * or camera exposure meters. An LDR (light-dependent resistor) forms a
 * voltage divider with a fixed resistor; brighter light lowers the LDR's
 * resistance, which raises the voltage seen by the ADC.
 *
 * Hardware (ATmega328P / Arduino Nano @ 16 MHz):
 *
 *   ┌──────────────┬───────┬───────────────────────────────────────────────┐
 *   │ Signal       │ Pin   │ Wiring                                        │
 *   ├──────────────┼───────┼───────────────────────────────────────────────┤
 *   │ Light sensor │ A0    │ LDR + 10 kΩ fixed resistor voltage divider:   │
 *   │              │       │   VCC -> LDR -> A0 -> 10k -> GND              │
 *   │              │       │   (brighter light -> higher voltage at A0)   │
 *   │ LED0..LED7   │ PB0-7 │ 8 LEDs + 220 Ω to GND (bar graph, active-hi) │
 *   │ DARK_LED     │ PD6   │ LED + 220 Ω to GND — lights when it's dark   │
 *   └──────────────┴───────┴───────────────────────────────────────────────┘
 *
 * ADC features used in this example:
 *   ADC_Driver.begin(ref, prescaler, leftAdjust)  — one-time setup
 *   ADC_Driver.read(channel)                      — blocking 10-bit read
 *
 * That's it — just two calls. Later examples in this series build on this
 * with more channels, interrupts, free-running mode, and reference switching.
 */

#include <avr/io.h>
#include <util/delay.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/adc.hpp>

using namespace MikroDuino;

// ── pin assignments ────────────────────────────────────────────────────────
static constexpr uint8_t CH_LIGHT  = 0;     // A0 — LDR voltage divider
static constexpr uint8_t PORT_B    = 1;     // LED bar graph port
static constexpr uint8_t DARK_LED  = PD6;   // single "it's dark" indicator

// Below this raw ADC value we consider the room "dark" and light the alarm
// LED. Tune this threshold to your LDR/resistor pair and room lighting.
static constexpr uint16_t DARK_THRESHOLD = 300;

// Map a 10-bit ADC value (0-1023) onto an 8-LED bar graph.
static uint8_t bar8(uint16_t v) {
    uint8_t n = static_cast<uint8_t>((static_cast<uint32_t>(v) * 8u + 512u) / 1024u);
    if (n > 8) n = 8;
    return (n >= 8) ? 0xFFu : static_cast<uint8_t>((1u << n) - 1u);
}

int main() {
    // Configure the 8-LED bar graph (PB0-PB7) as outputs.
    GPIO::portOutput(PORT_B, 0xFF);
    GPIO::output(DARK_LED);

    // One-time ADC setup:
    //   ADCRef::AVCC       — use the 5 V supply rail as the reference voltage
    //   ADCPrescaler::DIV128 — 16 MHz / 128 = 125 kHz ADC clock (in the
    //                          recommended 50-200 kHz range for full 10-bit accuracy)
    //   leftAdjust = false — standard right-aligned 10-bit result
    ADC_Driver.begin(ADCRef::AVCC, ADCPrescaler::DIV128, false);

    while (true) {
        // read() selects the channel, starts a conversion, blocks until the
        // hardware clears ADSC, and returns the 10-bit result (0-1023).
        uint16_t light = ADC_Driver.read(CH_LIGHT);

        // Show the light level as a bar graph — more LEDs lit = brighter.
        GPIO::portWrite(PORT_B, bar8(light));

        // Simple threshold alarm: a common building block in real designs
        // (e.g. "turn the porch light on at dusk").
        GPIO::write(DARK_LED, light < DARK_THRESHOLD);

        _delay_ms(50);   // ~20 samples/sec is plenty for a light meter
    }
}
