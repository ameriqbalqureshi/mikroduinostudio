/*
 * ADC Example 2 (Beginner+) — Battery Voltage Monitor — MikroDuino SDK
 *
 * Builds on Example 1 (adc_light_meter) by turning a raw ADC count into a
 * real physical unit (millivolts) and using it to drive a small alarm state
 * machine with hysteresis. This is the pattern used in almost every
 * battery-powered gadget: a voltage divider brings a pack voltage that
 * exceeds VCC down into the ADC's measurable range.
 *
 * Real-world application: a low-battery indicator/alarm for a battery-powered
 * project (e.g. a 2S Li-ion pack, or any battery whose voltage is above the
 * MCU's 5 V supply). Three LEDs show OK / LOW / CRITICAL state, and a buzzer
 * or LED blinks faster as the battery gets weaker.
 *
 * Hardware (ATmega328P / Arduino Nano @ 16 MHz):
 *
 *   ┌───────────────┬───────┬──────────────────────────────────────────────┐
 *   │ Signal        │ Pin   │ Wiring                                       │
 *   ├───────────────┼───────┼──────────────────────────────────────────────┤
 *   │ Battery sense │ A0    │ Voltage divider from battery+ to GND:        │
 *   │               │       │   BATT+ -- R1(100k) --+-- R2(47k) -- GND     │
 *   │               │       │                        |                    │
 *   │               │       │                        A0                   │
 *   │               │       │   Scales an 8.4V (2S Li-ion) max pack down   │
 *   │               │       │   to ~2.68V, safely inside the 0-5V ADC range│
 *   │ LED_OK        │ PD6   │ Green LED — battery healthy                  │
 *   │ LED_LOW       │ PD7   │ Yellow LED — battery low, recharge soon      │
 *   │ LED_CRITICAL  │ PB0   │ Red LED — critical, blinks                   │
 *   └───────────────┴───────┴──────────────────────────────────────────────┘
 *
 * ADC features used in this example (same two calls as Example 1, plus the
 * math to turn a raw count into a meaningful voltage):
 *   ADC_Driver.begin(ref, prescaler, leftAdjust)
 *   ADC_Driver.read(channel)
 *
 * NOTE on the voltage divider math: with R1=100k (top) and R2=47k (bottom),
 * the divider ratio is R2/(R1+R2) = 47/147 ~= 0.3197. A raw ADC count is
 * converted to divider-node millivolts via (raw * 5000 / 1023), then scaled
 * back up to actual battery millivolts by dividing by the ratio (i.e.
 * multiplying by 147/47). Adjust R1/R2 to match your own battery's max
 * voltage — the divider output at full charge must stay below 5 V.
 */

#include <avr/io.h>
#include <util/delay.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/adc.hpp>

using namespace MikroDuino;

// ── pin assignments ────────────────────────────────────────────────────────
static constexpr uint8_t CH_BATTERY    = 0;     // A0 — divider output
static constexpr uint8_t LED_OK        = PD6;
static constexpr uint8_t LED_LOW       = PD7;
static constexpr uint8_t LED_CRITICAL  = PB0;

// ── voltage divider ratio: R2 / (R1 + R2), as parts-per-thousand ──────────
// R1 = 100k (battery side), R2 = 47k (GND side) -> ratio = 47/147
static constexpr uint32_t DIVIDER_R1 = 100u;
static constexpr uint32_t DIVIDER_R2 = 47u;

// ── alarm thresholds, in real battery millivolts (2S Li-ion example) ──────
// Hysteresis gap between *_ENTER and *_EXIT prevents the LEDs from
// "chattering" when the voltage sits right at a threshold under load noise.
static constexpr uint16_t LOW_ENTER_MV      = 7000;  // drop below this -> LOW
static constexpr uint16_t LOW_EXIT_MV       = 7200;  // rise above this -> back to OK
static constexpr uint16_t CRITICAL_ENTER_MV = 6400;  // drop below this -> CRITICAL
static constexpr uint16_t CRITICAL_EXIT_MV  = 6700;  // rise above this -> back to LOW

enum class BattState : uint8_t { OK, LOW, CRITICAL };

// Convert a raw 10-bit ADC count at the divider node into real battery mV.
static uint16_t raw_to_battery_mv(uint16_t raw) {
    // Step 1: raw -> divider-node millivolts (assumes 5000 mV = AVCC reference)
    uint32_t node_mv = (static_cast<uint32_t>(raw) * 5000u) / 1023u;
    // Step 2: undo the divider ratio to recover the original battery voltage
    uint32_t batt_mv = (node_mv * (DIVIDER_R1 + DIVIDER_R2)) / DIVIDER_R2;
    return static_cast<uint16_t>(batt_mv);
}

static void set_state_leds(BattState s) {
    GPIO::write(LED_OK,       s == BattState::OK);
    GPIO::write(LED_LOW,      s == BattState::LOW);
    GPIO::write(LED_CRITICAL, s == BattState::CRITICAL);
}

int main() {
    GPIO::output(LED_OK);
    GPIO::output(LED_LOW);
    GPIO::output(LED_CRITICAL);

    ADC_Driver.begin(ADCRef::AVCC, ADCPrescaler::DIV128, false);

    BattState state = BattState::OK;
    uint16_t  blink_counter = 0;

    while (true) {
        uint16_t raw     = ADC_Driver.read(CH_BATTERY);
        uint16_t batt_mv = raw_to_battery_mv(raw);

        // State machine with hysteresis: the ENTER/EXIT thresholds differ so
        // small voltage ripple near a boundary doesn't flicker the LEDs.
        switch (state) {
            case BattState::OK:
                if (batt_mv < LOW_ENTER_MV) state = BattState::LOW;
                break;
            case BattState::LOW:
                if (batt_mv < CRITICAL_ENTER_MV)      state = BattState::CRITICAL;
                else if (batt_mv > LOW_EXIT_MV)        state = BattState::OK;
                break;
            case BattState::CRITICAL:
                if (batt_mv > CRITICAL_EXIT_MV) state = BattState::LOW;
                break;
        }

        if (state == BattState::CRITICAL) {
            // Blink the critical LED instead of a steady light — much harder
            // to miss on a device left unattended.
            ++blink_counter;
            GPIO::write(LED_CRITICAL, (blink_counter / 5) % 2 == 0);
            GPIO::write(LED_OK, false);
            GPIO::write(LED_LOW, false);
        } else {
            blink_counter = 0;
            set_state_leds(state);
        }

        _delay_ms(20);   // ~50 samples/sec; battery voltage changes slowly
    }
}
