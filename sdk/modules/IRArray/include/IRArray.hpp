#pragma once
/*
 * IRArray<N> — multi-sensor IR reflectance array for line following robots.
 *
 * Supports N sensors (2–16) in either DIGITAL or ANALOG mode and computes
 * a continuous line position using a weighted centre-of-mass formula.
 *
 *   linePosition() returns  0           → line under leftmost sensor
 *                           (N-1)*1000  → line under rightmost sensor
 *                           -1          → no sensor sees the line (lost)
 *
 * DIGITAL mode (default)
 *   Reads each pin as HIGH or LOW. Fast (no ADC). No calibration needed.
 *   Pass activeLow=true if sensor output goes LOW over the line (most
 *   digital comparator boards).
 *
 * ANALOG mode
 *   Reads each pin via ADC and normalises readings 0–1000 using per-sensor
 *   min/max calibration. Robust against ambient light and sensor variation.
 *   Call calibrate(surface, samples) while driving over black and white
 *   surfaces in setup, then call linePosition() in the control loop.
 *
 * Usage — 5 digital sensors, active-LOW:
 *   const uint8_t PINS[5] = { PD2, PD3, PD4, PD5, PD6 };
 *   MikroDuino::IRArray<5> array(PINS);
 *   array.begin();
 *   int16_t pos = array.linePosition(); // centre = 2000
 *
 * Usage — 6 analog sensors with calibration:
 *   MikroDuino::ADCDriver adc;
 *   adc.begin();
 *   const uint8_t PINS[6] = { PC0, PC1, PC2, PC3, PC4, PC5 };
 *   MikroDuino::IRArray<6> array(PINS, MikroDuino::IRArray<6>::ANALOG, false, &adc);
 *   array.begin();
 *   // In setup: sweep robot over line surface
 *   for (uint8_t i = 0; i < 50; i++) { array.calibrateBlack(); _delay_ms(20); }
 *   // Sweep over background
 *   for (uint8_t i = 0; i < 50; i++) { array.calibrateWhite(); _delay_ms(20); }
 *   // In loop:
 *   int16_t pos = array.linePosition();
 */

#include <stdint.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/adc.hpp>

namespace MikroDuino {

template<uint8_t N>
class IRArray {
    static_assert(N >= 2 && N <= 16, "IRArray: N must be in range [2, 16]");

public:
    enum Mode : uint8_t { DIGITAL, ANALOG };

    // pins      : array of N MikroDuino GPIO pin encodings, left to right
    // mode      : DIGITAL or ANALOG
    // activeLow : DIGITAL mode — true if sensor output is LOW when over the line
    // adc       : ANALOG mode — pointer to an initialised ADCDriver
    IRArray(const uint8_t pins[N],
            Mode mode      = DIGITAL,
            bool activeLow = true,
            ADCDriver* adc = nullptr)
        : _mode(mode), _activeLow(activeLow), _adc(adc)
    {
        for (uint8_t i = 0; i < N; i++) {
            _pins[i]  = pins[i];
            _minADC[i] = 1023u; // will be set by calibrateWhite()
            _maxADC[i] = 0u;    // will be set by calibrateBlack()
        }
    }

    // Set pin directions (DIGITAL: input with pull-up; ANALOG: no-op).
    void begin() {
        if (_mode == DIGITAL) {
            for (uint8_t i = 0; i < N; i++) GPIO::inputPullup(_pins[i]);
        }
    }

    // Fill out[N] with per-sensor values 0–1000 (0 = off line, 1000 = on line).
    void read(uint16_t out[N]) const {
        for (uint8_t i = 0; i < N; i++) out[i] = _readNorm(i);
    }

    // Weighted centre-of-mass position: 0 (left) … (N-1)*1000 (right).
    // Returns -1 when no sensor sees the line.
    int16_t linePosition() const {
        uint32_t weightSum = 0;
        uint32_t posSum    = 0;
        for (uint8_t i = 0; i < N; i++) {
            uint16_t v = _readNorm(i);
            weightSum += v;
            posSum    += static_cast<uint32_t>(v) * (static_cast<uint16_t>(i) * 1000u);
        }
        if (weightSum == 0) return -1;
        return static_cast<int16_t>(posSum / weightSum);
    }

    // true if any sensor sees the line
    bool onLine() const {
        for (uint8_t i = 0; i < N; i++)
            if (_readNorm(i) > 500u) return true;
        return false;
    }

    // true if every sensor sees the line (robot fully over wide line or line lost)
    bool allOnLine() const {
        for (uint8_t i = 0; i < N; i++)
            if (_readNorm(i) <= 500u) return false;
        return true;
    }

    // ANALOG calibration — call repeatedly while positioned over the dark surface.
    // Tracks the maximum ADC reading seen per sensor (dark = less reflection = higher
    // ADC on TCRT5000-type sensors).
    void calibrateBlack() {
        if (_mode != ANALOG || !_adc) return;
        for (uint8_t i = 0; i < N; i++) {
            uint16_t v = _rawADC(i);
            if (v > _maxADC[i]) _maxADC[i] = v;
        }
    }

    // ANALOG calibration — call repeatedly while positioned over the bright surface.
    // Tracks the minimum ADC reading seen per sensor.
    void calibrateWhite() {
        if (_mode != ANALOG || !_adc) return;
        for (uint8_t i = 0; i < N; i++) {
            uint16_t v = _rawADC(i);
            if (v < _minADC[i]) _minADC[i] = v;
        }
    }

    // Returns the raw ADC value for sensor i (ANALOG mode only; 0 otherwise).
    uint16_t rawADC(uint8_t i) const {
        if (_mode != ANALOG || i >= N) return 0;
        return _rawADC(i);
    }

private:
    uint8_t    _pins[N];
    Mode       _mode;
    bool       _activeLow;
    ADCDriver* _adc;
    uint16_t   _minADC[N]; // white-surface (bright) calibration minimum
    uint16_t   _maxADC[N]; // black-surface (dark)   calibration maximum

    // Normalised sensor reading: 0 = off line, 1000 = on line
    uint16_t _readNorm(uint8_t i) const {
        if (_mode == DIGITAL) {
            bool high = GPIO::read(_pins[i]);
            bool onLine = _activeLow ? !high : high;
            return onLine ? 1000u : 0u;
        }
        // ANALOG with per-sensor calibration
        uint16_t v    = _rawADC(i);
        uint16_t lo   = _minADC[i]; // white / bright
        uint16_t hi   = _maxADC[i]; // black / dark

        // Guard: not calibrated or degenerate range
        if (hi <= lo) return 0;

        // Determine which direction maps to "on line":
        // If the calibrated black max > calibrated white min → higher ADC = on line (TCRT5000)
        // If calibrated black max < calibrated white min → lower ADC = on line (inverted sensors)
        // The convention is: calibrateBlack() captures the surface the robot should follow.
        // So "on line" = ADC close to _maxADC.  Normalise: (v - lo) / (hi - lo).
        if (v <= lo) return 0;
        if (v >= hi) return 1000;
        return static_cast<uint16_t>(
            static_cast<uint32_t>(v - lo) * 1000u / (hi - lo)
        );
    }

    uint16_t _rawADC(uint8_t i) const {
        if (!_adc) return 0;
        uint8_t ch = _pins[i] & 0x07u; // bits[2:0] = ADC channel (PC0–PC5 on ATmega328P)
        return _adc->read(ch);
    }
};

} // namespace MikroDuino
