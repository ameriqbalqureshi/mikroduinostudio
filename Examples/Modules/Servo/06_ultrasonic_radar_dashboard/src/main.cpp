/*
 * Ultrasonic Radar Dashboard — Servo + HCSR04 + Button — MikroDuino
 * Module SDK (capstone)
 *
 * Project 6 of 6 in the examples/Modules/Servo series. A classic
 * beginner robotics build: an HC-SR04 ultrasonic sensor riding on a
 * servo horn, sweeping back and forth and sampling distance at every
 * angle, reporting each angle/distance pair like a radar sweep and
 * calling out the nearest obstacle found once each sweep completes.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ OC1A    │ PB1   │ Pan servo signal wire — Servo channel 0    │
 *   │ —       │ —     │ Servo external 5V supply + common GND (see │
 *   │         │       │ project 1's wiring note)                    │
 *   │ TRIG    │ PD6   │ HCSR04 trigger — mounted on the servo horn │
 *   │ ECHO    │ PD7   │ HCSR04 echo                                 │
 *   │ SW      │ PD4   │ Push button to GND, internal pull-up —     │
 *   │         │       │ pause/resume the sweep                      │
 *   │ TXD     │ PD1   │ USB-serial adapter, 9600 8N1                │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * State machine: a non-blocking two-phase step, MOVE then SETTLE, runs
 * on the shared millis() clock instead of a blocking _delay_ms() —
 * exactly the discipline examples/Modules/HCSR04/04 introduced for
 * combining a servo-speed schedule with a sensor-speed one:
 *   MOVE    — command the next angle with write(), then start a
 *             SETTLE_MS timer to give the servo's mechanics time to
 *             physically arrive before trusting a reading from it.
 *   SETTLE  — once SETTLE_MS has elapsed, take one HCSR04 measurement
 *             (measureCM() itself blocks briefly, up to its own
 *             internal ~30 ms timeout — the same accepted "atomic
 *             sensor op inside an otherwise non-blocking loop" pattern
 *             HCSR04 project 4 used), print it, update the running
 *             nearest-obstacle tracker for this sweep, and return to
 *             MOVE for the next angle. Reaching either sweep endpoint
 *             (ANGLE_MIN/ANGLE_MAX) reverses direction and prints a
 *             sweep summary instead of a single reading.
 * The button remains fully responsive throughout — nothing in this
 * loop ever blocks longer than a single HCSR04 ping.
 *
 * Gestures:
 *   clicked() — pause the sweep and detach() the servo (limp, saves
 *               current — see project 5), or resume from the paused
 *               angle if already paused.
 *
 * Servo concepts reused from the whole series:
 *   - Servo(channel), begin(), write(angleDeg), detach() (project 5).
 *
 * Non-Servo concepts reused:
 *   - HCSR04(trigPin, echoPin), begin(), measureCM().
 *   - Button for the pause/resume click.
 *   - The Timer0-overflow millis() clock (examples/timer/02) — free to
 *     use because Servo owns Timer1 exclusively, never Timer0.
 */

#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/usart.hpp>
#include <mikroduino/timer.hpp>
#include <mikroduino/registers.hpp>
#include <Servo.hpp>
#include <HCSR04.hpp>
#include <Button.hpp>

using namespace MikroDuino;

Servo   pan(0);
HCSR04  sonar(PD6, PD7);
Button  pauseButton(PD4);

// ── millis() clock (same technique as examples/timer/02_software_millis_clock) ──

static constexpr uint16_t MICROS_PER_OVERFLOW = 1024;
static constexpr uint16_t MILLIS_INC = MICROS_PER_OVERFLOW / 1000;
static constexpr uint16_t FRACT_INC  = MICROS_PER_OVERFLOW % 1000;
static constexpr uint16_t FRACT_MAX  = 1000;

static volatile uint32_t g_millis = 0;
static volatile uint16_t g_fract  = 0;

ISR(TIMER0_OVF_vect) {
    uint32_t m = g_millis;
    uint16_t f = g_fract;
    m += MILLIS_INC;
    f += FRACT_INC;
    if (f >= FRACT_MAX) { f -= FRACT_MAX; ++m; }
    g_fract  = f;
    g_millis = m;
}

