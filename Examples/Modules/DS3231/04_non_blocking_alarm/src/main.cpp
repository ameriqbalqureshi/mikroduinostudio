/*
 * DS3231 Non-Blocking Alarm Clock — millis() — MikroDuino Module SDK
 *
 * Project 4 of 6 in the examples/Modules/DS3231 series. Projects 1-3
 * all blocked the CPU with `_delay_ms()` between RTC polls — fine when
 * nothing else needs the CPU in between, but this project adds a
 * compile-time alarm (a fixed hour:minute target) that, once reached,
 * blinks an LED fast until the button silences it — and that blink
 * needs to keep animating smoothly, and the button needs to stay
 * responsive to a silence click, regardless of exactly when the next
 * once-a-second RTC poll happens to land.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ SDA     │ PC4   │ DS3231 (projects 1-3)                      │
 *   │ SCL     │ PC5   │                                            │
 *   │ Button  │ PD2   │ Other leg to GND — click silences an       │
 *   │         │       │ active alarm                                │
 *   │ LED     │ PB5   │ Off = no alarm, fast blink = alarm active  │
 *   │ TXD     │ PD1   │ USB-serial adapter, 9600 8N1               │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * Why this project switches to millis() (identical Timer0-overflow
 * technique to examples/timer/02_software_millis_clock, reused again in
 * examples/Modules/DHT22/04 and examples/Modules/DCMotor/04): a single
 * blocking `_delay_ms(1000)` per RTC poll would freeze button polling
 * for up to a second at a time — nowhere near the ~1 ms cadence
 * Button::update() needs — and would make a smooth 150 ms alarm blink
 * impossible on top of that. Driving the RTC poll, the button, and the
 * LED blink from three independent "next due" timestamps against one
 * free-running millis() clock lets all three keep their own schedule.
 *
 * Alarm design (ordinary application code, not part of DS3231): a
 * fixed ALARM_HOUR:ALARM_MINUTE target is compared against now() once a
 * second. Reaching it sets alarmActive; clicking the button while
 * active clears it AND remembers that this particular minute was
 * already silenced, so the alarm doesn't immediately re-trigger for
 * the 59 remaining seconds of that same minute. Once the clock ticks
 * past ALARM_MINUTE, the "already silenced" memory is cleared, ready
 * to fire again at the same time tomorrow.
 *
 * DS3231 concepts reused from projects 1-3:
 *   - DS3231(), begin(), now() — polled once a second, exactly as
 *     often as project 1, just from a non-blocking schedule instead of
 *     a blocking one.
 *
 * Button concepts reused from Button projects 1 and 5:
 *   - Button(pin), begin(), update(), clicked().
 */

#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/usart.hpp>
#include <mikroduino/i2c.hpp>
#include <mikroduino/timer.hpp>
#include <mikroduino/registers.hpp>
#include <Button.hpp>
#include <DS3231.hpp>

using namespace MikroDuino;

static constexpr uint8_t LED = PB5;

static constexpr uint8_t ALARM_HOUR   = 7;
static constexpr uint8_t ALARM_MINUTE = 0;

DS3231 rtc;
Button  silenceButton(PD2);

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

static void printField2(uint8_t v) {
    USART0.write(static_cast<char>('0' + (v / 10)));
    USART0.write(static_cast<char>('0' + (v % 10)));
}

static constexpr uint16_t RTC_POLL_MS  = 1000;
static constexpr uint16_t BLINK_ALARM_MS = 150;

int main() {
    GPIO::inputPullup(PC4);
    GPIO::inputPullup(PC5);
    GPIO::output(LED);
    GPIO::clear(LED);

    USART0.begin(9600);
    USART0.writeLine_P(PSTR("DS3231 non-blocking alarm clock"));
    USART0.writeLine_P(PSTR("================================="));
    USART0.write_P(PSTR("Alarm set for "));
    printField2(ALARM_HOUR);
    USART0.write_P(PSTR(":"));
    printField2(ALARM_MINUTE);
    USART0.writeLine_P(PSTR(""));
    USART0.writeLine_P(PSTR(""));

    I2C.beginMaster(100000UL);
    rtc.begin();
    silenceButton.begin();

    Timer0.mode(TimerMode::Normal);
    Timer0.prescaler(TimerPrescaler::DIV64);
    Timer0.start();
    Timer0.enableOverflow();
    sei();

    bool alarmActive         = false;
    bool silencedThisMinute  = false;

    uint32_t lastUpdateMs = millis();
    uint32_t nextPollAt   = millis();
    uint32_t nextBlinkAt  = millis();
    bool     ledOn        = false;

    while (true) {
        uint32_t now = millis();

        while (now - lastUpdateMs >= 1) {
            silenceButton.update();
            ++lastUpdateMs;
        }

        if (silenceButton.clicked() && alarmActive) {
            alarmActive        = false;
            silencedThisMinute = true;
            GPIO::clear(LED);
            USART0.writeLine_P(PSTR("Alarm silenced."));
        }

        // ---- RTC poll, once a second ----
        if (now >= nextPollAt) {
            nextPollAt = now + RTC_POLL_MS;

            DateTime dt = rtc.now();
            bool matches = (dt.hour == ALARM_HOUR) && (dt.minute == ALARM_MINUTE);

            if (matches && !silencedThisMinute && !alarmActive) {
                alarmActive = true;
                USART0.writeLine_P(PSTR(">>> ALARM <<<"));
            }
            if (!matches) {
                silencedThisMinute = false;   // armed again for next time
            }

            USART0.write_P(PSTR("Time: "));
            printField2(dt.hour);
            USART0.write_P(PSTR(":"));
            printField2(dt.minute);
            USART0.write_P(PSTR(":"));
            printField2(dt.second);
            USART0.writeLine_P(PSTR(""));
        }

        // ---- LED, on its own independent schedule ----
        if (alarmActive) {
            if (now >= nextBlinkAt) {
                nextBlinkAt = now + BLINK_ALARM_MS;
                ledOn = !ledOn;
                GPIO::write(LED, ledOn);
            }
        }
    }
}
