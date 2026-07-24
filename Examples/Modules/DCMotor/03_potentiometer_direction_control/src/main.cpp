/*
 * DCMotor + ADC + Button — Potentiometer Throttle, Button Direction/E-Stop —
 * MikroDuino Module SDK
 *
 * Project 3 of 6 in the examples/Modules/DCMotor series. Replaces the
 * fixed, pre-programmed speed ramps of projects 1-2 with live analog
 * control: a potentiometer sets throttle in real time, and a single
 * button both flips direction (clicked()) and latches an emergency stop
 * (longPressed()) — the same two events examples/Modules/Button/01 and
 * /03 introduced, reused here for motor control instead of an LED.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ IN1     │ PD4   │ H-bridge direction input 1                │
 *   │ IN2     │ PD5   │ H-bridge direction input 2                │
 *   │ ENA     │ PD6   │ OC0A hardware PWM — speed control          │
 *   │ Pot     │ PC0   │ ADC0 — wiper; outer legs to 5V/GND        │
 *   │ Button  │ PD2   │ Other leg to GND — click = flip direction,│
 *   │         │       │ long-press = toggle emergency-stop latch   │
 *   │ TXD     │ PD1   │ USB-serial adapter, 9600 8N1               │
 *   │ Motor   │ OUT1/2│ DC motor terminals                        │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * Timing model: exactly like examples/Modules/Button/01-02, this project
 * still has no genuinely concurrent work competing for the CPU (no
 * variable-rate animation the way Button project 3 needed one) — so it
 * keeps the simple `button.update(); _delay_ms(1);` busy-wait rather than
 * reaching for a millis() clock yet. That upgrade arrives in project 4,
 * once soft-start ramping actually needs its own independent schedule.
 *
 * ADC concepts introduced:
 *   - ADCDriver::begin() — defaults to AVCC reference, DIV128 prescaler
 *     (125 kHz ADC clock @ 16 MHz, the datasheet-recommended range for
 *     full 10-bit accuracy).
 *   - ADCDriver::read(channel) — blocking single conversion, returns the
 *     full 10-bit result (0-1023) in the ADC register pair. Channel 0
 *     corresponds to pin PC0 (ADC0) on the ATmega328P.
 *   - The pot is only sampled once every 10 ms (via a simple tick
 *     counter), not on every 1 ms loop pass — a blocking ADC conversion
 *     costs on the order of 100 us at this prescaler, and there is no
 *     reason to spend that on every single millisecond tick when a
 *     human hand turning a knob can't produce meaningfully different
 *     readings that fast anyway.
 *
 * DCMotor concepts reused from projects 1-2:
 *   - DCMotor(in1, in2, pwmPin), begin(), speed(pct), brake().
 *
 * Button concepts reused from Button projects 1 and 3:
 *   - Button(pin), begin(), update(), clicked(), longPressed().
 */

#include <util/delay.h>
#include <mikroduino/adc.hpp>
#include <mikroduino/usart.hpp>
#include <Button.hpp>
#include <DCMotor.hpp>

using namespace MikroDuino;

DCMotor  motor(PD4, PD5, PD6);
Button   dirButton(PD2);
ADCDriver adc;

int main() {
    USART0.begin(9600);
    USART0.writeLine_P(PSTR("DCMotor potentiometer throttle + button direction/e-stop"));
    USART0.writeLine_P(PSTR("=========================================================="));
    USART0.writeLine_P(PSTR("Click: flip direction.  Hold: toggle emergency stop."));
    USART0.writeLine_P(PSTR(""));

    motor.begin();
    dirButton.begin();
    adc.begin();   // AVCC reference, DIV128 prescaler (default)

    bool forward = true;
    bool eStop   = false;
    uint8_t lastPrintedPct = 0xFF;   // force one print on the first sample

    uint8_t adcTickMs = 0;

    while (true) {
        dirButton.update();

        if (dirButton.clicked()) {
            forward = !forward;
            USART0.writeLine_P(forward ? PSTR("Direction: FORWARD") : PSTR("Direction: REVERSE"));
        }

        if (dirButton.longPressed()) {
            eStop = !eStop;
            USART0.writeLine_P(eStop ? PSTR(">>> EMERGENCY STOP LATCHED <<<")
                                      : PSTR("Emergency stop released — resuming throttle"));
        }

        if (eStop) {
            // Latched: ignore the pot entirely and hold the brake on
            // until the next long-press releases the latch above.
            motor.brake();
        } else if (++adcTickMs >= 10) {
            adcTickMs = 0;

            uint16_t raw = adc.read(0);                         // 0-1023
            uint8_t  pct = static_cast<uint8_t>((static_cast<uint32_t>(raw) * 100u) / 1023u);

            motor.speed(forward ? static_cast<int8_t>(pct) : static_cast<int8_t>(-pct));

            if (pct != lastPrintedPct) {
                lastPrintedPct = pct;
                USART0.write_P(PSTR("Throttle: "));
                USART0.writeInt(pct);
                USART0.writeLine_P(PSTR(" %"));
            }
        }

        _delay_ms(1);   // Button::update() expects to be called ~every 1 ms
    }
}
