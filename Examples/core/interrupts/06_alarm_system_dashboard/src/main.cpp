/*
 * Two-Sensor Alarm System Dashboard — MikroDuino SDK (capstone)
 *
 * Project 6 of 6 in the examples/interrupts series. A small Disarmed /
 * Armed / Triggered state machine driven entirely by two external
 * interrupts: a "sensor" input (a door switch or PIR module in a real
 * build) and an "arm/disarm" button — bringing together every
 * InterruptManager technique from projects 1-4 in one coherent project.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌─────────────┬───────┬──────────────────────────────────────────┐
 *   │ Sensor      │ PD2   │ INT0 — door switch / PIR output. Other    │
 *   │             │       │ leg to GND, internal pull-up. IGNORED     │
 *   │             │       │ entirely (via Interrupt.disable()) while  │
 *   │             │       │ the system is disarmed.                    │
 *   │ Arm button  │ PD3   │ INT1 — press to arm when disarmed, or to  │
 *   │             │       │ disarm from Armed/Triggered at any time.  │
 *   │ LED         │ PB5   │ off=Disarmed, steady on=Armed,             │
 *   │             │       │ fast-blinking=Triggered                    │
 *   │ TXD         │ PD1   │ USB-serial adapter — timestamped log       │
 *   └─────────────┴───────┴──────────────────────────────────────────┘
 *
 * State machine:
 *
 *   Disarmed --[arm button]--> Armed --[sensor trip]--> Triggered
 *      ^                         |                          |
 *      |------[arm button]-------+-------[arm button]-------+
 *
 * Design principle carried over from every ISR in this series: the
 * interrupt handlers themselves do almost nothing — onSensorTrigger()
 * and onArmButtonPress() only debounce (project 4's lockout technique)
 * and set a one-byte flag. ALL state-machine logic (deciding what a
 * sensor trip or button press actually MEANS given the current state)
 * runs in main(), which polls those flags. This keeps every ISR short
 * and keeps the actually-interesting decision logic in ordinary,
 * easy-to-read main-thread code.
 *
 * Interrupt concepts reused from the whole series:
 *   - Interrupt.attach()/enableGlobal() (project 1).
 *   - Interrupt.disable(INT0) / Interrupt.enable(INT0) (project 2) —
 *     used here for real, not just as an API demo: while Disarmed, the
 *     sensor line is switched off in hardware, so a door opening while
 *     disarmed can't do anything at all, not even set a flag that main()
 *     has to remember to ignore.
 *   - ISR-side debounce lockout timed against millis() (project 4),
 *     applied to both the sensor and the arm button independently.
 *
 * Interrupt concept introduced:
 *   - Interrupt.disableGlobal() / Interrupt.enableGlobal() used as a
 *     CRITICAL SECTION around a multi-step state update in enterState()
 *     below — not the global setup call project 1 used it for. This is
 *     the exact same mechanism as ATOMIC_BLOCK_START/END (registers.hpp)
 *     from the timer series: both are just named wrappers around
 *     cli()/sei(). ATOMIC_BLOCK_START/END additionally saves and
 *     restores the previous interrupt-enable state (safe to nest);
 *     disableGlobal()/enableGlobal() unconditionally sets it, which is
 *     fine for a short, non-nested section like this one, but would be
 *     the wrong tool inside a function that might itself be called from
 *     within another critical section.
 */

#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/interrupt.hpp>
#include <mikroduino/usart.hpp>
#include <mikroduino/timer.hpp>
#include <mikroduino/registers.hpp>

using namespace MikroDuino;

static constexpr uint8_t SENSOR     = PD2;   // INT0
static constexpr uint8_t ARM_BUTTON = PD3;   // INT1
static constexpr uint8_t LED        = PB5;

// ── millis() clock (same technique as the timer series and project 4) ────

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

// ── Debounced event flags, set by ISRs, consumed by main() ────────────────

static constexpr uint16_t DEBOUNCE_MS = 50;

static volatile bool     g_sensorEvent     = false;
static volatile bool     g_armButtonEvent  = false;
static volatile uint32_t g_lastSensorEdge  = 0;
static volatile uint32_t g_lastArmEdge     = 0;

// g_sensorEvent/g_armButtonEvent are single bytes, written true only by
// their ISR and written false only by main() — a strict single-writer-
// per-value discipline, so (like project 2's press counters) no atomic
// protection is needed to read or clear them safely.

