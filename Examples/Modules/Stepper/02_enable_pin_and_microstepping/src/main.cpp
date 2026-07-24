/*
 * Stepper Enable Pin & Microstepping — enable()/disable(), stepsPerRev —
 * MikroDuino Module SDK
 *
 * Project 2 of 6 in the examples/Modules/Stepper series. Project 1 tied
 * the driver's /EN pin directly to GND so the driver was always powered.
 * This project wires /EN to the MCU instead, so the application can cut
 * coil current — and the heat and idle hum that comes with it — whenever
 * the motor doesn't need to hold position. It also switches the driver
 * to 1/16 microstepping (set on the DRIVER BOARD by its MS1/MS2/MS3
 * jumpers, tied to VCC — this class has no control over microstep
 * resolution, it only needs to know the resulting steps/revolution to
 * keep setRPM()'s timing correct).
 *
 * Hardware (ATmega328P @ 16 MHz, A4988/DRV8825, MS1/MS2/MS3 all tied
 * HIGH on the driver board for 1/16 microstepping):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ Signal  │ Pin   │ Wiring                                    │
 *   ├─────────┼───────┼──────────────────────────────────────────┤
 *   │ STEP    │ PD2   │ Driver STEP input                         │
 *   │ DIR     │ PD3   │ Driver DIR input                          │
 *   │ /EN     │ PD4   │ Driver enable, active LOW                 │
 *   │ TXD     │ PD1   │ USB-serial adapter, 9600 8N1               │
 *   │ MS1-3   │ —     │ All tied to driver VCC (1/16 microstep)   │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * Stepper concepts introduced:
 *   - Stepper(stepPin, dirPin, enablePin) — the three-argument
 *     constructor. Passing a real pin instead of Stepper::NO_PIN makes
 *     begin() configure it as an output and enable() take effect.
 *   - enable(bool) — drives /EN LOW (powered, holding torque) or HIGH
 *     (coils unpowered, motor free-spins, driver runs cool). begin()
 *     calls enable(true) once, so the motor starts powered.
 *   - setRPM(rpm, stepsPerRev) with a microstep-scaled stepsPerRev: at
 *     1/16 microstepping a full mechanical revolution is 200 x 16 = 3200
 *     steps rather than 200, so stepsPerRev must scale to match or the
 *     motor spins 16x slower than the requested RPM implies.
 */

#include <util/delay.h>
#include <mikroduino/usart.hpp>
#include <Stepper.hpp>

using namespace MikroDuino;

Stepper motor(PD2, PD3, PD4);   // step=PD2, dir=PD3, /EN=PD4

static constexpr uint16_t MICROSTEPS_PER_REV = 200 * 16;   // 1/16 microstepping

int main() {
    USART0.begin(9600);
    USART0.writeLine_P(PSTR("Stepper enable pin + microstepping"));
    USART0.writeLine_P(PSTR("===================================="));
    USART0.writeLine_P(PSTR(""));

    motor.begin();   // enable(true) runs here — driver powered from the start
    motor.setRPM(45, MICROSTEPS_PER_REV);

    while (true) {
        USART0.writeLine_P(PSTR("Enabled: 1 smooth microstepped revolution"));
        motor.step(MICROSTEPS_PER_REV);
        _delay_ms(500);

        USART0.writeLine_P(PSTR("Disabling driver - motor free-spins, coils cool down"));
        motor.enable(false);
        _delay_ms(2000);

        USART0.writeLine_P(PSTR("Re-enabling driver"));
        motor.enable(true);
        _delay_ms(200);   // brief settle before the next move
    }
}
