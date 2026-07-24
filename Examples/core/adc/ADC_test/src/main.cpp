/*
 * ADC Test — MikroDuino SDK
 *
 * Exercises every ADCDriver method with two analog sensors and 8 LEDs.
 * Each demo phase runs for a fixed window then advances automatically.
 *
 * Hardware (ATmega328P TQFP / Arduino Nano @ 16 MHz):
 *
 *   ┌──────────────┬───────┬───────────────────────────────────────────────────┐
 *   │ Signal       │ Pin   │ Wiring                                            │
 *   ├──────────────┼───────┼───────────────────────────────────────────────────┤
 *   │ LED0         │ PD6   │ LED + 220 Ω to GND (active-high)                 │
 *   │ LED1         │ PD7   │ LED + 220 Ω to GND (active-high)                 │
 *   │ LED2         │ PB0   │ LED + 220 Ω to GND (active-high)                 │
 *   │ LED3         │ PB1   │ LED + 220 Ω to GND (active-high)                 │
 *   │ LED4         │ PB2   │ LED + 220 Ω to GND (active-high)                 │
 *   │ LED5         │ PB3   │ LED + 220 Ω to GND (active-high)                 │
 *   │ LED6         │ PB4   │ LED + 220 Ω to GND (active-high)                 │
 *   │ LED7         │ PB5   │ LED + 220 Ω to GND (active-high)                 │
 *   │ BTN0         │ PD2   │ Button to GND, 10 kΩ external pull-up            │
 *   │ BTN1         │ PD3   │ Button to GND, 10 kΩ external pull-up            │
 *   │ BTN2         │ PD4   │ Button to GND, 10 kΩ external pull-up            │
 *   │ BTN3         │ PD5   │ Button to GND, 10 kΩ external pull-up            │
 *   │ Potentiometer│ A7    │ Wiper to ADC7; ends to GND and VCC               │
 *   │ Light sensor │ A6    │ Output to ADC6 (e.g. LDR + voltage divider)      │
 *   └──────────────┴───────┴───────────────────────────────────────────────────┘
 *
 * NOTE: A6 (ADC6) and A7 (ADC7) are analog-only pins — present on ATmega328P
 * TQFP/QFN packages (Arduino Nano, Pro Mini) but NOT on the PDIP-28 package.
 * No DDR/PORT configuration is needed or possible for these pins.
 *
 * LED pattern encoding used by set_leds():
 *   bit 0 → LED0 (PD6)   bit 4 → LED4 (PB2)
 *   bit 1 → LED1 (PD7)   bit 5 → LED5 (PB3)
 *   bit 2 → LED2 (PB0)   bit 6 → LED6 (PB4)
 *   bit 3 → LED3 (PB1)   bit 7 → LED7 (PB5)
 *
 * Demo sequence (loops forever):
 *   Phase 1 — begin() + read()           potentiometer → 8-LED bar graph      3 s
 *   Phase 2 — read() dual channel        button-selected channel display       5 s
 *   Phase 3 — end() / begin(leftAdjust)  read8() → raw binary on 8 LEDs       3 s
 *   Phase 4 — setReference()             AVCC vs internal 1.1 V comparison     5 s
 *   Phase 5 — beginFreeRunning()         conversionComplete()/clearFlag() poll 4 s
 *   Phase 6 — enableInterrupt() + ISR    light sensor, interrupt-driven        4 s
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/adc.hpp>

using namespace MikroDuino;

// ── port indices ───────────────────────────────────────────────────────────
static constexpr uint8_t PORT_B = 1;
static constexpr uint8_t PORT_D = 3;

// ── LED pins ───────────────────────────────────────────────────────────────
static constexpr uint8_t LED[8] = { PD6, PD7, PB0, PB1, PB2, PB3, PB4, PB5 };

// ── Button pins (external pull-up → idle HIGH, press LOW) ─────────────────
static constexpr uint8_t BTN[4] = { PD2, PD3, PD4, PD5 };

// ── ADC channels ───────────────────────────────────────────────────────────
static constexpr uint8_t CH_POT   = 7;   // A7 — potentiometer
static constexpr uint8_t CH_LIGHT = 6;   // A6 — light sensor

// ── port masks ─────────────────────────────────────────────────────────────
static constexpr uint8_t LED_B_MASK = 0x3F;  // PB0–PB5
static constexpr uint8_t LED_D_MASK = 0xC0;  // PD6, PD7

// ── ISR shared state (written by ADC ISR, read by main) ───────────────────
static volatile uint16_t g_isr_result = 0;
static volatile bool     g_isr_ready  = false;

ISR(ADC_vect) {
    g_isr_result = ADC;   // reading ADC register also clears the ADIF flag
    g_isr_ready  = true;
}

// ── utilities ──────────────────────────────────────────────────────────────

static void delay_ms(uint16_t ms) {
    while (ms--) _delay_ms(1);
}

// Write an 8-bit pattern across the two LED ports.
//   bits 0–1 → PD6–PD7    bits 2–7 → PB0–PB5
static void set_leds(uint8_t pattern) {
    GPIO::portWrite(PORT_D, static_cast<uint8_t>((pattern & 0x03u) << 6));
    GPIO::portWrite(PORT_B, static_cast<uint8_t>((pattern >> 2) & 0x3Fu));
}

static void all_leds_off() { set_leds(0x00); }

// Map a 10-bit ADC value (0–1023) to an 8-LED bar-graph pattern.
// Each segment represents 128 counts; full-scale lights all 8 LEDs.
static uint8_t bar8(uint16_t v) {
    uint8_t n = static_cast<uint8_t>((static_cast<uint32_t>(v) * 8u + 512u) / 1024u);
    if (n > 8) n = 8;
    return (n >= 8) ? 0xFFu : static_cast<uint8_t>((1u << n) - 1u);
}

// Map a 10-bit value to a 4-LED bar-graph (for split display).
static uint8_t bar4(uint16_t v) {
    uint8_t n = static_cast<uint8_t>((static_cast<uint32_t>(v) * 4u + 512u) / 1024u);
    if (n > 4) n = 4;
    return (n >= 4) ? 0x0Fu : static_cast<uint8_t>((1u << n) - 1u);
}

static void pause_between_phases() {
    all_leds_off();
    delay_ms(500);
}

// ── Phase 1: begin() + read() — blocking 10-bit bar graph ─────────────────
//
// ADCDriver::begin() configures ADMUX and ADCSRA:
//   ref        = AVCC — uses VCC as the 5 V full-scale reference
//   prescaler  = DIV128 — ADC clock = 16 MHz / 128 = 125 kHz (within 50–200 kHz for 10-bit)
//   leftAdjust = false — result in ADCL:ADCH (right-aligned, standard 10-bit)
//
// ADCDriver::read(ch) — selects channel, sets ADSC, blocks until ADSC clears,
// returns the 16-bit ADC register (only bits 9:0 are valid).
//
// Rotate the potentiometer to sweep the bar graph from 0 to 8 LEDs.
//
static void demo_basic_read() {
    ADC_Driver.begin(ADCRef::AVCC, ADCPrescaler::DIV128, false);

    for (uint16_t t = 0; t < 3000; ++t) {
        uint16_t raw = ADC_Driver.read(CH_POT);
        set_leds(bar8(raw));
        _delay_ms(1);
    }
}

// ── Phase 2: read() — button-controlled dual-channel display ───────────────
//
// Demonstrates runtime channel switching by calling read() with different
// channel arguments based on which buttons are held.
//
//   BTN0 alone  → potentiometer  (A7) full 8-LED bar
//   BTN1 alone  → light sensor   (A6) full 8-LED bar
//   BTN0 + BTN1 → average of both channels, full 8-LED bar
//   Neither     → split display: lower 4 LEDs = pot, upper 4 LEDs = light
//
// The split display makes it easy to compare both sensors simultaneously.
//
static void demo_channel_select() {
    for (uint16_t t = 0; t < 5000; ++t) {
        bool b0 = !GPIO::read(BTN[0]);
        bool b1 = !GPIO::read(BTN[1]);

        if (b0 && !b1) {
            set_leds(bar8(ADC_Driver.read(CH_POT)));

        } else if (b1 && !b0) {
            set_leds(bar8(ADC_Driver.read(CH_LIGHT)));

        } else if (b0 && b1) {
            uint16_t avg = static_cast<uint16_t>(
                (ADC_Driver.read(CH_POT) + ADC_Driver.read(CH_LIGHT)) / 2u);
            set_leds(bar8(avg));

        } else {
            // Split: lower nibble of set_leds() pattern = pot bar (LED0–LED3)
            //        upper nibble                        = light bar (LED4–LED7)
            uint8_t pot_seg   = bar4(ADC_Driver.read(CH_POT));
            uint8_t light_seg = static_cast<uint8_t>(bar4(ADC_Driver.read(CH_LIGHT)) << 4);
            set_leds(pot_seg | light_seg);
        }

        _delay_ms(1);
    }
}

// ── Phase 3: end() + begin(leftAdjust=true) + read8() ────────────────────
//
// ADCDriver::end() — clears ADEN, powering down the ADC.
//
// Restarting with leftAdjust=true sets ADLAR in ADMUX: the 10-bit result is
// left-shifted so ADCH holds the upper 8 bits. read8() returns ADCH only,
// giving a fast 8-bit result without reading ADCL.
//
// Using DIV64 here (250 kHz ADC clock) — acceptable for 8-bit accuracy,
// and slightly faster conversions than DIV128.
//
// The raw byte is displayed directly on the 8 LEDs in binary: a perfect
// binary counter effect as you rotate the potentiometer.
//
static void demo_read8() {
    ADC_Driver.end();
    ADC_Driver.begin(ADCRef::AVCC, ADCPrescaler::DIV64, true);   // left-adjust ON

    for (uint16_t t = 0; t < 3000; ++t) {
        uint8_t raw8 = ADC_Driver.read8(CH_POT);   // returns ADCH (upper 8 bits)
        set_leds(raw8);                              // raw binary pattern on all 8 LEDs
        _delay_ms(1);
    }

    // Restore 10-bit right-aligned mode for remaining phases
    ADC_Driver.end();
    ADC_Driver.begin(ADCRef::AVCC, ADCPrescaler::DIV128, false);
}

// ── Phase 4: setReference() — AVCC vs Internal 1.1 V ─────────────────────
//
// ADCDriver::setReference() updates only the REFS1:REFS0 bits in ADMUX,
// leaving channel and left-adjust settings untouched.
//
// With AVCC (5 V) reference: full pot sweep = 0–1023 → 0–8 LEDs.
// With Internal 1.1 V reference: a 1.1 V signal already drives ADC to 1023.
// Any voltage above ~1.1 V saturates the ADC at 1023 (all 8 LEDs on).
//
// To see the reference change clearly:
//   - Set pot to ~25% (roughly 1.25 V): shows ~2 LEDs with AVCC
//   - After switching to 1.1 V: the same voltage saturates → all LEDs on
//
static void demo_set_reference() {
    // First half: AVCC reference — normal full-range bar graph
    for (uint16_t t = 0; t < 2500; ++t) {
        set_leds(bar8(ADC_Driver.read(CH_POT)));
        _delay_ms(1);
    }

    // Flash twice to signal reference switch
    all_leds_off(); delay_ms(150);
    set_leds(0xFF); delay_ms(150);
    all_leds_off(); delay_ms(150);
    set_leds(0xFF); delay_ms(150);
    all_leds_off(); delay_ms(50);

    // Switch to internal 1.1 V reference — allow a few ms to settle
    ADC_Driver.setReference(ADCRef::Internal);
    delay_ms(20);

    // Second half: internal 1.1 V — ADC saturates above ~1.1 V input
    for (uint16_t t = 0; t < 2500; ++t) {
        set_leds(bar8(ADC_Driver.read(CH_POT)));
        _delay_ms(1);
    }

    // Restore AVCC for subsequent phases
    ADC_Driver.setReference(ADCRef::AVCC);
    delay_ms(10);
}

// ── Phase 5: beginFreeRunning() + conversionComplete() + clearFlag() ───────
//
// ADCDriver::beginFreeRunning(ch) — enables auto-trigger (ADATE), sets the
//   auto-trigger source to free-running (ADCSRB = 0), then starts the first
//   conversion with ADSC. Conversions restart automatically after each one.
//
// ADCDriver::conversionComplete() — checks the ADIF flag (set by hardware
//   at the end of each conversion). Allows non-blocking polling: the CPU can
//   do other work and only process the result when it is fresh.
//
// ADCDriver::resultFreeRunning() — reads the ADC register without starting
//   a new conversion (unlike read() which starts and blocks).
//
// ADCDriver::clearFlag() — writes 1 to ADIF to acknowledge the conversion.
//   Without clearing, conversionComplete() will appear to stay true.
//
// Effect: bar graph updates ~125 times/second driven by the ADC itself;
// in between updates the CPU is free (here just burning _delay_ms(1)).
//
static void demo_free_running() {
    ADC_Driver.beginFreeRunning(CH_POT);

    for (uint16_t t = 0; t < 4000; ++t) {
        if (ADC_Driver.conversionComplete()) {
            uint16_t raw = ADC_Driver.resultFreeRunning();
            ADC_Driver.clearFlag();
            set_leds(bar8(raw));
        }
        // CPU is free here — in a real application this would do other work
        _delay_ms(1);
    }

    ADC_Driver.stopFreeRunning();
}

// ── Phase 6: enableInterrupt() + ISR + disableInterrupt() ─────────────────
//
// ADCDriver::enableInterrupt() — sets ADIE in ADCSRA; the ADC conversion
//   complete interrupt fires after each conversion (requires I-bit in SREG).
// ADCDriver::disableInterrupt() — clears ADIE.
//
// ISR(ADC_vect) at the top of this file reads the ADC register (which also
// clears ADIF), sets g_isr_ready, and stores the result in g_isr_result.
//
// The main loop simply checks g_isr_ready and updates the LED bar without
// any blocking wait — the ADC drives itself via free-running + interrupt.
//
// Light sensor is used here so you can observe the bar graph react to
// covering/uncovering the sensor while the potentiometer remains unchanged.
//
static void demo_interrupt_driven() {
    g_isr_result = 0;
    g_isr_ready  = false;

    ADC_Driver.enableInterrupt();
    ADC_Driver.beginFreeRunning(CH_LIGHT);
    sei();                          // enable global interrupts (I-bit in SREG)

    for (uint16_t t = 0; t < 4000; ++t) {
        if (g_isr_ready) {
            uint16_t snap = g_isr_result;   // snapshot before re-enabling updates
            g_isr_ready = false;
            set_leds(bar8(snap));
        }
        _delay_ms(1);
    }

    cli();                          // disable global interrupts before cleanup
    ADC_Driver.stopFreeRunning();
    ADC_Driver.disableInterrupt();
}

// ── main ───────────────────────────────────────────────────────────────────

int main() {
    // Configure LED output pins
    GPIO::output(LED[0]);                      // PD6
    GPIO::output(LED[1]);                      // PD7
    GPIO::portOutput(PORT_B, LED_B_MASK);      // PB0–PB5

    // Configure button input pins (external pull-ups; no internal pull-up needed)
    for (uint8_t i = 0; i < 4; ++i) GPIO::input(BTN[i]);

    all_leds_off();

    // A6 and A7 are analog-only pins — no GPIO init required or possible.

    while (true) {
        demo_basic_read();          // begin() + read()  — 10-bit blocking
        pause_between_phases();

        demo_channel_select();      // read() with runtime channel switching
        pause_between_phases();

        demo_read8();               // end() / begin(leftAdjust=true) / read8()
        pause_between_phases();

        demo_set_reference();       // setReference(): AVCC → Internal 1.1 V
        pause_between_phases();

        demo_free_running();        // beginFreeRunning() / resultFreeRunning()
                                    // conversionComplete() / clearFlag()
        pause_between_phases();

        demo_interrupt_driven();    // enableInterrupt() / ISR / disableInterrupt()
        pause_between_phases();
    }
}
