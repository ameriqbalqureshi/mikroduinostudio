/*
 * ADC Example 5 (Advanced) — Interrupt-Driven Multi-Channel Data Logger
 * MikroDuino SDK
 *
 * Combines free-running conversions (Example 4) with the ADC's own
 * interrupt instead of polling conversionComplete() in the main loop. The
 * ISR just stashes each result; main() is completely free to do other work
 * between samples and only touches the ADC to decide when to switch
 * channels. This is the pattern to reach for once polling in the main loop
 * becomes inconvenient (e.g. the main loop also needs to service USART
 * commands, run a UI, etc).
 *
 * Real-world application: a greenhouse/environmental data logger that
 * round-robins three analog channels, accumulates min/max/average
 * statistics for each over a batch of samples, and streams the results as
 * CSV rows over USART — ready to paste into a spreadsheet or capture with a
 * terminal logging tool for offline analysis.
 *
 * Hardware (ATmega328P TQFP / Arduino Nano @ 16 MHz):
 *
 *   ┌────────────────┬───────┬────────────────────────────────────────────┐
 *   │ Signal         │ Pin   │ Wiring                                     │
 *   ├────────────────┼───────┼────────────────────────────────────────────┤
 *   │ Soil moisture  │ A6    │ Potentiometer wiper (moisture-probe stand-in)│
 *   │ Ambient light  │ A7    │ LDR + 10k voltage divider                  │
 *   │ Supply/battery │ A0    │ Voltage divider (see adc_battery_monitor)  │
 *   │ STATUS_LED     │ PB5   │ Blinks once per completed round of 3 chans │
 *   │ USART0 TX      │ PD1   │ To PC via USB-serial, 9600 8N1             │
 *   └────────────────┴───────┴────────────────────────────────────────────┘
 *
 * ADC features used in this example (new ones vs. Examples 1-4 in bold):
 *   ADC_Driver.begin(ref, prescaler, leftAdjust)
 *   ADC_Driver.beginFreeRunning(channel)  — reused from Example 4, restarted
 *                                           each time we switch channels
 *   ADC_Driver.enableInterrupt()   [NEW]  — sets ADIE so ADC_vect fires after
 *                                           every completed conversion
 *   ADC_Driver.disableInterrupt()  [NEW]  — clears ADIE between channel swaps
 *   ISR(ADC_vect)                  [NEW]  — reading the ADC register inside
 *                                           the ISR both fetches the result
 *                                           AND clears ADIF automatically
 *   ADC_Driver.stopFreeRunning()          — halts conversions while we swap
 *                                           channel and reset statistics
 *
 * Why round-robin instead of one free-running channel per ISR call: the ADC
 * has a single sample-and-hold input, so only one channel can be digitized
 * at a time. Logging N channels "simultaneously" always means time-slicing
 * them — exactly what real multi-channel data loggers do internally.
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/adc.hpp>
#include <mikroduino/usart.hpp>

using namespace MikroDuino;

#define WP(s)   USART0.write_P(PSTR(s))
#define WLP(s)  USART0.writeLine_P(PSTR(s))

static constexpr uint8_t STATUS_LED = PB5;

// ── channel table: round-robin through these in order ─────────────────────
struct ChannelDef {
    uint8_t     channel;
    const char* name;   // PROGMEM label printed in the CSV header
};

static constexpr uint8_t   NUM_CHANNELS = 3;
static constexpr uint8_t   CH_MOISTURE  = 6;
static constexpr uint8_t   CH_LIGHT     = 7;
static constexpr uint8_t   CH_SUPPLY    = 0;

static const uint8_t CHANNEL_LIST[NUM_CHANNELS] = { CH_MOISTURE, CH_LIGHT, CH_SUPPLY };

// How many ISR-delivered samples to accumulate per channel before moving on.
// Larger = smoother average, slower logger cadence.
static constexpr uint16_t SAMPLES_PER_CHANNEL = 200;

// ── ISR shared state ────────────────────────────────────────────────────
// volatile because it is written in ISR(ADC_vect) and read in main().
static volatile uint16_t g_isr_result = 0;
static volatile bool     g_isr_ready  = false;

ISR(ADC_vect) {
    g_isr_result = ADC;   // reading ADC also clears ADIF for us
    g_isr_ready  = true;
}

static void print_csv_header() {
    WLP("");
    WLP("channel,samples,min,max,avg");
}

// Sample one channel SAMPLES_PER_CHANNEL times via the interrupt-driven
// free-running ADC, then print one CSV row with its min/max/average.
static void log_channel(const char* label, uint8_t channel) {
    uint16_t min_v = 1023;
    uint16_t max_v = 0;
    uint32_t sum   = 0;
    uint16_t count = 0;

    g_isr_ready = false;
    ADC_Driver.enableInterrupt();          // ADIE=1 — ADC_vect fires after each conversion
    ADC_Driver.beginFreeRunning(channel);  // selects channel, starts auto-triggered conversions
    sei();

    while (count < SAMPLES_PER_CHANNEL) {
        // The ISR runs in the background; main() just waits for the flag.
        // Because conversions are free-running, the next sample is already
        // being digitized while we process the current one below.
        if (g_isr_ready) {
            uint16_t v = g_isr_result;   // snapshot before clearing the flag
            g_isr_ready = false;

            if (v < min_v) min_v = v;
            if (v > max_v) max_v = v;
            sum += v;
            ++count;
        }
    }

    cli();
    ADC_Driver.stopFreeRunning();
    ADC_Driver.disableInterrupt();

    uint16_t avg = static_cast<uint16_t>(sum / count);

    // CSV row: channel,samples,min,max,avg
    // label points to a plain (SRAM) string literal, not a PSTR() flash one,
    // so use write(const char*) rather than the WP()/write_P() flash macro.
    USART0.write(label);
    WP(",");   USART0.writeInt(static_cast<int32_t>(count));
    WP(",");   USART0.writeInt(static_cast<int32_t>(min_v));
    WP(",");   USART0.writeInt(static_cast<int32_t>(max_v));
    WP(",");   USART0.writeInt(static_cast<int32_t>(avg));
    WLP("");
}

int main() {
    GPIO::output(STATUS_LED);
    USART0.begin(9600);

    WLP("MikroDuino ADC Data Logger — Example 5/6");
    WP("Sampling "); USART0.writeInt(SAMPLES_PER_CHANNEL);
    WLP(" points/channel, round-robin, interrupt-driven.");

    // Standard AVCC/10-bit setup — used as the base configuration before
    // each beginFreeRunning() call re-selects the active channel.
    ADC_Driver.begin(ADCRef::AVCC, ADCPrescaler::DIV128, false);

    static const char* const NAMES[NUM_CHANNELS] = { "moisture", "light", "supply" };

    while (true) {
        print_csv_header();

        for (uint8_t i = 0; i < NUM_CHANNELS; ++i) {
            log_channel(NAMES[i], CHANNEL_LIST[i]);
        }

        // Heartbeat: one blink per completed round over all channels, so you
        // can confirm the logger is alive without watching the serial output.
        GPIO::toggle(STATUS_LED);

        _delay_ms(500);   // pause between rounds
    }
}
