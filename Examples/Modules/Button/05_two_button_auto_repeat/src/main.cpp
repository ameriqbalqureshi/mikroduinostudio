/*
 * Two Buttons, Independent Instances, Hand-Built Auto-Repeat —
 * MikroDuino Module SDK
 *
 * Project 5 of 6 in the examples/Modules/Button series. Wires up TWO
 * completely independent Button objects — an UP and a DOWN button
 * controlling one PWM-dimmed LED's brightness — and builds a "hold to
 * repeat" behaviour on top of Button's primitives, because the Button
 * class itself has no auto-repeat event: this project SUPPLEMENTS the
 * module with ordinary application logic built from isDown() and
 * heldMs(), exactly the kind of composition every module in this SDK is
 * designed to support rather than special-case internally.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ UP      │ PD2   │ Other leg to GND — increases brightness   │
 *   │ DOWN    │ PD3   │ Other leg to GND — decreases brightness   │
 *   │ LED     │ PB1   │ OC1A — PWM-driven brightness (see          │
 *   │         │       │ examples/pwm/01_led_breathing)             │
 *   │ TXD     │ PD1   │ USB-serial adapter                        │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * Two Button instances, fully independent: each one owns its own
 * debounce timer, hold timer, and click state — button.update() must be
 * called on BOTH objects every tick, and each tracks its own physical
 * pin completely separately. Nothing about using two at once is special;
 * this project exists mainly to make that concrete rather than assumed.
 *
 * The auto-repeat design, built from Button's state queries rather than
 * any single event method:
 *   - pressed() applies one immediate step — the "quick tap" case needs
 *     no repeat logic at all.
 *   - While isDown() stays true past an initial delay, this project's
 *     own repeatDue() helper (not part of Button) schedules further
 *     steps on a timer that ACCELERATES the longer heldMs() grows —
 *     slow at first, fast after a couple of seconds — the same
 *     "getting more urgent" curve project 3 used for its arming blink,
 *     reused here for step rate instead of blink rate.
 *   - released() ends the sequence for free: once isDown() goes false,
 *     the repeat loop simply stops being fed.
 *   - clicked() is deliberately NOT used for stepping here — pressed()
 *     already provides the single immediate step a tap needs, and using
 *     clicked() too would double-count every quick tap (once from
 *     pressed(), again from clicked() on release). It was project 1-4's
 *     tool for "wait and see if this became a full gesture"; this
 *     project's gesture vocabulary is simpler (just "down" and "how
 *     long"), so pressed()/isDown()/heldMs()/released() are the right
 *     four tools for the job.
 *
 * Button concepts reused from projects 1-4:
 *   - Button(pin), begin(), update(), pressed(), released(), isDown(),
 *     heldMs().
 *
 * Non-Button concept reused from the timer and pwm series:
 *   - The Timer0-overflow millis() clock (examples/timer/02) driving a
 *     non-blocking main loop, and PWM1.begin()/dutyA() (examples/pwm/01)
 *     for the brightness output itself.
 */

#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/usart.hpp>
#include <mikroduino/timer.hpp>
#include <mikroduino/pwm.hpp>
#include <mikroduino/registers.hpp>
#include <Button.hpp>

using namespace MikroDuino;

Button buttonUp(PD2);
Button buttonDown(PD3);

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

// ── Hand-built auto-repeat, layered on top of Button's isDown()/heldMs() ──

static constexpr uint16_t REPEAT_INITIAL_DELAY_MS = 400;   // pause before repeat kicks in
static constexpr uint16_t REPEAT_SLOW_MS = 180;             // step period, just started repeating
static constexpr uint16_t REPEAT_FAST_MS = 30;              // step period, held a while
static constexpr uint16_t REPEAT_ACCEL_WINDOW_MS = 2000;    // time to reach full speed

static uint16_t repeatIntervalFor(uint16_t heldMs) {
    if (heldMs >= REPEAT_INITIAL_DELAY_MS + REPEAT_ACCEL_WINDOW_MS) return REPEAT_FAST_MS;
    uint16_t intoAccel = static_cast<uint16_t>(heldMs - REPEAT_INITIAL_DELAY_MS);
    uint32_t span = REPEAT_SLOW_MS - REPEAT_FAST_MS;
    uint32_t drop = (span * intoAccel) / REPEAT_ACCEL_WINDOW_MS;
    return static_cast<uint16_t>(REPEAT_SLOW_MS - drop);
}

// Per-button repeat bookkeeping — this struct and repeatDue() are this
// project's own code, not part of Button.
struct RepeatTracker {
    uint32_t nextStepAt = 0;
};

static bool repeatDue(Button& btn, RepeatTracker& tracker, uint32_t now) {
    if (!btn.isDown()) return false;

    uint16_t held = btn.heldMs();
    if (held < REPEAT_INITIAL_DELAY_MS) return false;   // still in the initial pause

    if (now >= tracker.nextStepAt) {
        tracker.nextStepAt = now + repeatIntervalFor(held);
        return true;
    }
    return false;
}

// ── Brightness output ────────────────────────────────────────────────────

static uint8_t g_brightness = 50;

static void applyBrightness(int8_t delta) {
    int16_t v = static_cast<int16_t>(g_brightness) + delta;
    if (v < 0)   v = 0;
    if (v > 100) v = 100;
    g_brightness = static_cast<uint8_t>(v);
    PWM1.dutyA(g_brightness);
}

int main() {
    buttonUp.begin();
    buttonDown.begin();

    PWM1.begin(500, PWMType::FastPWM);
    PWM1.dutyA(g_brightness);

    Timer0.mode(TimerMode::Normal);
    Timer0.prescaler(TimerPrescaler::DIV64);
    Timer0.start();
    Timer0.enableOverflow();
    sei();

    USART0.begin(9600);
    USART0.writeLine_P(PSTR("Two independent buttons: UP/DOWN with hand-built auto-repeat"));
    USART0.writeLine_P(PSTR("================================================================"));
    USART0.write_P(PSTR("brightness="));
    USART0.writeInt(g_brightness);
    USART0.writeLine_P(PSTR(""));

    RepeatTracker upRepeat, downRepeat;
    uint32_t lastUpdateMs = millis();
    uint8_t  lastPrinted  = g_brightness;

    while (true) {
        uint32_t now = millis();

        while (now - lastUpdateMs >= 1) {
            buttonUp.update();
            buttonDown.update();
            ++lastUpdateMs;
        }

        if (buttonUp.pressed())   applyBrightness(+5);
        if (buttonDown.pressed()) applyBrightness(-5);

        if (repeatDue(buttonUp, upRepeat, now))     applyBrightness(+5);
        if (repeatDue(buttonDown, downRepeat, now)) applyBrightness(-5);

        // released() consumed here purely so each event queue stays
        // clean — this project has no behaviour tied to release itself,
        // the repeat loop already stops naturally once isDown() is false.
        buttonUp.released();
        buttonDown.released();

        if (g_brightness != lastPrinted) {
            lastPrinted = g_brightness;
            USART0.write_P(PSTR("brightness="));
            USART0.writeInt(g_brightness);
            USART0.writeLine_P(PSTR(""));
        }
    }
}
