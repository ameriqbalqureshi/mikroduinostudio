/*
 * Pulse — Period and Frequency Measurement — MikroDuino Module SDK
 *
 * Project 4 of 6 in the examples/Modules/Pulse series. Project 3 measured
 * a single pulse's WIDTH. This project measures the SIGNAL as a whole —
 * period() (time between two consecutive same-type edges) and
 * frequency() (edges counted over a fixed window) — on the same
 * DCMotor-generated loopback signal, and cross-checks the two against
 * each other: frequency should equal 1,000,000 / period to within the
 * measurement's own jitter.
 *
 * Hardware — identical wiring to project 3:
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ GEN     │ PD6   │ OC0A — DCMotor PWM output (~977 Hz)        │
 *   │ MEAS    │ PD7   │ PulseMeter input — jumper this to GEN       │
 *   │ TXD     │ PD1   │ USB-serial adapter, 9600 8N1, for readings │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * DCMotor's PWM carrier frequency is FIXED by its timer configuration —
 * changing speed(pct) changes the pulse WIDTH (project 3), never the
 * period between cycles. This project holds duty at a constant 50% and
 * focuses entirely on period()/frequency() themselves.
 *
 * Pulse concepts introduced:
 *   - period(risingEdge, timeoutUs) — measures a FULL CYCLE: time from
 *     one edge to the next SAME-type edge (both rising by default). This
 *     is the general case pulseWidth() is a special case of — pulseWidth
 *     times an ON-phase only, period times an entire ON+OFF cycle.
 *   - frequency(windowUs) — a completely different measurement strategy:
 *     rather than timing between two edges, it COUNTS rising edges over a
 *     fixed time window and divides. Longer windows trade responsiveness
 *     for accuracy (see its own header comment: ~50 kHz is the practical
 *     ceiling, GPIO-poll-rate limited) — this project uses the default
 *     100 ms window, long enough to catch ~97 cycles of our ~977 Hz
 *     signal per call.
 *   - Cross-checking two independently-implemented measurements of the
 *     same underlying signal against one another (period-derived Hz vs.
 *     frequency()'s own Hz) is a real diagnostic technique, not just a
 *     demo flourish — persistent disagreement between the two would
 *     point at a wiring or noise problem project 3's single-pulse check
 *     might not reveal.
 */

#include <util/delay.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/usart.hpp>
#include <Pulse.hpp>
#include <DCMotor.hpp>

using namespace MikroDuino;

static constexpr uint8_t GEN_PIN  = PD6;   // OC0A
static constexpr uint8_t MEAS_PIN = PD7;

DCMotor    gen(PB0, PB1, GEN_PIN);   // in1/in2 unused - see project 3's header comment
PulseMeter pm(MEAS_PIN);

int main() {
    USART0.begin(9600);
    USART0.writeLine_P(PSTR("Pulse: period and frequency signal analyzer"));
    USART0.writeLine_P(PSTR("============================================"));
    USART0.writeLine_P(PSTR("Jumper GEN (PD6) to MEAS (PD7)"));
    USART0.writeLine_P(PSTR(""));

    gen.begin();
    gen.speed(50);   // fixed 50% duty; only period/frequency are under test here
    pm.begin();

    while (true) {
        uint32_t periodUs = pm.period(true, 1000000UL);   // default 1 s timeout

        USART0.write_P(PSTR("period="));
        if (periodUs == 0) {
            USART0.writeLine_P(PSTR("TIMEOUT (check the GEN-to-MEAS jumper)"));
        } else {
            USART0.writeInt(static_cast<int32_t>(periodUs));
            USART0.write_P(PSTR("us  (="));
            USART0.writeInt(static_cast<int32_t>(1000000UL / periodUs));
            USART0.write_P(PSTR("Hz)"));

            uint32_t measuredHz = pm.frequency(100000UL);   // 100 ms window
            USART0.write_P(PSTR("   frequency()="));
            USART0.writeInt(static_cast<int32_t>(measuredHz));
            USART0.writeLine_P(PSTR("Hz"));
        }

        _delay_ms(500);
    }
}
