#pragma once
/*
 * MikroDuino HC-SR04 Ultrasonic Distance Sensor
 */

#include <mikroduino/gpio.hpp>
#include <stdint.h>
#include <util/delay.h>

namespace MikroDuino {

class HCSR04 {
public:
    HCSR04(uint8_t trigPin, uint8_t echoPin)
        : _trig(trigPin), _echo(echoPin) {}

    void begin() {
        GPIO::output(_trig);
        GPIO::input(_echo);
        GPIO::clear(_trig);
    }

    // Returns distance in mm. Returns 0 if no echo (out of range or timeout).
    //
    // Timing is polled in fixed STEP_US increments via _delay_us(), which
    // avr-libc calibrates against F_CPU at compile time. Counting real
    // microseconds this way (instead of raw busy-wait loop iterations)
    // keeps both the timeout and the measured pulse width correct
    // regardless of clock speed or compiler optimization level -- the
    // previous iteration-counting version silently reported wrong
    // distances on anything but a 16 MHz part built exactly the same way.
    uint32_t measureMM() {
        static constexpr uint16_t STEP_US    = 10;
        static constexpr uint32_t TIMEOUT_US = 30000; // ~5 m round trip, beyond HC-SR04's rated range

        // 10 µs trigger pulse
        GPIO::set(_trig);
        _delay_us(10);
        GPIO::clear(_trig);

        // Wait for echo to go high
        uint32_t waitedUs = 0;
        while (!GPIO::read(_echo)) {
            _delay_us(STEP_US);
            waitedUs += STEP_US;
            if (waitedUs >= TIMEOUT_US) return 0;
        }

        // Measure how long echo stays high
        uint32_t pulseUs = 0;
        while (GPIO::read(_echo)) {
            _delay_us(STEP_US);
            pulseUs += STEP_US;
            if (pulseUs >= TIMEOUT_US) return 0; // echo never fell -- treat as failure, not a bogus reading
        }

        // Speed of sound: ~343 m/s = 0.343 mm/µs; ÷2 for round trip => 0.1715 mm/µs
        return (pulseUs * 1715UL) / 10000UL;
    }

    // Returns distance in cm
    uint32_t measureCM() { return measureMM() / 10; }

private:
    uint8_t _trig, _echo;
};

} // namespace MikroDuino
