#pragma once
/*
 * MikroDuino Timer Library
 *
 * Full hardware timer control. Nothing hidden.
 * Supports Timer0 (8-bit), Timer1 (16-bit), Timer2 (8-bit async).
 * Timer3/4 on ATmega64/128.
 */

#include "platform.hpp"
#include "registers.hpp"
#include <avr/io.h>
#include <stdint.h>

namespace MikroDuino {

enum class TimerMode : uint8_t {
    Normal         = 0,
    CTC            = 1,   // Clear Timer on Compare
    FastPWM        = 2,
    PhaseCorrectPWM = 3,
};

enum class TimerPrescaler : uint16_t {
    Off      = 0,
    DIV1     = 1,
    DIV8     = 8,
    DIV64    = 64,
    DIV256   = 256,
    DIV1024  = 1024,
    ExtFall  = 0xFFFE, // external clock falling edge
    ExtRise  = 0xFFFF, // external clock rising edge
};

// --------------------------------------------------------------------------
// 16-bit Timer1 driver (primary general-purpose timer)
// --------------------------------------------------------------------------
class Timer1Driver {
public:
    void mode(TimerMode m) {
        _mode = m;
    }

    void prescaler(TimerPrescaler p) {
        _prescaler = p;
    }

    // Set compare A value (used for CTC period / PWM duty)
    void compareA(uint16_t val) {
        OCR1A = val;
    }

    void compareB(uint16_t val) {
        OCR1B = val;
    }

    void inputCapture(uint16_t val) {
        ICR1 = val;
    }

    void start() {
        // Build TCCR1A
        uint8_t tccr1a = 0;
        uint8_t tccr1b = 0;

        switch (_mode) {
            case TimerMode::Normal:
                // WGM = 0000
                break;
            case TimerMode::CTC:
                // WGM = 0100 (CTC with OCR1A)
                tccr1b |= (1<<WGM12);
                break;
            case TimerMode::FastPWM:
                // WGM = 1110 (fast PWM, ICR1 TOP)
                tccr1a |= (1<<WGM11);
                tccr1b |= (1<<WGM13)|(1<<WGM12);
                break;
            case TimerMode::PhaseCorrectPWM:
                // WGM = 1010 (phase correct, ICR1 TOP)
                tccr1a |= (1<<WGM11);
                tccr1b |= (1<<WGM13);
                break;
        }

        // Prescaler bits
        tccr1b |= prescalerBits();

        TCCR1A = tccr1a;
        TCCR1B = tccr1b;
    }

    void stop() {
        // Clear prescaler bits → stops clock
        TCCR1B &= ~((1<<CS12)|(1<<CS11)|(1<<CS10));
    }

    void reset() {
        TCNT1 = 0;
    }

    uint16_t count() const {
        return TCNT1;
    }

    void enableInterruptA()  { BITSET(TIMSK1, OCIE1A); }
    void disableInterruptA() { BITCLEAR(TIMSK1, OCIE1A); }
    void enableInterruptB()  { BITSET(TIMSK1, OCIE1B); }
    void disableInterruptB() { BITCLEAR(TIMSK1, OCIE1B); }
    void enableOverflow()    { BITSET(TIMSK1, TOIE1); }
    void disableOverflow()   { BITCLEAR(TIMSK1, TOIE1); }
    void enableCapture()     { BITSET(TIMSK1, ICIE1); }
    void disableCapture()    { BITCLEAR(TIMSK1, ICIE1); }

    bool overflowFlag() const  { return BITREAD(TIFR1, TOV1) != 0; }
    bool compareAFlag() const  { return BITREAD(TIFR1, OCF1A) != 0; }
    void clearOverflowFlag()   { BITSET(TIFR1, TOV1); }
    void clearCompareAFlag()   { BITSET(TIFR1, OCF1A); }

    // Helper: compute period in ticks for a given frequency
    uint16_t ticksForHz(uint32_t hz) const {
        uint32_t psr = prescalerValue();
        if (psr == 0) return 0xFFFF;
        return static_cast<uint16_t>(F_CPU / (psr * hz) - 1);
    }

    uint32_t prescalerValue() const {
        switch (_prescaler) {
            case TimerPrescaler::DIV1:    return 1;
            case TimerPrescaler::DIV8:    return 8;
            case TimerPrescaler::DIV64:   return 64;
            case TimerPrescaler::DIV256:  return 256;
            case TimerPrescaler::DIV1024: return 1024;
            default:                       return 0;
        }
    }

private:
    TimerMode _mode           = TimerMode::Normal;
    TimerPrescaler _prescaler = TimerPrescaler::DIV1;

    uint8_t prescalerBits() const {
        switch (_prescaler) {
            case TimerPrescaler::Off:     return 0;
            case TimerPrescaler::DIV1:    return (1<<CS10);
            case TimerPrescaler::DIV8:    return (1<<CS11);
            case TimerPrescaler::DIV64:   return (1<<CS11)|(1<<CS10);
            case TimerPrescaler::DIV256:  return (1<<CS12);
            case TimerPrescaler::DIV1024: return (1<<CS12)|(1<<CS10);
            case TimerPrescaler::ExtFall: return (1<<CS12)|(1<<CS11);
            case TimerPrescaler::ExtRise: return (1<<CS12)|(1<<CS11)|(1<<CS10);
            default:                       return 0;
        }
    }
};

// --------------------------------------------------------------------------
// 8-bit Timer0 driver
// --------------------------------------------------------------------------
class Timer0Driver {
public:
    void mode(TimerMode m) { _mode = m; }
    void prescaler(TimerPrescaler p) { _prescaler = p; }

    void compareA(uint8_t val) { OCR0A = val; }
    void compareB(uint8_t val) { OCR0B = val; }

    void start() {
        uint8_t tccr0a = 0;
        uint8_t tccr0b = 0;
        switch (_mode) {
            case TimerMode::Normal: break;
            case TimerMode::CTC:
                tccr0a |= (1<<WGM01); break;
            case TimerMode::FastPWM:
                tccr0a |= (1<<WGM01)|(1<<WGM00); break;
            case TimerMode::PhaseCorrectPWM:
                tccr0a |= (1<<WGM00); break;
        }
        tccr0b |= prescalerBits();
        TCCR0A = tccr0a;
        TCCR0B = tccr0b;
    }

    void stop()  { TCCR0B &= ~((1<<CS02)|(1<<CS01)|(1<<CS00)); }
    void reset() { TCNT0 = 0; }
    uint8_t count() const { return TCNT0; }

    void enableInterruptA()  { BITSET(TIMSK0, OCIE0A); }
    void disableInterruptA() { BITCLEAR(TIMSK0, OCIE0A); }
    void enableOverflow()    { BITSET(TIMSK0, TOIE0); }
    void disableOverflow()   { BITCLEAR(TIMSK0, TOIE0); }

private:
    TimerMode _mode           = TimerMode::Normal;
    TimerPrescaler _prescaler = TimerPrescaler::DIV1;

    uint8_t prescalerBits() const {
        switch (_prescaler) {
            case TimerPrescaler::Off:     return 0;
            case TimerPrescaler::DIV1:    return (1<<CS00);
            case TimerPrescaler::DIV8:    return (1<<CS01);
            case TimerPrescaler::DIV64:   return (1<<CS01)|(1<<CS00);
            case TimerPrescaler::DIV256:  return (1<<CS02);
            case TimerPrescaler::DIV1024: return (1<<CS02)|(1<<CS00);
            default:                       return 0;
        }
    }
};

static Timer0Driver Timer0;
static Timer1Driver Timer1;

} // namespace MikroDuino
