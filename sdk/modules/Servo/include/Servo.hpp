#pragma once
/*
 * Servo — hardware PWM servo driver using Timer1.
 *
 * Generates a 50 Hz (20 ms period) PWM signal with 0.5 µs resolution
 * by running Timer1 in Fast PWM mode with ICR1 as TOP and prescaler 8.
 *
 * Two independent channels are available:
 *   channel 0  →  OC1A  (PB1, Arduino D9)
 *   channel 1  →  OC1B  (PB2, Arduino D10)
 *
 * CAUTION: Timer1 is exclusively owned by this module. Do not use
 * MikroDuino PWM1 or Timer1 simultaneously.
 *
 * Usage:
 *   #include <Servo.hpp>
 *
 *   MikroDuino::Servo pan(0);   // OC1A
 *   MikroDuino::Servo tilt(1);  // OC1B
 *
 *   pan.begin();
 *   tilt.begin();
 *   pan.write(90);              // centre position
 *   tilt.writeMicroseconds(1200);
 */

#include <stdint.h>
#include <avr/io.h>
#include <mikroduino/gpio.hpp>

namespace MikroDuino {

class Servo {
public:
    // channel  : 0 = OC1A (PB1), 1 = OC1B (PB2)
    // minUs    : pulse width for 0° (default 1000 µs)
    // maxUs    : pulse width for 180° (default 2000 µs)
    Servo(uint8_t channel, uint16_t minUs = 1000, uint16_t maxUs = 2000);

    // Set pin direction and enable the PWM output. Safe to call on both
    // channels; Timer1 is initialised once and left running.
    void begin();

    // Disconnect OC pin from Timer1 (pin becomes a regular GPIO).
    void detach();

    // Move to angleDeg (0–180). Values are clamped to [minUs, maxUs].
    void write(uint8_t angleDeg);

    // Set pulse width directly in microseconds. Clamped to [minUs, maxUs].
    void writeMicroseconds(uint16_t us);

    uint16_t readMicroseconds() const { return _currentUs; }

private:
    uint8_t  _channel;
    uint16_t _minUs, _maxUs, _currentUs;

    // Timer1 TOP for 50 Hz at prescaler 8: (F_CPU / 8 / 50) − 1
    static constexpr uint16_t _T1_TOP =
        static_cast<uint16_t>((F_CPU / 8UL / 50UL) - 1UL);

    // Convert microseconds to Timer1 compare value (ticks of 0.5 µs at 16 MHz)
    static uint16_t _usToCmp(uint16_t us) {
        // (us * (F_CPU/1e6)) / 8  — reordered to avoid 32-bit overflow
        return static_cast<uint16_t>(
            (static_cast<uint32_t>(us) * (F_CPU / 1000000UL)) / 8UL
        );
    }
};

} // namespace MikroDuino
