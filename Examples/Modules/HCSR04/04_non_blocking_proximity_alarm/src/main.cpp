/*
 * HCSR04 Non-Blocking Scheduling + Proportional Proximity Alarm —
 * millis() — MikroDuino Module SDK
 *
 * Project 4 of 6 in the examples/Modules/HCSR04 series. Projects 1-3 all
 * blocked the CPU between pings with `_delay_ms()` — fine when nothing
 * else needs to happen in the meantime, but this project adds a genuine
 * second job: a classic parking-sensor LED that idles off with nothing
 * in range, blinks a slow steady heartbeat once an object is detected
 * but still safely far away, and speeds up continuously the closer that
 * object gets — which needs to keep blinking smoothly on its OWN
 * schedule regardless of when the next (blocking, sub-millisecond to a
 * few-millisecond) HCSR04 ping happens to fire.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ TRIG    │ PD6   │ HCSR04 trigger (project 1)                │
 *   │ ECHO    │ PD7   │ HCSR04 echo (project 1)                    │
 *   │ LED     │ PB5   │ Off = nothing in range, slow heartbeat =    │
 *   │         │       │ object detected but outside the alarm zone, │
 *   │         │       │ speeding up continuously as it gets closer  │
 *   │ TXD     │ PD1   │ USB-serial adapter, 9600 8N1                │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * Why this project switches to millis() (identical Timer0-overflow
 * technique to examples/timer/02_software_millis_clock and
 * examples/Modules/DHT22/04): a single blocking `_delay_ms(N)` per loop
 * pass can only serve one schedule. This project needs TWO independent
 * ones at once — a ~120 ms HCSR04 ping period (comfortably over the
 * sensor's recommended ~60 ms minimum spacing) and a much faster LED
 * blink period (as low as 80 ms right up against the object) — so both
 * are driven from the same free-running millis() clock instead, each
 * checked against its own "next due" timestamp every loop pass.
 *
 * Alarm logic (ordinary application code on top of measureCM(), not
 * part of the module): any reading at or beyond ALARM_DISTANCE_CM is
 * "safe" and blinks at the fixed slow BLINK_SAFE_MS rate. Anything
 * closer scales the blink period LINEARLY between BLINK_SAFE_MS (right
 * at the alarm boundary) and BLINK_MIN_MS (at 0 cm) — the same
 * continuously-speeding-up beep a car's parking sensor gives as you
 * back closer to an obstacle. A single failed ping (measureCM()
 * returning 0) does NOT change the alarm state either way — it simply
 * leaves the LED blinking at its last known distance's rate rather than
 * snapping off on one missed echo.
 *
 * HCSR04 concepts reused from projects 1-3:
 *   - HCSR04(trigPin, echoPin), begin(), measureCM().
 */

#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/usart.hpp>
#include <mikroduino/timer.hpp>
#include <mikroduino/registers.hpp>
#include <HCSR04.hpp>

using namespace MikroDuino;

static constexpr uint8_t LED = PB5;

HCSR04 sonar(PD6, PD7);

// ── millis() clock (identical technique to examples/timer/02_software_millis_clock) ──

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

// ── Alarm zone ───────────────────────────────────────────────────────────

static constexpr int32_t  ALARM_DISTANCE_CM = 30;    // zone boundary
static constexpr uint16_t BLINK_SAFE_MS     = 1000;  // slow heartbeat, at/beyond the boundary
static constexpr uint16_t BLINK_MIN_MS      = 80;    // fastest blink, right on top of the object

static constexpr uint16_t PING_PERIOD_MS = 120;   // >= HCSR04's ~60 ms recommended spacing

// Maps a distance inside the alarm zone to a blink period: BLINK_SAFE_MS
// at the boundary, shrinking linearly down to BLINK_MIN_MS at 0 cm.
static uint16_t blinkPeriodFor(int32_t cm) {
    if (cm >= ALARM_DISTANCE_CM) return BLINK_SAFE_MS;
    if (cm < 0) cm = 0;
    uint32_t span = static_cast<uint32_t>(BLINK_SAFE_MS - BLINK_MIN_MS);
    uint32_t period = BLINK_MIN_MS + (static_cast<uint32_t>(cm) * span) / ALARM_DISTANCE_CM;
    return static_cast<uint16_t>(period);
}

int main() {
    USART0.begin(9600);
    USART0.writeLine_P(PSTR("HCSR04 non-blocking scheduling + proximity alarm"));
    USART0.writeLine_P(PSTR("==================================================="));
    USART0.writeLine_P(PSTR(""));

    GPIO::output(LED);
    GPIO::clear(LED);

    sonar.begin();

    Timer0.mode(TimerMode::Normal);
    Timer0.prescaler(TimerPrescaler::DIV64);
    Timer0.start();
    Timer0.enableOverflow();
    sei();

    bool    haveData = false;
    int32_t lastDistanceCM = 0;

    uint32_t nextPingAt  = millis();
    uint32_t nextBlinkAt = millis();
    bool     ledOn       = false;

    while (true) {
        uint32_t now = millis();

        // ---- Sensor pinging, every PING_PERIOD_MS ----
        if (now >= nextPingAt) {
            nextPingAt = now + PING_PERIOD_MS;

            uint32_t cm = sonar.measureCM();
            if (cm > 0) {
                haveData = true;
                lastDistanceCM = static_cast<int32_t>(cm);

                bool inZone = lastDistanceCM < ALARM_DISTANCE_CM;
                USART0.write_P(PSTR("Distance: "));
                USART0.writeInt(lastDistanceCM);
                USART0.write_P(PSTR(" cm  "));
                USART0.writeLine_P(inZone ? PSTR(">>> ALARM ZONE <<<") : PSTR("(safe)"));
            } else {
                USART0.writeLine_P(PSTR("No echo - keeping previous alarm state"));
            }
        }

        // ---- LED blink, on its own independent schedule ----
        if (!haveData) {
            GPIO::clear(LED);
        } else if (now >= nextBlinkAt) {
            nextBlinkAt = now + blinkPeriodFor(lastDistanceCM);
            ledOn = !ledOn;
            GPIO::write(LED, ledOn);
        }
    }
}
