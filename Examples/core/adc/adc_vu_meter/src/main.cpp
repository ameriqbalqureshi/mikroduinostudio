/*
 * ADC Example 4 (Intermediate/Advanced) — Audio VU Meter — MikroDuino SDK
 *
 * Introduces free-running mode: the ADC hardware restarts each conversion
 * automatically the instant the previous one finishes, without any CPU
 * intervention. This is essential for anything that needs a steady, fast
 * sample rate (audio, vibration, fast-changing signals) instead of the
 * on-demand reads used in Examples 1-3.
 *
 * Real-world application: a VU meter / sound-level indicator, as found in
 * audio mixers and "sound reactive" LED projects. An electret microphone
 * module (with its own onboard amplifier, e.g. a MAX4466 or KY-038 board)
 * outputs an analog voltage centered around VCC/2 that swings with the
 * sound pressure. We free-run the ADC as fast as possible, rectify the
 * signal around its center point, and track a fast-attack/slow-decay peak
 * envelope — the same technique real VU meters use.
 *
 * Hardware (ATmega328P / Arduino Nano @ 16 MHz):
 *
 *   ┌────────────────┬───────┬────────────────────────────────────────────┐
 *   │ Signal         │ Pin   │ Wiring                                     │
 *   ├────────────────┼───────┼────────────────────────────────────────────┤
 *   │ Mic amp output │ A0    │ Electret mic module analog OUT (biased     │
 *   │                │       │ around ~VCC/2 by the module's own circuit) │
 *   │ LED0..LED7     │ PB0-7 │ 8-LED bar graph, 220 Ω to GND (VU display) │
 *   └────────────────┴───────┴────────────────────────────────────────────┘
 *
 * ADC features used in this example (new ones vs. Examples 1-3 in bold):
 *   ADC_Driver.begin(ref, prescaler, leftAdjust)
 *   ADC_Driver.beginFreeRunning(channel)  [NEW] — enable ADATE (auto-trigger)
 *                                                 in free-running mode and
 *                                                 kick off the first conversion
 *   ADC_Driver.conversionComplete()       [NEW] — non-blocking poll of ADIF;
 *                                                 lets the CPU do other work
 *                                                 between samples
 *   ADC_Driver.resultFreeRunning()        [NEW] — read the latest ADC result
 *                                                 WITHOUT starting a new
 *                                                 conversion (unlike read())
 *   ADC_Driver.clearFlag()                [NEW] — acknowledge ADIF so
 *                                                 conversionComplete() reports
 *                                                 the NEXT sample, not this one
 *   ADC_Driver.stopFreeRunning()          [NEW] — disable ADATE/ADSC cleanly
 *
 * Using DIV32 instead of DIV128 here deliberately trades some 10-bit
 * accuracy for speed: at 16 MHz, DIV32 gives a 500 kHz ADC clock, so a full
 * 13-cycle conversion takes ~26 us -> nearly 38,000 samples/sec free-running.
 * That's far faster than needed for audible frequencies, giving headroom to
 * catch transient peaks between main-loop passes.
 */

#include <avr/io.h>
#include <util/delay.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/adc.hpp>

using namespace MikroDuino;

static constexpr uint8_t CH_MIC  = 0;   // A0 — electret mic module output
static constexpr uint8_t PORT_B  = 1;   // 8-LED VU bar graph

// The mic module biases its output around mid-supply; with a 10-bit ADC and
// AVCC reference that's close to raw count 512. We measure the AC swing
// around this center rather than the absolute voltage.
static constexpr uint16_t DC_BIAS = 512;

// Peak envelope tuning: attack is instant (peaks jump up immediately), decay
// gently falls back down each sample so the display doesn't flicker but
// still reacts quickly to transients — a classic VU-meter "ballistics" model.
static constexpr uint16_t DECAY_STEP = 3;

static uint8_t bar8(uint16_t v /* 0-511 AC swing magnitude */) {
    uint8_t n = static_cast<uint8_t>((static_cast<uint32_t>(v) * 8u) / 512u);
    if (n > 8) n = 8;
    return (n >= 8) ? 0xFFu : static_cast<uint8_t>((1u << n) - 1u);
}

int main() {
    GPIO::portOutput(PORT_B, 0xFF);

    // begin() first, in the standard 10-bit right-aligned mode, so ADMUX and
    // ADCSRA start from a known state before switching to free-running.
    ADC_Driver.begin(ADCRef::AVCC, ADCPrescaler::DIV32, false);

    // beginFreeRunning() selects CH_MIC, sets ADATE (auto-trigger enable),
    // configures ADCSRB for free-running trigger source, and starts the
    // first conversion. From this point the ADC re-triggers itself forever
    // until stopFreeRunning() is called.
    ADC_Driver.beginFreeRunning(CH_MIC);

    uint16_t peak = 0;

    while (true) {
        // Non-blocking poll: conversionComplete() just checks the ADIF flag.
        // Because the ADC is free-running, a new conversion is *already*
        // under way while we process this one — no time is wasted waiting.
        if (ADC_Driver.conversionComplete()) {
            // resultFreeRunning() just reads the ADC register directly; it
            // does NOT start a new conversion (that already happened
            // automatically), unlike read() in the earlier examples.
            uint16_t raw = ADC_Driver.resultFreeRunning();

            // Must explicitly clear ADIF, or conversionComplete() would keep
            // reporting the same stale sample as "ready" forever.
            ADC_Driver.clearFlag();

            // Rectify around the mic's DC bias to get an instantaneous
            // "loudness" magnitude regardless of whether the waveform is
            // currently above or below center.
            uint16_t mag = (raw > DC_BIAS) ? (raw - DC_BIAS) : (DC_BIAS - raw);

            // Fast attack: any louder sample immediately becomes the new peak.
            if (mag > peak) peak = mag;

            GPIO::portWrite(PORT_B, bar8(peak));
        }

        // Slow decay: every main-loop pass, let the displayed peak fall a
        // little. Combined with the fast attack above, LEDs jump up
        // instantly on a loud sound and settle back down smoothly —
        // exactly how an analog VU meter needle behaves.
        if (peak > DECAY_STEP) peak -= DECAY_STEP;
        else                    peak = 0;

        _delay_ms(2);   // ~500 Hz envelope update rate — smooth to the eye
    }
}
