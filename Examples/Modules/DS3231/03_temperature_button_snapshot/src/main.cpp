/*
 * DS3231 + Button — Temperature Reading + On-Demand Snapshot —
 * MikroDuino Module SDK
 *
 * Project 3 of 6 in the examples/Modules/DS3231 series. Adds the
 * DS3231's built-in die temperature sensor, and wires a button so a
 * full time+date+temperature "snapshot" can be printed on demand — the
 * same clicked() gesture examples/Modules/Button/01 introduced, reused
 * here for an RTC logging task instead of an LED.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ SDA     │ PC4   │ DS3231 (projects 1-2)                      │
 *   │ SCL     │ PC5   │                                            │
 *   │ Button  │ PD2   │ Other leg to GND — click prints a snapshot │
 *   │ TXD     │ PD1   │ USB-serial adapter, 9600 8N1               │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * Timing model: like examples/Modules/DHT22/03 (sensor + button), this
 * project has no genuinely concurrent animation competing for the CPU,
 * so it keeps the simple `button.update(); _delay_ms(1);` busy-wait
 * from examples/Modules/Button/01-02 rather than reaching for a
 * millis() clock. A tick counter still auto-prints the time once a
 * second in the background, in addition to whatever the button
 * triggers — both share the same loop, they just run on different
 * periods measured off the same 1 ms tick.
 *
 * DS3231 concepts reused from projects 1-2:
 *   - DS3231(), begin(), isRunning(), now(), set() (only if halted).
 *
 * DS3231 concept introduced:
 *   - temperatureRaw() / temperature() — the DS3231's built-in
 *     temperature-compensation sensor (the same measurement the chip
 *     uses internally to correct its own oscillator drift), exposed
 *     for free. temperatureRaw() returns a signed count in 0.25 °C
 *     units straight off the two raw registers; temperature() is just
 *     `temperatureRaw() * 0.25f` for convenience. IMPORTANT: per the
 *     datasheet, the DS3231 only refreshes this measurement once every
 *     64 seconds internally — calling temperature() more often than
 *     that just re-reads the same cached value, unlike now(), which
 *     reflects the live running clock on every single call.
 *
 * Button concepts reused from Button project 1:
 *   - Button(pin), begin(), update(), clicked().
 */

#include <util/delay.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/usart.hpp>
#include <mikroduino/i2c.hpp>
#include <Button.hpp>
#include <DS3231.hpp>

using namespace MikroDuino;

DS3231 rtc;
Button  snapshotButton(PD2);

static void printField2(uint8_t v) {
    USART0.write(static_cast<char>('0' + (v / 10)));
    USART0.write(static_cast<char>('0' + (v % 10)));
}

static void printTime(const DateTime& now) {
    printField2(now.hour);
    USART0.write_P(PSTR(":"));
    printField2(now.minute);
    USART0.write_P(PSTR(":"));
    printField2(now.second);
}

static void printSnapshot() {
    DateTime now = rtc.now();
    float temp = rtc.temperature();

    USART0.write_P(PSTR(">>> SNAPSHOT  20"));
    printField2(now.year);
    USART0.write_P(PSTR("-"));
    printField2(now.month);
    USART0.write_P(PSTR("-"));
    printField2(now.day);
    USART0.write_P(PSTR("  "));
    printTime(now);
    USART0.write_P(PSTR("  "));
    USART0.writeFloat(temp, 2);
    USART0.writeLine_P(PSTR(" C"));
}

int main() {
    GPIO::inputPullup(PC4);
    GPIO::inputPullup(PC5);

    USART0.begin(9600);
    USART0.writeLine_P(PSTR("DS3231 temperature + button snapshot"));
    USART0.writeLine_P(PSTR("======================================"));
    USART0.writeLine_P(PSTR("Click the button anytime for a full snapshot."));
    USART0.writeLine_P(PSTR(""));

    I2C.beginMaster(100000UL);
    rtc.begin();
    snapshotButton.begin();

    uint16_t tickMs = 0;

    while (true) {
        snapshotButton.update();

        if (snapshotButton.clicked()) {
            printSnapshot();
        }

        if (++tickMs >= 1000) {
            tickMs = 0;
            USART0.write_P(PSTR("Time: "));
            printTime(rtc.now());
            USART0.writeLine_P(PSTR(""));
        }

        _delay_ms(1);   // Button::update() expects to be called ~every 1 ms
    }
}
