/*
 * DHT22 Non-Blocking Scheduling + Threshold Alarm — millis() —
 * MikroDuino Module SDK
 *
 * Project 4 of 6 in the examples/Modules/DHT22 series. Projects 1-3 all
 * blocked the CPU between reads with `_delay_ms()` — fine when nothing
 * else needs to happen in the meantime, but this project adds a genuine
 * second job: an LED that blinks at a rate reflecting sensor status
 * (off = no data yet, slow heartbeat = all readings within comfort
 * range, fast = an alarm threshold is exceeded), which needs to keep
 * blinking smoothly on its OWN schedule regardless of when the next
 * (blocking, few-millisecond) DHT22 read happens to fire.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ DATA    │ PD4   │ DHT22 data line + pull-up (project 1)     │
 *   │ LED     │ PB5   │ Off = no data yet, slow blink = OK, fast   │
 *   │         │       │ blink = alarm threshold exceeded           │
 *   │ TXD     │ PD1   │ USB-serial adapter, 9600 8N1               │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * Why this project switches to millis() (identical Timer0-overflow
 * technique to examples/timer/02_software_millis_clock and
 * examples/Modules/Button/03, reused again in
 * examples/Modules/DCMotor/04): a single blocking `_delay_ms(N)` per
 * loop pass can only serve one schedule. This project needs TWO
 * independent ones at once — a ~2.5 s DHT22 sampling period (project
 * 1's spacing requirement) and a much faster LED blink period (as low
 * as 150 ms during an alarm) — so both are driven from the same
 * free-running millis() clock instead, each checked against its own
 * "next due" timestamp every loop pass.
 *
 * Alarm logic (ordinary application code on top of DHT22Data, not part
 * of the module): a reading counts as an alarm if temperature exceeds
 * TEMP_ALARM_C or humidity exceeds HUMIDITY_ALARM_PCT. A single failed
 * read (valid == false) does NOT change the alarm state either way — it
 * simply leaves the LED reflecting the last known-good reading rather
 * than flickering off on a transient checksum failure.
 *
 * DHT22 concepts reused from projects 1-3:
 *   - DHT22Driver(pin), read(), DHT22Data{humidity, temperature, valid}.
 */

#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/usart.hpp>
#include <mikroduino/timer.hpp>
#include <mikroduino/registers.hpp>
#include <DHT22.hpp>

using namespace MikroDuino;

static constexpr uint8_t LED = PB5;

DHT22Driver sensor(PD4);

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

// ── Alarm thresholds ─────────────────────────────────────────────────────

static constexpr float TEMP_ALARM_C       = 28.0f;
static constexpr float HUMIDITY_ALARM_PCT = 70.0f;

static constexpr uint16_t SAMPLE_PERIOD_MS = 2500;
static constexpr uint16_t BLINK_OK_MS      = 1000;   // slow heartbeat
static constexpr uint16_t BLINK_ALARM_MS   = 150;    // fast alarm blink

int main() {
    USART0.begin(9600);
    USART0.writeLine_P(PSTR("DHT22 non-blocking scheduling + threshold alarm"));
    USART0.writeLine_P(PSTR("=================================================="));
    USART0.writeLine_P(PSTR(""));

    GPIO::output(LED);
    GPIO::clear(LED);

    Timer0.mode(TimerMode::Normal);
    Timer0.prescaler(TimerPrescaler::DIV64);
    Timer0.start();
    Timer0.enableOverflow();
    sei();

    bool haveData = false;
    bool alarm    = false;

    uint32_t nextSampleAt = millis();
    uint32_t nextBlinkAt  = millis();
    bool     ledOn        = false;

    while (true) {
        uint32_t now = millis();

        // ---- Sensor sampling, every SAMPLE_PERIOD_MS ----
        if (now >= nextSampleAt) {
            nextSampleAt = now + SAMPLE_PERIOD_MS;

            DHT22Data reading = sensor.read();
            if (reading.valid) {
                haveData = true;
                alarm = (reading.temperature > TEMP_ALARM_C) ||
                        (reading.humidity    > HUMIDITY_ALARM_PCT);

                USART0.write_P(PSTR("H: "));
                USART0.writeFloat(reading.humidity, 1);
                USART0.write_P(PSTR("%  T: "));
                USART0.writeFloat(reading.temperature, 1);
                USART0.write_P(PSTR("C  "));
                USART0.writeLine_P(alarm ? PSTR(">>> ALARM <<<") : PSTR("(OK)"));
            } else {
                USART0.writeLine_P(PSTR("Read failed - keeping previous alarm state"));
            }
        }

        // ---- LED blink, on its own independent schedule ----
        if (!haveData) {
            GPIO::clear(LED);
        } else if (now >= nextBlinkAt) {
            uint16_t period = alarm ? BLINK_ALARM_MS : BLINK_OK_MS;
            nextBlinkAt = now + period;
            ledOn = !ledOn;
            GPIO::write(LED, ledOn);
        }
    }
}
