#pragma once
/*
 * IRSensor — single infrared proximity / reflectance sensor.
 *
 * DIGITAL mode  (default)
 *   Reads a comparator output pin (HIGH/LOW). Suited for FC-51, TCRT5000
 *   with comparator board, E18-D80NK. detected() returns true when the
 *   pin is at the active level (LOW by default — most boards pull the
 *   output LOW when an object is detected).
 *
 * ANALOG mode
 *   Reads an ADC channel and compares against a software threshold.
 *   Suited for bare TCRT5000 / QRE1113 phototransistor outputs where
 *   the ADC value increases when an object is near (more IR reflection).
 *   Pass an initialised ADCDriver instance to the constructor.
 *   detected() returns true when readRaw() > threshold (default 512).
 *   Adjust threshold depending on surface / ambient lighting.
 *
 * Usage — digital, active-LOW:
 *   MikroDuino::IRSensor ir(PD2);
 *   ir.begin();
 *   if (ir.detected()) { ... }
 *
 * Usage — analog, threshold 600:
 *   MikroDuino::ADCDriver adc;
 *   adc.begin();
 *   MikroDuino::IRSensor ir(PC0, MikroDuino::IRSensor::ANALOG, false, 600, &adc);
 *   ir.begin();
 *   uint16_t raw = ir.readRaw(); // 0..1023
 *   if (ir.detected()) { ... }
 */

#include <stdint.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/adc.hpp>

namespace MikroDuino {

class IRSensor {
public:
    enum Mode : uint8_t { DIGITAL, ANALOG };

    // pin       : MikroDuino GPIO encoding.
    //             DIGITAL mode: any digital pin.
    //             ANALOG mode : must be an ADC pin (PC0–PC5 on ATmega328P).
    // mode      : DIGITAL or ANALOG
    // activeLow : DIGITAL mode only. true = detected when pin reads LOW.
    // threshold : ANALOG mode only. detected() returns true when raw > threshold.
    // adc       : ANALOG mode only. Pointer to an initialised ADCDriver.
    IRSensor(uint8_t pin,
             Mode mode        = DIGITAL,
             bool activeLow   = true,
             uint16_t threshold = 512,
             ADCDriver* adc   = nullptr)
        : _pin(pin), _mode(mode), _activeLow(activeLow),
          _threshold(threshold), _adc(adc) {}

    void begin() {
        if (_mode == DIGITAL) GPIO::inputPullup(_pin);
        // ANALOG: pin is an ADC channel; no DDR config needed
    }

    // true when an object / line is detected
    bool detected() const {
        if (_mode == DIGITAL) {
            bool level = GPIO::read(_pin);
            return _activeLow ? !level : level;
        }
        return readRaw() > _threshold;
    }

    // DIGITAL: returns 0 or 1.
    // ANALOG : returns raw ADC value 0–1023.
    uint16_t readRaw() const {
        if (_mode == DIGITAL) return GPIO::read(_pin) ? 1u : 0u;
        if (!_adc) return 0;
        uint8_t ch = _pin & 0x07u; // bits[2:0] = ADC channel (port C on ATmega328P)
        return _adc->read(ch);
    }

    void setThreshold(uint16_t t) { _threshold = t; }

private:
    uint8_t    _pin;
    Mode       _mode;
    bool       _activeLow;
    uint16_t   _threshold;
    ADCDriver* _adc;
};

} // namespace MikroDuino
