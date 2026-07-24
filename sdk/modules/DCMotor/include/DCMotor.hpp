#pragma once
/*
 * DCMotor — H-bridge DC motor driver (L298N, L293D, TB6612, etc.)
 *
 * Controls direction via two GPIO pins (IN1, IN2) and speed via an
 * optional hardware PWM pin connected to ENA / PWMA.
 *
 * Without a PWM pin the motor runs at full speed or stops (digital mode).
 * With a PWM pin, speed() scales output from 0–100 % duty cycle.
 *
 * Supported PWM pins on ATmega328P (Timer0 / Timer2 — Timer1 is for Servo):
 *   OC0A  PD6   (3<<3)|6 = 0x1E      ~977 Hz PWM
 *   OC0B  PD5   (3<<3)|5 = 0x1D      ~977 Hz PWM
 *   OC2A  PB3   (1<<3)|3 = 0x0B      ~977 Hz PWM
 *   OC2B  PD3   (3<<3)|3 = 0x1B      ~977 Hz PWM
 *
 * Usage (full speed, no PWM):
 *   MikroDuino::DCMotor motor(PD4, PD5);
 *   motor.begin();
 *   motor.speed(100);   // full CW
 *   motor.speed(-50);   // half speed CCW
 *   motor.brake();
 *
 * Usage (PWM speed on OC0A = PD6):
 *   MikroDuino::DCMotor motor(PD4, PD5, PD6);
 *   motor.begin();
 *   motor.speed(75);    // 75 % duty, CW
 */

#include <stdint.h>
#include <avr/io.h>
#include <mikroduino/gpio.hpp>

namespace MikroDuino {

class DCMotor {
public:
    static constexpr uint8_t NO_PWM = 0xFF;

    // in1, in2  : direction pins (any GPIO)
    // pwmPin    : ENA / PWMA pin; NO_PWM for full-speed-only mode
    DCMotor(uint8_t in1, uint8_t in2, uint8_t pwmPin = NO_PWM);

    // Configure pin modes and initialise the PWM timer if pwmPin is set.
    void begin();

    // speed : -100 (full CCW) … 0 (coast) … +100 (full CW)
    void speed(int8_t pct);

    // brake : short-circuit both motor terminals (fast stop)
    void brake();

    // coast : float both motor terminals (motor spins down freely)
    void coast();

private:
    uint8_t _in1, _in2, _pwmPin;
    volatile uint8_t* _ocr; // OCR register for pwmPin; nullptr in NO_PWM mode

    void _initPWM();
    void _setPWM(uint8_t duty);
};

// ---------- inline implementations ----------

inline DCMotor::DCMotor(uint8_t in1, uint8_t in2, uint8_t pwmPin)
    : _in1(in1), _in2(in2), _pwmPin(pwmPin), _ocr(nullptr) {}

inline void DCMotor::_initPWM() {
    // ATmega328P pin→OCR map. Two timers cover all four PWM channels.
    // PD6=0x1E→OC0A, PD5=0x1D→OC0B, PB3=0x0B→OC2A, PD3=0x1B→OC2B
    switch (_pwmPin) {
        case 0x1E: // OC0A — Fast PWM, prescaler 64 (~977 Hz @ 16 MHz)
            DDRD   |= (1u << DDD6);
            TCCR0A |= (1u << COM0A1) | (1u << WGM01) | (1u << WGM00);
            TCCR0B |= (1u << CS01)   | (1u << CS00);
            _ocr    = &OCR0A;
            break;
        case 0x1D: // OC0B
            DDRD   |= (1u << DDD5);
            TCCR0A |= (1u << COM0B1) | (1u << WGM01) | (1u << WGM00);
            TCCR0B |= (1u << CS01)   | (1u << CS00);
            _ocr    = &OCR0B;
            break;
        case 0x0B: // OC2A — Fast PWM, prescaler 64
            DDRB   |= (1u << DDB3);
            TCCR2A |= (1u << COM2A1) | (1u << WGM21) | (1u << WGM20);
            TCCR2B |= (1u << CS22);
            _ocr    = &OCR2A;
            break;
        case 0x1B: // OC2B
            DDRD   |= (1u << DDD3);
            TCCR2A |= (1u << COM2B1) | (1u << WGM21) | (1u << WGM20);
            TCCR2B |= (1u << CS22);
            _ocr    = &OCR2B;
            break;
        default:
            _ocr = nullptr;
            break;
    }
    if (_ocr) *_ocr = 0;
}

inline void DCMotor::begin() {
    GPIO::output(_in1);
    GPIO::output(_in2);
    GPIO::clear(_in1);
    GPIO::clear(_in2);
    if (_pwmPin != NO_PWM) _initPWM();
}

inline void DCMotor::_setPWM(uint8_t duty) {
    if (_ocr) *_ocr = duty;
}

inline void DCMotor::speed(int8_t pct) {
    if (pct == 0) { coast(); return; }
    uint8_t absPct = (pct < 0) ? static_cast<uint8_t>(-pct)
                                : static_cast<uint8_t>(pct);
    uint8_t duty = (_ocr)
        ? static_cast<uint8_t>(static_cast<uint16_t>(absPct) * 255u / 100u)
        : 255u;

    GPIO::write(_in1, pct > 0);
    GPIO::write(_in2, pct < 0);
    _setPWM(duty);
}

inline void DCMotor::brake() {
    // Both IN pins HIGH → motor is actively shorted → fast stop
    _setPWM(255);
    GPIO::set(_in1);
    GPIO::set(_in2);
}

inline void DCMotor::coast() {
    // Both IN pins LOW → driver tri-states → motor coasts
    _setPWM(0);
    GPIO::clear(_in1);
    GPIO::clear(_in2);
}

} // namespace MikroDuino
