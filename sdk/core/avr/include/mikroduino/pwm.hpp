#pragma once
/*
 * MikroDuino PWM Library
 *
 * Hardware PWM using Timer1 (16-bit) and Timer0 (8-bit).
 * Fast PWM and Phase-Correct PWM modes.
 * Frequency control via ICR1 (Timer1) or OCRnA (Timer0/2).
 */

#include "platform.hpp"
#include "registers.hpp"
#include "gpio.hpp"
#include <avr/io.h>
#include <stdint.h>

namespace MikroDuino {

enum class PWMChannel : uint8_t {
    OC0A = 0,  // Timer0 ch A — PD6 (328P)
    OC0B = 1,  // Timer0 ch B — PD5
    OC1A = 2,  // Timer1 ch A — PB1
    OC1B = 3,  // Timer1 ch B — PB2
    OC2A = 4,  // Timer2 ch A — PB3
    OC2B = 5,  // Timer2 ch B — PD3
};

enum class PWMType : uint8_t {
    FastPWM,
    PhaseCorrect,
};

// --------------------------------------------------------------------------
// 16-bit PWM via Timer1 — best frequency resolution
// --------------------------------------------------------------------------
class PWM1Driver {
public:
    // frequency in Hz, top calculated automatically using ICR1
    void begin(uint32_t frequencyHz, PWMType type = PWMType::FastPWM) {
        // Choose smallest prescaler so ICR1 fits in 16 bits
        uint32_t prescalers[] = {1, 8, 64, 256, 1024};
        uint8_t  csBits[]     = {
            (1<<CS10),
            (1<<CS11),
            (1<<CS11)|(1<<CS10),
            (1<<CS12),
            (1<<CS12)|(1<<CS10)
        };

        for (uint8_t i = 0; i < 5; ++i) {
            uint32_t top;
            if (type == PWMType::FastPWM)
                top = F_CPU / (prescalers[i] * frequencyHz) - 1;
            else
                top = F_CPU / (2UL * prescalers[i] * frequencyHz) - 1;

            if (top <= 0xFFFFUL) {
                ICR1 = static_cast<uint16_t>(top);
                _top = static_cast<uint16_t>(top);

                uint8_t wgm = (type == PWMType::FastPWM) ? 0b1110 : 0b1010;
                TCCR1A = ((wgm & 0x03) << WGM10);
                TCCR1B = ((wgm >> 2) << WGM12) | csBits[i];
                return;
            }
        }
        // fallback: slowest
        ICR1 = 0xFFFF;
        _top = 0xFFFF;
        TCCR1A = (1<<WGM11);
        TCCR1B = (1<<WGM13)|(1<<WGM12)|(1<<CS12)|(1<<CS10);
    }

    // duty: 0-100 percent
    void dutyA(uint8_t percent) {
        enableChannelA();
        OCR1A = static_cast<uint16_t>(_top * percent / 100u);
    }

    void dutyB(uint8_t percent) {
        enableChannelB();
        OCR1B = static_cast<uint16_t>(_top * percent / 100u);
    }

    // raw duty (0 to ICR1)
    void rawA(uint16_t value) {
        enableChannelA();
        OCR1A = value;
    }

    void rawB(uint16_t value) {
        enableChannelB();
        OCR1B = value;
    }

    void stopA() {
        TCCR1A &= ~((1<<COM1A1)|(1<<COM1A0));
        GPIO::input(PB1);
    }

    void stopB() {
        TCCR1A &= ~((1<<COM1B1)|(1<<COM1B0));
        GPIO::input(PB2);
    }

    uint16_t top() const { return _top; }

private:
    uint16_t _top = 0xFFFF;

    void enableChannelA() {
        GPIO::output(PB1);
        TCCR1A |= (1<<COM1A1); // non-inverting
        TCCR1A &= ~(1<<COM1A0);
    }

    void enableChannelB() {
        GPIO::output(PB2);
        TCCR1A |= (1<<COM1B1);
        TCCR1A &= ~(1<<COM1B0);
    }
};

static PWM1Driver PWM1;

} // namespace MikroDuino
