/*
 * IRArray Basics — 3 Digital Sensors, read() and onLine() — MikroDuino
 * Module SDK
 *
 * The simplest possible use of the IRArray<N> module: three digital IR
 * reflectance sensors read as plain HIGH/LOW comparator outputs, printed
 * over USART every 200 ms. This is project 1 of 6 in the
 * examples/Modules/IRArray series, which walks IRArray<N> from a single
 * blocking digital read up to a capstone line-following robot dashboard
 * combining IRArray, PID, DCMotor, and Button.
 *
 * Hardware (ATmega328P @ 16 MHz, three digital IR reflectance sensors —
 * e.g. TCRT5000 modules with an onboard comparator, LEFT/CENTER/RIGHT):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ LEFT    │ PD2   │ Digital comparator output                  │
 *   │ CENTER  │ PD3   │ Digital comparator output                  │
 *   │ RIGHT   │ PD4   │ Digital comparator output                  │
 *   │ TXD     │ PD1   │ USB-serial adapter, 9600 8N1, for readings │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * Most digital IR comparator boards pull their output LOW when the
 * sensor is over a dark (line) surface and HIGH over a light background
 * — that's the "activeLow" convention IRArray defaults to, and the one
 * this project's wiring assumes. If your boards are wired the opposite
 * way, pass activeLow=false to the constructor instead.
 *
 * IRArray concepts introduced:
 *   - IRArray<N> — a template class; N (3 here) is fixed at compile time
 *     and must be in [2, 16]. The template parameter is what lets a
 *     single class body serve a 2-sensor array and a 16-sensor array
 *     with no runtime overhead for the ones you don't use.
 *   - IRArray(pins[N], mode, activeLow, adc) — this project uses only
 *     the first two constructor arguments; DIGITAL is the default mode
 *     and no ADCDriver is needed for digital sensors.
 *   - begin() — in DIGITAL mode, configures every sensor pin as an input
 *     with the internal pull-up enabled. No calibration step exists (or
 *     is needed) for digital sensors.
 *   - read(out[N]) — fills a caller-provided array with one normalised
 *     value per sensor: 0 means "sees background", 1000 means "sees
 *     line". Digital mode can only ever report one of those two values
 *     (no in-between reading is possible from a comparator output).
 *   - onLine() — true if ANY sensor currently reads over the line. A
 *     handy one-shot summary when you don't need each sensor's
 *     individual state, used here to print a LOST warning when all
 *     three sensors leave the line at once.
 */

#include <util/delay.h>
#include <mikroduino/usart.hpp>
#include <IRArray.hpp>

using namespace MikroDuino;

static constexpr uint8_t SENSOR_COUNT = 3;
static const uint8_t SENSOR_PINS[SENSOR_COUNT] = { PD2, PD3, PD4 };

// DIGITAL mode (default), activeLow=true (default) — matches most
// off-the-shelf digital IR comparator boards.
IRArray<SENSOR_COUNT> irArray(SENSOR_PINS);

int main() {
    USART0.begin(9600);
    USART0.writeLine_P(PSTR("IRArray basic digital read"));
    USART0.writeLine_P(PSTR("=========================="));
    USART0.writeLine_P(PSTR("L / C / R sensor state, printed every 200 ms"));
    USART0.writeLine_P(PSTR(""));

    irArray.begin();

    while (true) {
        uint16_t readings[SENSOR_COUNT];
        irArray.read(readings);   // 0 = background, 1000 = line, per sensor

        USART0.write_P(PSTR("L="));
        USART0.writeInt(static_cast<int32_t>(readings[0]));
        USART0.write_P(PSTR("  C="));
        USART0.writeInt(static_cast<int32_t>(readings[1]));
        USART0.write_P(PSTR("  R="));
        USART0.writeInt(static_cast<int32_t>(readings[2]));

        if (irArray.onLine()) {
            USART0.writeLine_P(PSTR("   [on line]"));
        } else {
            USART0.writeLine_P(PSTR("   [LOST - no sensor sees the line]"));
        }

        _delay_ms(200);
    }
}
