/*
 * HCSR04 + Button — Min/Max Distance Tracking With A Reset Button —
 * MikroDuino Module SDK
 *
 * Project 3 of 6 in the examples/Modules/HCSR04 series. Keeps a running
 * minimum and maximum distance since the last reset, and wires a button
 * to clear them back to "no data yet" on demand — the same clicked()
 * gesture examples/Modules/Button/01 introduced, reused here for a
 * sensor-logging task instead of an LED.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ TRIG    │ PD6   │ HCSR04 trigger (project 1)                │
 *   │ ECHO    │ PD7   │ HCSR04 echo (project 1)                    │
 *   │ Button  │ PD2   │ Other leg to GND — click resets min/max    │
 *   │ TXD     │ PD1   │ USB-serial adapter, 9600 8N1                │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * Timing model: like examples/Modules/DHT22/03 (sensor + button), this
 * project has no genuinely concurrent animation competing for the CPU,
 * so it keeps the simple `button.update(); _delay_ms(1);` busy-wait
 * from examples/Modules/Button/01-02 rather than reaching for a
 * millis() clock. A tick counter gates the (blocking, sub-millisecond
 * to a few-millisecond) HCSR04 ping to roughly once every 150 ms, while
 * the button is still polled every single millisecond in between.
 *
 * HCSR04 concepts reused from projects 1-2:
 *   - HCSR04(trigPin, echoPin), begin(), measureCM(); 0 means no echo
 *     this cycle and is excluded from min/max the same way project 2
 *     excluded it from the median filter.
 *
 * Button concepts reused from Button project 1:
 *   - Button(pin), begin(), update(), clicked().
 *
 * Min/max bookkeeping is ordinary application logic, not part of either
 * module: a "have we seen a first valid reading yet" flag seeds the
 * initial min/max from that first reading (rather than from an
 * arbitrary sentinel like 0, which would silently corrupt the
 * comparison — every real reading would look "further" than a
 * sentinel of 0), and every later valid reading just widens the range.
 */

#include <util/delay.h>
#include <mikroduino/usart.hpp>
#include <Button.hpp>
#include <HCSR04.hpp>

using namespace MikroDuino;

HCSR04 sonar(PD6, PD7);
Button resetButton(PD2);

static bool    haveData = false;
static int32_t distanceMin, distanceMax;

static void resetMinMax() {
    haveData = false;
    USART0.writeLine_P(PSTR("Min/max reset."));
}

static void recordReading(int32_t cm) {
    if (!haveData) {
        distanceMin = distanceMax = cm;
        haveData = true;
        return;
    }
    if (cm < distanceMin) distanceMin = cm;
    if (cm > distanceMax) distanceMax = cm;
}

int main() {
    USART0.begin(9600);
    USART0.writeLine_P(PSTR("HCSR04 min/max tracking (click button to reset)"));
    USART0.writeLine_P(PSTR("================================================="));
    USART0.writeLine_P(PSTR(""));

    sonar.begin();
    resetButton.begin();

    uint16_t tickMs = 0;
    static constexpr uint16_t SAMPLE_PERIOD_MS = 150;

    while (true) {
        resetButton.update();

        if (resetButton.clicked()) {
            resetMinMax();
        }

        if (++tickMs >= SAMPLE_PERIOD_MS) {
            tickMs = 0;

            uint32_t cm = sonar.measureCM();
            if (cm > 0) {
                recordReading(static_cast<int32_t>(cm));

                USART0.write_P(PSTR("Distance: "));
                USART0.writeInt(static_cast<int32_t>(cm));
                USART0.write_P(PSTR(" cm  (min "));
                USART0.writeInt(distanceMin);
                USART0.write_P(PSTR(" max "));
                USART0.writeInt(distanceMax);
                USART0.writeLine_P(PSTR(")"));
            } else {
                USART0.writeLine_P(PSTR("No echo this cycle - skipping"));
            }
        }

        _delay_ms(1);   // Button::update() expects to be called ~every 1 ms
    }
}
