#pragma once
/*
 * Stepper — blocking step/direction driver for A4988, DRV8825, TMC2208
 * and compatible stepper motor controllers.
 *
 * Generates STEP pulses and sets DIR. The step() call busy-waits between
 * pulses using _delay_ms / _delay_us, so it blocks the CPU for the full
 * move. For non-blocking use, call step(1) from a MikroDuino Scheduler task.
 *
 * Pin encoding: bits[5:3] = port (0=A…6=G), bits[2:0] = bit. Use the PB0…PD7
 * constants from <mikroduino/gpio.hpp>.
 *
 * Usage:
 *   #include <Stepper.hpp>
 *
 *   MikroDuino::Stepper motor(PD2, PD3);        // step=PD2, dir=PD3
 *   MikroDuino::Stepper motor(PD2, PD3, PD4);   // + active-LOW enable
 *
 *   motor.begin();
 *   motor.setRPM(60, 200);    // 60 RPM, 200 full steps/rev
 *   motor.step(200);          // 1 full revolution CW
 *   motor.step(-400);         // 2 revolutions CCW
 *
 * Microstepping: stepsPerRev × microstep divisor (e.g. 200 × 16 = 3200
 * for 1/16 microstepping on an A4988).
 *
 * Timing note: the per-step delay is split into a millisecond chunk
 * (accurate) plus a sub-millisecond busy-wait. Accuracy is ±1% for
 * step delays ≥ 500 µs; this covers speeds up to ~600 RPM (200 steps/rev).
 */

#include <stdint.h>
#include <util/delay.h>
#include <mikroduino/gpio.hpp>

namespace MikroDuino {

class Stepper {
public:
    static constexpr uint8_t NO_PIN = 0xFF;

    // stepPin, dirPin  : MikroDuino GPIO pin encoding
    // enablePin        : active-LOW /EN pin; pass NO_PIN to omit
    Stepper(uint8_t stepPin, uint8_t dirPin, uint8_t enablePin = NO_PIN);

    // Configure pin directions and optionally enable the driver.
    void begin();

    // Compute step delay from speed and motor resolution.
    //   rpm         : desired speed in revolutions per minute
    //   stepsPerRev : full steps/rev × microstep divisor (e.g. 200, 3200)
    void setRPM(uint16_t rpm, uint16_t stepsPerRev = 200);

    // Set the total step period (pulse high + idle low) directly in µs.
    void setStepDelay(uint32_t us);

    // Move n steps; positive = CW (DIR HIGH), negative = CCW. Blocking.
    void step(int32_t steps);

    // Drive /EN pin: true = driver powered, false = driver disabled.
    void enable(bool on = true);

private:
    uint8_t  _stepPin, _dirPin, _enablePin;
    uint32_t _stepDelay_us;

    static void _delayUs(uint32_t us);
};

// ---------- inline implementations ----------

inline Stepper::Stepper(uint8_t stepPin, uint8_t dirPin, uint8_t enablePin)
    : _stepPin(stepPin), _dirPin(dirPin), _enablePin(enablePin),
      _stepDelay_us(5000UL) {} // 5 ms ≈ 60 RPM @ 200 steps/rev

inline void Stepper::begin() {
    GPIO::output(_stepPin);
    GPIO::output(_dirPin);
    GPIO::clear(_stepPin);
    GPIO::clear(_dirPin);
    if (_enablePin != NO_PIN) {
        GPIO::output(_enablePin);
        enable(true);
    }
}

inline void Stepper::setRPM(uint16_t rpm, uint16_t stepsPerRev) {
    if (rpm == 0) { _stepDelay_us = 0xFFFFFFFFUL; return; }
    // period (µs) = 60,000,000 / (rpm × stepsPerRev)
    _stepDelay_us = 60000000UL / (static_cast<uint32_t>(rpm) * stepsPerRev);
}

inline void Stepper::setStepDelay(uint32_t us) {
    _stepDelay_us = us;
}

inline void Stepper::enable(bool on) {
    if (_enablePin != NO_PIN)
        GPIO::write(_enablePin, !on); // active LOW
}

inline void Stepper::step(int32_t steps) {
    if (steps == 0) return;

    GPIO::write(_dirPin, steps > 0);
    _delayUs(2); // DIR setup time (A4988 spec: ≥ 200 ns; 2 µs is ample)

    uint32_t n = (steps > 0) ? static_cast<uint32_t>(steps)
                              : static_cast<uint32_t>(-steps);
    while (n--) {
        GPIO::set(_stepPin);
        _delayUs(2);                                     // STEP high time (≥ 1 µs)
        GPIO::clear(_stepPin);
        if (_stepDelay_us > 4) _delayUs(_stepDelay_us - 4); // remaining idle time
    }
}

// Split into ms chunks (accurate) + sub-ms residual to minimise loop overhead.
inline void Stepper::_delayUs(uint32_t us) {
    while (us >= 1000UL) {
        _delay_ms(1.0);
        us -= 1000UL;
    }
    uint16_t rem = static_cast<uint16_t>(us);
    while (rem--) _delay_us(1.0);
}

} // namespace MikroDuino
