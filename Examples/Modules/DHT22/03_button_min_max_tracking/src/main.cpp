/*
 * DHT22 + Button — Min/Max Tracking With A Reset Button — MikroDuino
 * Module SDK
 *
 * Project 3 of 6 in the examples/Modules/DHT22 series. Keeps a running
 * minimum and maximum for both humidity and temperature since the last
 * reset, and wires a button to clear them back to "no data yet" on
 * demand — the same clicked() gesture examples/Modules/Button/01
 * introduced, reused here for a sensor-logging task instead of an LED.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ DATA    │ PD4   │ DHT22 data line + pull-up (project 1)     │
 *   │ Button  │ PD2   │ Other leg to GND — click resets min/max   │
 *   │ TXD     │ PD1   │ USB-serial adapter, 9600 8N1               │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * Timing model: like examples/Modules/DCMotor/03 (pot + button), this
 * project has no genuinely concurrent animation competing for the CPU,
 * so it keeps the simple `button.update(); _delay_ms(1);` busy-wait from
 * examples/Modules/Button/01-02 rather than reaching for a millis()
 * clock. A tick counter gates the (blocking, few-millisecond) DHT22
 * read to roughly once every 2.5 s, the same spacing project 1
 * established, while the button is still polled every single
 * millisecond in between — the occasional few extra milliseconds a
 * read() call blocks for is a rounding error against a 2.5 s period.
 *
 * DHT22 concepts reused from projects 1-2:
 *   - DHT22Driver(pin), read(), DHT22Data{humidity, temperature, valid}.
 *
 * Button concepts reused from Button project 1:
 *   - Button(pin), begin(), update(), clicked().
 *
 * Min/max bookkeeping is ordinary application logic, not part of either
 * module: a "have we seen a first valid reading yet" flag seeds the
 * initial min/max from that first reading (rather than from an
 * arbitrary sentinel like 0 or -40, which would silently corrupt the
 * comparison), and every later valid reading just widens the range.
 */

#include <util/delay.h>
#include <mikroduino/usart.hpp>
#include <Button.hpp>
#include <DHT22.hpp>

using namespace MikroDuino;

DHT22Driver sensor(PD4);
Button      resetButton(PD2);

static bool  haveData = false;
static float humidityMin, humidityMax;
static float temperatureMin, temperatureMax;

static void resetMinMax() {
    haveData = false;
    USART0.writeLine_P(PSTR("Min/max reset."));
}

static void recordReading(const DHT22Data& r) {
    if (!haveData) {
        humidityMin = humidityMax = r.humidity;
        temperatureMin = temperatureMax = r.temperature;
        haveData = true;
        return;
    }
    if (r.humidity < humidityMin) humidityMin = r.humidity;
    if (r.humidity > humidityMax) humidityMax = r.humidity;
    if (r.temperature < temperatureMin) temperatureMin = r.temperature;
    if (r.temperature > temperatureMax) temperatureMax = r.temperature;
}

int main() {
    USART0.begin(9600);
    USART0.writeLine_P(PSTR("DHT22 min/max tracking (click button to reset)"));
    USART0.writeLine_P(PSTR("================================================"));
    USART0.writeLine_P(PSTR(""));

    resetButton.begin();

    uint16_t tickMs = 0;
    static constexpr uint16_t SAMPLE_PERIOD_MS = 2500;

    while (true) {
        resetButton.update();

        if (resetButton.clicked()) {
            resetMinMax();
        }

        if (++tickMs >= SAMPLE_PERIOD_MS) {
            tickMs = 0;

            DHT22Data reading = sensor.read();
            if (reading.valid) {
                recordReading(reading);

                USART0.write_P(PSTR("H: "));
                USART0.writeFloat(reading.humidity, 1);
                USART0.write_P(PSTR("% (min "));
                USART0.writeFloat(humidityMin, 1);
                USART0.write_P(PSTR(" max "));
                USART0.writeFloat(humidityMax, 1);
                USART0.writeLine_P(PSTR(")"));

                USART0.write_P(PSTR("T: "));
                USART0.writeFloat(reading.temperature, 1);
                USART0.write_P(PSTR("C (min "));
                USART0.writeFloat(temperatureMin, 1);
                USART0.write_P(PSTR(" max "));
                USART0.writeFloat(temperatureMax, 1);
                USART0.writeLine_P(PSTR(")"));
                USART0.writeLine_P(PSTR(""));
            } else {
                USART0.writeLine_P(PSTR("Read failed - skipping this cycle"));
            }
        }

        _delay_ms(1);   // Button::update() expects to be called ~every 1 ms
    }
}
