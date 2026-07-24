/*
 * DHT22 Retry Logic + Unit Conversion — MikroDuino Module SDK
 *
 * Project 2 of 6 in the examples/Modules/DHT22 series. DHT22Driver's
 * read() has no built-in retry — a single timeout or checksum failure
 * just comes back with valid=false, full stop. This project builds a
 * small retry loop entirely out of application code on top of that one
 * primitive, plus a Celsius-to-Fahrenheit conversion and a running
 * success-rate counter, all ordinary arithmetic on the DHT22Data struct
 * project 1 introduced.
 *
 * Hardware: identical to project 1 — DHT22 DATA on PD4 (with its
 * pull-up resistor), USART0 TX on PD1 at 9600 8N1.
 *
 * Retry design: a single occasional invalid read is normal and not
 * worth alarming over (the datasheet itself allows for it), but several
 * in a row usually means a real problem — a marginal pull-up, an
 * over-long wire, or a genuinely disconnected sensor. This project
 * tries up to MAX_ATTEMPTS times per cycle, waiting RETRY_DELAY_MS
 * between attempts (itself comfortably over the sensor's minimum 2 s
 * spacing — see project 1), and only reports a full cycle as failed
 * once every attempt in it has failed.
 *
 * DHT22 concepts reused from project 1:
 *   - DHT22Driver(pin), read(), DHT22Data{humidity, temperature, valid}.
 *
 * New, non-DHT22 concepts introduced:
 *   - celsiusToFahrenheit() — plain float arithmetic on top of the
 *     driver's Celsius-only output; DHT22Driver itself has no unit
 *     concept at all, it just reports whatever the datasheet's raw
 *     encoding decodes to.
 *   - A rolling attempt/success counter, printed each cycle as a
 *     percentage — the kind of simple reliability metric worth keeping
 *     on any sensor that occasionally misses a beat.
 */

#include <util/delay.h>
#include <mikroduino/usart.hpp>
#include <DHT22.hpp>

using namespace MikroDuino;

DHT22Driver sensor(PD4);

static constexpr uint8_t  MAX_ATTEMPTS    = 3;
static constexpr uint16_t RETRY_DELAY_MS  = 2500;   // >= DHT22's 2 s minimum spacing

static float celsiusToFahrenheit(float celsius) {
    return celsius * 1.8f + 32.0f;
}

int main() {
    USART0.begin(9600);
    USART0.writeLine_P(PSTR("DHT22 retry + Fahrenheit conversion"));
    USART0.writeLine_P(PSTR("====================================="));
    USART0.writeLine_P(PSTR(""));

    uint32_t cyclesTotal   = 0;
    uint32_t cyclesSucceeded = 0;

    while (true) {
        DHT22Data reading;
        bool ok = false;

        for (uint8_t attempt = 1; attempt <= MAX_ATTEMPTS; ++attempt) {
            reading = sensor.read();
            if (reading.valid) {
                ok = true;
                break;
            }
            USART0.write_P(PSTR("Attempt "));
            USART0.writeInt(attempt);
            USART0.writeLine_P(PSTR(" failed, retrying..."));
            _delay_ms(RETRY_DELAY_MS);
        }

        ++cyclesTotal;
        if (ok) ++cyclesSucceeded;

        if (ok) {
            float fahrenheit = celsiusToFahrenheit(reading.temperature);

            USART0.write_P(PSTR("Humidity: "));
            USART0.writeFloat(reading.humidity, 1);
            USART0.write_P(PSTR(" %RH   Temp: "));
            USART0.writeFloat(reading.temperature, 1);
            USART0.write_P(PSTR(" C ("));
            USART0.writeFloat(fahrenheit, 1);
            USART0.writeLine_P(PSTR(" F)"));
        } else {
            USART0.writeLine_P(PSTR("All attempts failed this cycle."));
        }

        uint32_t successPct = (cyclesSucceeded * 100u) / cyclesTotal;
        USART0.write_P(PSTR("Success rate: "));
        USART0.writeInt(static_cast<int32_t>(successPct));
        USART0.write_P(PSTR("% ("));
        USART0.writeInt(static_cast<int32_t>(cyclesSucceeded));
        USART0.write_P(PSTR("/"));
        USART0.writeInt(static_cast<int32_t>(cyclesTotal));
        USART0.writeLine_P(PSTR(")"));
        USART0.writeLine_P(PSTR(""));

        // Only wait out the full inter-cycle spacing here if the loop
        // above didn't already spend it retrying.
        if (ok) _delay_ms(RETRY_DELAY_MS);
    }
}
