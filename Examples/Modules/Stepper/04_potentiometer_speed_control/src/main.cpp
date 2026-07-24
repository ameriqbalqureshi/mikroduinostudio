/*
 * Stepper + ADC — Potentiometer Speed Control — MikroDuino Module SDK
 *
 * Project 4 of 6 in the examples/Modules/Stepper series. Replaces the
 * fixed setRPM() call from projects 1-2 with a potentiometer read live,
 * every loop pass, so twisting the knob speeds the motor up or slows it
 * down while it's already turning — the same live-analog-control idea
 * examples/Modules/DCMotor/03 and Servo/03 used for a motor's speed and
 * a servo's angle respectively.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ STEP    │ PD2   │ Driver STEP input                         │
 *   │ DIR     │ PD3   │ Driver DIR input, tied so motion is always │
 *   │         │       │ CW in this project                        │
 *   │ Pot     │ PC0   │ ADC0 — wiper; outer legs to 5V/GND        │
 *   │ TXD     │ PD1   │ USB-serial adapter, 9600 8N1               │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * Why step(1) instead of step(200) in a loop: setRPM() only affects the
 * step delay used by the NEXT call to step(). A single blocking
 * step(200) call would commit to one speed for a whole revolution before
 * the pot could be re-read. Calling step(1) once per pulse and re-
 * reading the pot in between lets speed track the knob in near real
 * time. No extra throttling is needed on the ADC read — step(1) itself
 * already blocks for the current step delay (a few ms up to ~20 ms
 * across this project's RPM range), so the pot is naturally sampled no
 * faster than the motor is actually stepping.
 *
 * ADC concepts reused from DCMotor project 3:
 *   - ADCDriver::begin(), ADCDriver::read(channel) — blocking 10-bit
 *     conversion (0-1023).
 *
 * Stepper concepts reused from project 1:
 *   - Stepper(stepPin, dirPin), begin(), setRPM(rpm, stepsPerRev).
 * Stepper concepts introduced:
 *   - Calling setRPM() repeatedly, between individual step(1) calls, to
 *     change speed on the fly instead of committing to one speed for an
 *     entire multi-step move.
 */

#include <mikroduino/adc.hpp>
#include <mikroduino/usart.hpp>
#include <Stepper.hpp>

using namespace MikroDuino;

Stepper   motor(PD2, PD3);
ADCDriver adc;

static constexpr uint16_t STEPS_PER_REV = 200;
static constexpr uint16_t MIN_RPM = 5;
static constexpr uint16_t MAX_RPM = 120;

int main() {
    USART0.begin(9600);
    USART0.writeLine_P(PSTR("Stepper potentiometer speed control"));
    USART0.writeLine_P(PSTR("====================================="));
    USART0.writeLine_P(PSTR(""));

    motor.begin();
    adc.begin();   // AVCC reference, DIV128 prescaler (default)

    uint16_t lastPrintedRpm = 0xFFFF;

    while (true) {
        uint16_t raw = adc.read(0);   // 0-1023
        uint16_t rpm = static_cast<uint16_t>(MIN_RPM +
            (static_cast<uint32_t>(raw) * (MAX_RPM - MIN_RPM)) / 1023u);
        motor.setRPM(rpm, STEPS_PER_REV);

        if (rpm != lastPrintedRpm) {
            lastPrintedRpm = rpm;
            USART0.write_P(PSTR("Speed: "));
            USART0.writeInt(static_cast<int32_t>(rpm));
            USART0.writeLine_P(PSTR(" RPM"));
        }

        motor.step(1);
    }
}