void onSensorTrigger() {
    uint32_t now = millis();
    if (now - g_lastSensorEdge >= DEBOUNCE_MS) {
        g_sensorEvent = true;
    }
    g_lastSensorEdge = now;
}

void onArmButtonPress() {
    uint32_t now = millis();
    if (now - g_lastArmEdge >= DEBOUNCE_MS) {
        g_armButtonEvent = true;
    }
    g_lastArmEdge = now;
}

// ── State machine (main-thread only — no ISR ever touches these) ────────

enum class AlarmState : uint8_t { Disarmed, Armed, Triggered };

static AlarmState g_state        = AlarmState::Disarmed;
static uint32_t   g_triggeredAtMs = 0;

static void logLine_P(PGM_P msg) {
    USART0.write_P(PSTR("["));
    USART0.writeInt(static_cast<int32_t>(millis()));
    USART0.write_P(PSTR(" ms] "));
    USART0.writeLine_P(msg);
}

static void enterState(AlarmState newState) {
    uint32_t ts = millis();   // read before the critical section, kept simple

    // Critical section: g_state and g_triggeredAtMs must change together,
    // never read by main() (this same thread, admittedly — but this
    // mirrors the pattern correctly for the general case where a
    // multi-step update must not be observed half-done) as an
    // inconsistent pair.
    Interrupt.disableGlobal();
    g_state = newState;
    if (newState == AlarmState::Triggered) g_triggeredAtMs = ts;
    Interrupt.enableGlobal();

    switch (newState) {
        case AlarmState::Disarmed:
            Interrupt.disable(IntSource::INT0);   // sensor is now hardware-ignored
            GPIO::clear(LED);
            logLine_P(PSTR("DISARMED — sensor input disabled"));
            break;

        case AlarmState::Armed:
            Interrupt.enable(IntSource::INT0);    // sensor is live again
            GPIO::set(LED);
            logLine_P(PSTR("ARMED — sensor input enabled"));
            break;

        case AlarmState::Triggered:
            logLine_P(PSTR("*** ALARM TRIGGERED ***"));
            break;
    }
}

int main() {
    GPIO::output(LED);
    GPIO::clear(LED);
    GPIO::inputPullup(SENSOR);
    GPIO::inputPullup(ARM_BUTTON);

    Timer0.mode(TimerMode::Normal);
    Timer0.prescaler(TimerPrescaler::DIV64);
    Timer0.start();
    Timer0.enableOverflow();

    USART0.begin(9600);
    USART0.writeLine_P(PSTR("Two-sensor alarm system dashboard"));
    USART0.writeLine_P(PSTR("===================================="));
    USART0.writeLine_P(PSTR("Arm button (INT1/PD3): arm when disarmed, disarm at any other time"));
    USART0.writeLine_P(PSTR("Sensor     (INT0/PD2): trips the alarm only while Armed"));
    USART0.writeLine_P(PSTR(""));

    // Attach both sources up front; INT0 (sensor) starts disabled to
    // match the Disarmed state below, demonstrated with a direct call
    // rather than going through enterState() since nothing has "entered"
    // Disarmed yet — this IS the initial state.
    Interrupt.attach(IntSource::INT0, onSensorTrigger, IntSense::Falling);
    Interrupt.attach(IntSource::INT1, onArmButtonPress, IntSense::Falling);
    Interrupt.disable(IntSource::INT0);
    Interrupt.enableGlobal();

    logLine_P(PSTR("System boot: DISARMED"));

    while (true) {
        if (g_sensorEvent) {
            g_sensorEvent = false;
            if (g_state == AlarmState::Armed) {
                enterState(AlarmState::Triggered);
            }
            // Disarmed: can't happen — INT0 is hardware-disabled.
            // Already Triggered: ignore additional trips.
        }

        if (g_armButtonEvent) {
            g_armButtonEvent = false;
            if (g_state == AlarmState::Disarmed) {
                enterState(AlarmState::Armed);
            } else {
                enterState(AlarmState::Disarmed);   // Armed or Triggered -> Disarmed
            }
        }

        // Non-blocking fast blink while Triggered, timed off millis() —
        // main() stays free to keep polling the two event flags above
        // instantly, unlike a _delay_ms()-based blink would allow.
        if (g_state == AlarmState::Triggered) {
            uint32_t elapsed = millis() - g_triggeredAtMs;
            bool blinkOn = ((elapsed / 150) % 2) == 0;
            GPIO::write(LED, blinkOn);
        }
    }
}