static uint32_t millis() {
    uint32_t snapshot;
    ATOMIC_BLOCK_START;
    snapshot = g_millis;
    ATOMIC_BLOCK_END;
    return snapshot;
}

static constexpr int16_t  ANGLE_MIN   = 10;
static constexpr int16_t  ANGLE_MAX   = 170;
static constexpr int16_t  ANGLE_STEP  = 5;
static constexpr uint16_t SETTLE_MS   = 150;   // time for a 5 deg step to physically settle

enum class Phase : uint8_t { MOVE, SETTLE };

static void printSweepSummary(uint32_t nearestCm, int16_t nearestAngle) {
    if (nearestCm == 0xFFFFFFFFUL) {
        USART0.writeLine_P(PSTR("Sweep complete. No echo detected anywhere."));
        return;
    }
    USART0.write_P(PSTR("Sweep complete. Nearest: "));
    USART0.writeInt(static_cast<int32_t>(nearestCm));
    USART0.write_P(PSTR(" cm at angle "));
    USART0.writeInt(nearestAngle);
    USART0.writeLine_P(PSTR(" deg"));
}

int main() {
    pan.begin();
    sonar.begin();
    pauseButton.begin();

    Timer0.mode(TimerMode::Normal);
    Timer0.prescaler(TimerPrescaler::DIV64);
    Timer0.start();
    Timer0.enableOverflow();
    sei();

    USART0.begin(9600);
    USART0.writeLine_P(PSTR("Ultrasonic radar dashboard"));
    USART0.writeLine_P(PSTR("============================"));
    USART0.writeLine_P(PSTR("Click: pause/resume the sweep."));

    int16_t  angle       = ANGLE_MIN;
    int16_t  direction   = ANGLE_STEP;
    Phase    phase       = Phase::MOVE;
    bool     paused      = false;

    uint32_t nearestCm    = 0xFFFFFFFFUL;   // sentinel: "nothing measured yet this sweep"
    int16_t  nearestAngle = 0;

    uint32_t lastUpdateMs = millis();
    uint32_t phaseDueAt   = millis();

    pan.write(static_cast<uint8_t>(angle));

    while (true) {
        uint32_t now = millis();

        while (now - lastUpdateMs >= 1) {
            pauseButton.update();
            ++lastUpdateMs;
        }

        if (pauseButton.clicked()) {
            paused = !paused;
            if (paused) {
                pan.detach();
                USART0.writeLine_P(PSTR("Paused (servo detached)."));
            } else {
                pan.begin();
                pan.write(static_cast<uint8_t>(angle));
                phase     = Phase::MOVE;
                phaseDueAt = now + SETTLE_MS;
                USART0.writeLine_P(PSTR("Resumed."));
            }
        }

        if (paused || now < phaseDueAt) continue;

        if (phase == Phase::MOVE) {
            pan.write(static_cast<uint8_t>(angle));
            phase      = Phase::SETTLE;
            phaseDueAt = now + SETTLE_MS;
        } else {   // Phase::SETTLE due -> take a reading, then advance
            uint32_t cm = sonar.measureCM();

            USART0.write_P(PSTR("angle="));
            USART0.writeInt(angle);
            if (cm > 0) {
                USART0.write_P(PSTR(" deg  distance="));
                USART0.writeInt(static_cast<int32_t>(cm));
                USART0.writeLine_P(PSTR(" cm"));

                if (cm < nearestCm) {
                    nearestCm    = cm;
                    nearestAngle = angle;
                }
            } else {
                USART0.writeLine_P(PSTR(" deg  distance=(no echo)"));
            }

            // ---- Advance to the next angle, reversing at either end ----
            int16_t next = static_cast<int16_t>(angle + direction);
            if (next > ANGLE_MAX) {
                next      = ANGLE_MAX;
                direction = static_cast<int16_t>(-ANGLE_STEP);
                printSweepSummary(nearestCm, nearestAngle);
                nearestCm = 0xFFFFFFFFUL;
            } else if (next < ANGLE_MIN) {
                next      = ANGLE_MIN;
                direction = ANGLE_STEP;
                printSweepSummary(nearestCm, nearestAngle);
                nearestCm = 0xFFFFFFFFUL;
            }
            angle = next;

            phase      = Phase::MOVE;
            phaseDueAt = now;   // move immediately; MOVE phase sets its own SETTLE_MS wait
        }
    }
}
