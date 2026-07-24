#pragma once
/*
 * MikroDuino ADC Library
 *
 * Full control: reference, prescaler, channel, interrupt, free-running.
 * No hidden conversions. You start it, you read it.
 */

#include "platform.hpp"
#include "registers.hpp"
#include <avr/io.h>
#include <stdint.h>

namespace MikroDuino {

enum class ADCRef : uint8_t {
    AREF     = 0,           // External AREF
    AVCC     = 1,           // VCC with cap on AREF
    Internal = 3,           // Internal 1.1 V (328P) / 2.56 V (mega64/128)
};

enum class ADCPrescaler : uint8_t {
    DIV2   = 1,
    DIV4   = 2,
    DIV8   = 3,
    DIV16  = 4,
    DIV32  = 5,
    DIV64  = 6,
    DIV128 = 7,
};

// For 10-bit accuracy the ADC clock should be 50-200 kHz.
// At 16 MHz → prescaler 128 → ADC clock 125 kHz  (recommended default)
// At 8 MHz  → prescaler  64 → ADC clock 125 kHz

class ADCDriver {
public:
    void begin(
        ADCRef       ref        = ADCRef::AVCC,
        ADCPrescaler prescaler  = ADCPrescaler::DIV128,
        bool         leftAdjust = false)
    {
        ADMUX  = (static_cast<uint8_t>(ref) << REFS0) | (leftAdjust ? (1<<ADLAR) : 0);
        ADCSRA = (1<<ADEN) | static_cast<uint8_t>(prescaler);
    }

    void end() {
        BITCLEAR(ADCSRA, ADEN);
    }

    // Blocking single conversion on channel 0-7 (or 8=temp sensor on 328P)
    uint16_t read(uint8_t channel) {
        selectChannel(channel);
        BITSET(ADCSRA, ADSC);              // start conversion
        WAIT_BITCLEAR(ADCSRA, ADSC);       // wait for completion
        return ADC;                         // 16-bit result register
    }

    // Read as 8-bit (requires leftAdjust=true in begin())
    uint8_t read8(uint8_t channel) {
        selectChannel(channel);
        BITSET(ADCSRA, ADSC);
        WAIT_BITCLEAR(ADCSRA, ADSC);
        return ADCH;
    }

    // Free-running mode: conversions run automatically
    void beginFreeRunning(uint8_t channel) {
        selectChannel(channel);
        BITSET(ADCSRB, 0); // auto trigger source = free running
        BITSET(ADCSRA, ADATE);
        BITSET(ADCSRA, ADSC);
    }

    void stopFreeRunning() {
        BITCLEAR(ADCSRA, ADATE);
        BITCLEAR(ADCSRA, ADSC);
    }

    uint16_t resultFreeRunning() const { return ADC; }

    // Interrupt
    void enableInterrupt()  { BITSET(ADCSRA, ADIE); }
    void disableInterrupt() { BITCLEAR(ADCSRA, ADIE); }

    bool conversionComplete() const { return BITREAD(ADCSRA, ADIF) != 0; }
    void clearFlag()                { BITSET(ADCSRA, ADIF); }

    // Set voltage reference without restarting ADC
    void setReference(ADCRef ref) {
        FIELD_SET(ADMUX, (0x03<<REFS0), REFS0, static_cast<uint8_t>(ref));
    }

private:
    void selectChannel(uint8_t ch) {
        ADMUX = (ADMUX & 0xF0u) | (ch & 0x0Fu);
    }
};

static ADCDriver ADC_Driver __attribute__((unused));

} // namespace MikroDuino
