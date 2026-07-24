/*
 * IRSensor Basics — Digital Proximity Polling — MikroDuino Module SDK
 *
 * The simplest possible use of the IRSensor module: one digital IR
 * comparator board (FC-51, TCRT5000-with-comparator, E18-D80NK) polled in
 * a plain blocking loop, printed over USART. This is project 1 of 6 in
 * the examples/Modules/IRSensor series, which walks IRSensor from a single
 * blocking digital read up to a capstone obstacle-avoiding robot combining
 * IRSensor, DCMotor, Button, and StrBuilder.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ IR OUT  │ PD2   │ Digital comparator output of the sensor   │
 *   │ LED     │ PB5   │ Lit while an object is detected           │
 *   │ TXD     │ PD1   │ USB-serial adapter, 9600 8N1, for readings│
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * Most digital IR comparator boards pull their output LOW when an object
 * is in range and HIGH when the beam is clear — that is the "activeLow"
 * convention IRSensor defaults to, and the one this project's wiring
 * assumes. If your board is wired the opposite way, pass activeLow=false
 * to the constructor instead.
 *
 * IRSensor concepts introduced:
 *   - IRSensor(pin, mode, activeLow, threshold, adc) — this project uses
 *     only the first argument; DIGITAL is the default mode, activeLow
 *     defaults to true, and threshold/adc are ANALOG-mode-only (project 2
 *     introduces them).
 *   - begin() — in DIGITAL mode, configures the pin as an input with the
 *     internal pull-up enabled. No calibration step exists for digital
 *     sensors.
 *   - detected() — true when an object is within range, already adjusted
 *     for activeLow polarity so callers never touch raw pin logic.
 *   - readRaw() — in DIGITAL mode returns exactly 0 or 1 (the polarity-
 *     adjusted logical reading); ANALOG mode's 0-1023 range is project 2's
 *     topic.
 */

#include <util/delay.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/usart.hpp>
#include <IRSensor.hpp>

using namespace MikroDuino;

static constexpr uint8_t LED = PB5;

// DIGITAL mode (default), activeLow=true (default) — matches most
// off-the-shelf digital IR comparator boards.
IRSensor ir(PD2);

int main() {
    USART0.begin(9600);
    USART0.writeLine_P(PSTR("IRSensor basic digital polling"));
    USART0.writeLine_P(PSTR("==============================="));
    USART0.writeLine_P(PSTR("raw / detected state, printed every 200 ms"));
    USART0.writeLine_P(PSTR(""));

    GPIO::output(LED);
    GPIO::clear(LED);

    ir.begin();

    while (true) {
        bool     isDetected = ir.detected();
        uint16_t raw        = ir.readRaw();

        USART0.write_P(PSTR("raw="));
        USART0.writeInt(static_cast<int32_t>(raw));

        if (isDetected) {
            USART0.writeLine_P(PSTR("  [OBJECT DETECTED]"));
            GPIO::set(LED);
        } else {
            USART0.writeLine_P(PSTR("  [clear]"));
            GPIO::clear(LED);
        }

        _delay_ms(200);
    }
}
