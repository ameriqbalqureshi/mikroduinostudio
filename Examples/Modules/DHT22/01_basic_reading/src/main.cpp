/*
 * DHT22 Basics — Blocking read(), Validity Checking — MikroDuino Module SDK
 *
 * The simplest possible use of the DHT22 module: read temperature and
 * humidity off a single sensor and print them over USART. This is
 * project 1 of 6 in the examples/Modules/DHT22 series, which walks the
 * DHT22Driver class from a single blocking read() up to a capstone
 * datalogger dashboard combining DHT22, LCD, Button, and EEPROM.
 *
 * Hardware (ATmega328P @ 16 MHz, DHT22/AM2302 temperature+humidity
 * sensor):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ Signal  │ Pin   │ Wiring                                    │
 *   ├─────────┼───────┼──────────────────────────────────────────┤
 *   │ DATA    │ PD4   │ Sensor's single data line. Also needs a   │
 *   │         │       │ 4.7k-10k resistor from DATA to VCC — many │
 *   │         │       │ breakout boards already carry this pull-  │
 *   │         │       │ up on-board, a bare AM2302 sensor does not │
 *   │ VCC     │ —     │ 3.3-5.5V                                   │
 *   │ GND     │ —     │ Ground                                    │
 *   │ TXD     │ PD1   │ USB-serial adapter, 9600 8N1, for readings │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * DHT22 concepts introduced:
 *   - DHT22Driver(pin) — the entire constructor: it just records which
 *     pin the sensor's DATA line is on. Unlike every other module in
 *     this SDK so far (CharLCD, Button, DCMotor all have begin()),
 *     DHT22Driver has NO begin() — there's no persistent hardware state
 *     to configure ahead of time, because read() reconfigures the pin's
 *     direction itself, every single call (see DHT22.cpp: it drives the
 *     pin as an output to send the start signal, then immediately
 *     switches it to an input with pull-up to listen for the sensor's
 *     response). Nothing needs to happen before the first read().
 *   - read() — a BLOCKING call that runs the full single-wire handshake:
 *     pull DATA low for 1 ms, release it, wait for the sensor's
 *     acknowledgement pulses, then clock in 40 bits (16 humidity + 16
 *     temperature + 8 checksum). The whole exchange takes a few
 *     milliseconds and ties up the CPU for its duration — there is no
 *     interrupt-driven or non-blocking version in this driver.
 *   - DHT22Data — the plain struct read() returns: humidity (%RH,
 *     0.0-100.0), temperature (°C, -40.0-80.0), and valid (bool).
 *   - valid — MUST be checked before trusting humidity/temperature.
 *     read() sets it false and leaves both readings at 0.0 whenever the
 *     handshake times out or the checksum doesn't match — which happens
 *     occasionally on real hardware from line noise or being polled too
 *     often (see below), not just on a wiring fault. This project simply
 *     skips printing on an invalid read; project 2 adds an actual retry.
 *
 * Critical timing rule this whole series depends on: the DHT22 datasheet
 * requires AT LEAST 2 seconds between the start of one read() and the
 * start of the next. The driver does not enforce this itself — calling
 * read() faster than that is very likely to return invalid data, or
 * occasionally stale data repeated from the sensor's own internal
 * sample. This project waits 2.5 seconds between reads to stay safely
 * clear of that limit.
 */

#include <util/delay.h>
#include <mikroduino/usart.hpp>
#include <DHT22.hpp>

using namespace MikroDuino;

DHT22Driver sensor(PD4);

int main() {
    USART0.begin(9600);
    USART0.writeLine_P(PSTR("DHT22 basic reading"));
    USART0.writeLine_P(PSTR("===================="));
    USART0.writeLine_P(PSTR(""));

    while (true) {
        DHT22Data reading = sensor.read();

        if (reading.valid) {
            USART0.write_P(PSTR("Humidity: "));
            USART0.writeFloat(reading.humidity, 1);
            USART0.write_P(PSTR(" %RH   Temperature: "));
            USART0.writeFloat(reading.temperature, 1);
            USART0.writeLine_P(PSTR(" C"));
        } else {
            USART0.writeLine_P(PSTR("Read failed (timeout or bad checksum) - skipping"));
        }

        // Datasheet minimum is 2 s between reads; 2.5 s leaves margin.
        _delay_ms(2500);
    }
}
