/*
 * IRArray Five-Sensor Line Position — linePosition() and allOnLine() —
 * MikroDuino Module SDK
 *
 * Project 2 of 6 in the examples/Modules/IRArray series. Widens project
 * 1's three-sensor array to five and switches from "does ANY sensor see
 * the line" to a single continuous number describing WHERE across the
 * array the line currently sits — the core measurement every real line
 * follower steers on.
 *
 * Hardware (ATmega328P @ 16 MHz, five digital IR reflectance sensors in
 * a row, evenly spaced across the front of a line-following chassis):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ Sensor0 │ PD2   │ Far left                                    │
 *   │ Sensor1 │ PD3   │ Left-of-centre                              │
 *   │ Sensor2 │ PD4   │ Centre                                      │
 *   │ Sensor3 │ PD5   │ Right-of-centre                             │
 *   │ Sensor4 │ PD6   │ Far right                                   │
 *   │ TXD     │ PD1   │ USB-serial adapter, 9600 8N1                │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * IRArray concepts introduced:
 *   - linePosition() — a weighted centre-of-mass over all N sensors,
 *     scaled so sensor 0 reads 0, sensor (N-1) reads (N-1)*1000, and
 *     anywhere the line straddles two sensors interpolates smoothly
 *     between them. For N=5 that range is 0..4000, with 2000 being
 *     dead-centre under sensor2 — the "on target" value a steering loop
 *     tries to hold. It returns exactly -1 when NO sensor sees the line
 *     at all (weightSum == 0 inside the module), which this project
 *     treats as a distinct LOST state rather than a real position.
 *   - allOnLine() — true only when EVERY sensor reads over the line at
 *     once. On a normal line follower this almost never happens by
 *     accident — it usually means the robot has reached a solid stop
 *     bar or a wide junction marker, which is why real line-following
 *     robots (see project 6) treat it as an event distinct from "still
 *     tracking a normal line", not just "very well centred".
 *
 * IRArray concepts reused from project 1:
 *   - IRArray<N>(pins), begin() — identical usage, just N=5 instead of
 *     N=3, and the constructor's activeLow default (true) still matches
 *     this project's comparator boards.
 *
 * Steering-hint logic (this project's own code, not part of IRArray):
 *   CENTER_POS = (N-1)*1000/2 = 2000, the value linePosition() reports
 *   when the line sits exactly under the middle sensor. This project
 *   compares linePosition() against CENTER_POS with a small deadband —
 *   the same "how far off target, and which way" comparison a
 *   proportional or PID steering loop (see projects 5-6) performs every
 *   control cycle, just printed as a word here instead of driving a
 *   motor.
 */

#include <util/delay.h>
#include <mikroduino/usart.hpp>
#include <IRArray.hpp>

using namespace MikroDuino;

static constexpr uint8_t SENSOR_COUNT = 5;
static const uint8_t SENSOR_PINS[SENSOR_COUNT] = { PD2, PD3, PD4, PD5, PD6 };

IRArray<SENSOR_COUNT> irArray(SENSOR_PINS);

// Centre-of-mass value linePosition() reports when the line is exactly
// under the middle sensor: (N-1)*1000/2.
static constexpr int16_t CENTER_POS = (SENSOR_COUNT - 1) * 1000 / 2;

// How far from CENTER_POS still counts as "close enough" to call it centred.
static constexpr int16_t DEADBAND = 300;

int main() {
    USART0.begin(9600);
    USART0.writeLine_P(PSTR("IRArray five-sensor line position"));
    USART0.writeLine_P(PSTR("=================================="));
    USART0.write_P(PSTR("Centre position = "));
    USART0.writeInt(CENTER_POS);
    USART0.writeLine_P(PSTR(""));

    irArray.begin();

    while (true) {
        int16_t pos = irArray.linePosition();

        if (pos < 0) {
            USART0.writeLine_P(PSTR("pos=  --  [LOST - no sensor sees the line]"));
        } else {
            USART0.write_P(PSTR("pos="));
            USART0.writeInt(pos);

            int16_t error = static_cast<int16_t>(pos - CENTER_POS);

            if (irArray.allOnLine()) {
                USART0.writeLine_P(PSTR("   [ALL ON LINE - stop bar / wide marker?]"));
            } else if (error > DEADBAND) {
                USART0.writeLine_P(PSTR("   -> steer RIGHT"));
            } else if (error < -DEADBAND) {
                USART0.writeLine_P(PSTR("   -> steer LEFT"));
            } else {
                USART0.writeLine_P(PSTR("   -> CENTER, on target"));
            }
        }

        _delay_ms(150);
    }
}
