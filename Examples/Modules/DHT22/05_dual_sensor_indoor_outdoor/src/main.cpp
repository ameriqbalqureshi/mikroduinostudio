/*
 * Two DHT22 Instances — Indoor/Outdoor Comparison — MikroDuino Module SDK
 *
 * Project 5 of 6 in the examples/Modules/DHT22 series. Wires up TWO
 * completely independent DHT22Driver objects on separate data pins and
 * reads both once per cycle, printing not just each sensor's own
 * readings but the DELTA between them — exactly the kind of two-sensor
 * comparison a real weather station or HVAC controller needs. Just like
 * examples/Modules/Button/05's two independent Button objects and
 * examples/Modules/DCMotor/05's two independent DCMotor objects,
 * nothing about using two DHT22Driver instances at once is special —
 * each one is a self-contained object over its own pin, and this
 * project exists mainly to make that concrete.
 *
 * Hardware (ATmega328P @ 16 MHz, two independent DHT22/AM2302 sensors):
 *
 *   ┌───────────────┬───────┬──────────────────────────────────────┐
 *   │ INDOOR DATA   │ PD4   │ Data line + pull-up (project 1)        │
 *   │ OUTDOOR DATA  │ PD5   │ Data line + pull-up, own sensor         │
 *   │ TXD           │ PD1   │ USB-serial adapter, 9600 8N1            │
 *   └───────────────┴───────┴──────────────────────────────────────┘
 *
 * Because each sensor has its own dedicated data pin, there's no bus
 * contention to worry about the way a shared bus (I2C, 1-Wire with
 * multiple devices) would need — the two read() calls are simply run
 * back-to-back, each a completely independent single-wire handshake on
 * its own pin. The only shared constraint is the same 2 s-minimum
 * spacing project 1 established, which applies to EACH sensor
 * independently — reading indoor then outdoor back-to-back inside one
 * cycle is fine; it's calling read() on the SAME sensor object again
 * too soon that would risk an invalid result.
 *
 * DHT22 concepts reused from projects 1-4:
 *   - DHT22Driver(pin), read(), DHT22Data{humidity, temperature, valid}
 *     — used identically on two separate instances.
 *
 * Delta math is ordinary application code: simple subtraction, printed
 * with an explicit sign so "warmer/colder" and "more/less humid" read
 * naturally without needing separate branches for each direction.
 */

#include <util/delay.h>
#include <mikroduino/usart.hpp>
#include <DHT22.hpp>

using namespace MikroDuino;

DHT22Driver indoor(PD4);
DHT22Driver outdoor(PD5);

static void printSigned(float value, uint8_t decimals) {
    if (value >= 0.0f) USART0.write_P(PSTR("+"));
    USART0.writeFloat(value, decimals);
}

int main() {
    USART0.begin(9600);
    USART0.writeLine_P(PSTR("DHT22 dual-sensor indoor/outdoor comparison"));
    USART0.writeLine_P(PSTR("============================================="));
    USART0.writeLine_P(PSTR(""));

    while (true) {
        DHT22Data in  = indoor.read();
        DHT22Data out = outdoor.read();

        if (in.valid) {
            USART0.write_P(PSTR("Indoor:  "));
            USART0.writeFloat(in.temperature, 1);
            USART0.write_P(PSTR("C  "));
            USART0.writeFloat(in.humidity, 1);
            USART0.writeLine_P(PSTR("%RH"));
        } else {
            USART0.writeLine_P(PSTR("Indoor:  read failed"));
        }

        if (out.valid) {
            USART0.write_P(PSTR("Outdoor: "));
            USART0.writeFloat(out.temperature, 1);
            USART0.write_P(PSTR("C  "));
            USART0.writeFloat(out.humidity, 1);
            USART0.writeLine_P(PSTR("%RH"));
        } else {
            USART0.writeLine_P(PSTR("Outdoor: read failed"));
        }

        if (in.valid && out.valid) {
            float tempDelta = in.temperature - out.temperature;
            float humDelta  = in.humidity - out.humidity;

            USART0.write_P(PSTR("Delta (indoor - outdoor): "));
            printSigned(tempDelta, 1);
            USART0.write_P(PSTR("C, "));
            printSigned(humDelta, 1);
            USART0.writeLine_P(PSTR("%RH"));
        } else {
            USART0.writeLine_P(PSTR("Delta: unavailable (one or both reads failed)"));
        }

        USART0.writeLine_P(PSTR(""));
        _delay_ms(2500);   // >= DHT22's 2 s minimum spacing, applies per-sensor
    }
}
