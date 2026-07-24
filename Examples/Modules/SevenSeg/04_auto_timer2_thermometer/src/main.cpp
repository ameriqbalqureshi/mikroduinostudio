/*
 * SevenSeg Auto-Refresh Thermometer — beginTimer2() + ADC + printFloat() —
 * MikroDuino Module SDK
 *
 * Project 4 of 6 in the examples/Modules/SevenSeg series. Projects 2
 * and 3 each hand-rolled their own refresh schedule (a blocking
 * _delay_ms() loop, then a hand-configured Timer1 ISR). This project
 * uses SevenSegMux's THIRD and most convenient refresh strategy —
 * beginTimer2() — which configures Timer2 and its overflow interrupt
 * for you and calls refresh() automatically forever after, leaving the
 * main loop completely free for the potentiometer-thermometer logic
 * below.
 *
 * Hardware (ATmega328P @ 16 MHz, same 4-digit shared-bus wiring as
 * projects 2-3, plus a potentiometer standing in for an analog
 * temperature sensor):
 *
 *   ┌──────────────┬───────┬──────────────────────────────────────────┐
 *   │ Segment a-g  │ PD0-6 │ Shared bus (see project 2's transistor      │
 *   │              │       │ note — applies here too)                    │
 *   │ Digit 0-3    │ PB0-3 │ Same as projects 2-3                         │
 *   │ ADC0         │ PC0   │ Potentiometer wiper; outer legs to 5V/GND.  │
 *   │              │       │ Stands in for a real analog sensor (e.g.    │
 *   │              │       │ an LM35, 10 mV/deg C) so this project runs   │
 *   │              │       │ on the same bare breadboard as projects 1-3 │
 *   │              │       │ without extra hardware.                     │
 *   └──────────────┴───────┴──────────────────────────────────────────┘
 *
 * SevenSegMux<N> concepts reused from projects 2-3:
 *   - SevenSegMux<4>(segPins, digitPins), begin().
 *
 * New SevenSegMux<N> concepts:
 *   - beginTimer2(prescaler) — configures Timer2 in free-running Normal
 *     mode and enables its overflow interrupt to call refresh()
 *     automatically; sei() must still be called afterwards, exactly
 *     once, same as any other interrupt-driven peripheral. Default
 *     prescaler 64 gives ~1 ms per overflow at 16 MHz, so with N=4
 *     digits each one refreshes at ~250 Hz — comfortably above the
 *     flicker threshold with no manual scheduling at all. See
 *     SevenSeg.hpp's header comment for the Timer2-conflict note: this
 *     mode cannot be combined with IRRemote::begin() or DCMotor on
 *     OC2A/OC2B, since both also claim TIMER2_OVF_vect.
 *   - printFloat(val, decimals) — formats a float with the given
 *     number of fractional digits (default 1) and places the decimal
 *     point on the correct digit automatically via setDP() internally.
 *     Needs N >= 2; this project's N=4 easily fits e.g. "23.5" across
 *     3 visible digits with one leading blank.
 *
 * Reading the "sensor": ADC_Driver.read(0) (mikroduino/adc.hpp, same
 * blocking single-conversion call examples/Modules/LCD/05 used) gives
 * a raw 0-1023 result, linearly mapped across the whole pot travel to
 * a 0.0-50.0 deg C range purely for a readable demo — swap
 * mapAdcToCelsius() for whatever conversion a real sensor's datasheet
 * specifies and everything downstream (printFloat, the refresh) stays
 * the same.
 */

#include <util/delay.h>
#include <avr/interrupt.h>
#include <mikroduino/adc.hpp>
#include <SevenSeg.hpp>

using namespace MikroDuino;

static const uint8_t SEGS[7]   = { PD0, PD1, PD2, PD3, PD4, PD5, PD6 };
static const uint8_t DIGITS[4] = { PB0, PB1, PB2, PB3 };

SevenSegMux<4> mx(SEGS, DIGITS);

// Maps a raw 10-bit ADC reading across the whole potentiometer travel
// onto a 0.0-50.0 deg C range, standing in for a real sensor's own
// datasheet formula.
static float mapAdcToCelsius(uint16_t raw) {
    return (static_cast<float>(raw) * 50.0f) / 1023.0f;
}

int main() {
    mx.begin();
    mx.beginTimer2();   // default prescaler 64 -> ~250 Hz per-digit refresh
    sei();

    ADC_Driver.begin();   // defaults: AVCC reference, DIV128 prescaler

    while (true) {
        uint16_t raw    = ADC_Driver.read(0);
        float    tempC  = mapAdcToCelsius(raw);

        mx.printFloat(tempC, 1);

        _delay_ms(200);   // readable update rate; the display itself
                           // keeps refreshing at ~250 Hz regardless
    }
}
