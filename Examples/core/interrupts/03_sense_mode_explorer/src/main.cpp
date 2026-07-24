/*
 * IntSense Mode Explorer — MikroDuino SDK
 *
 * Project 3 of 6 in the examples/interrupts series. Cycles ONE button
 * through all four IntSense trigger conditions, a few seconds each,
 * counting and reporting how many times the interrupt actually fires in
 * each mode — the differences are easy to describe but genuinely
 * surprising to see counted for the first time.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ Button  │ PD2   │ INT0 — other leg to GND, internal pull-up │
 *   │ LED     │ PB5   │ Toggled on every trigger, whatever the    │
 *   │         │       │ current sense mode is                     │
 *   │ TXD     │ PD1   │ USB-serial adapter                        │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * The four IntSense modes, and what they mean for a button wired with an
 * internal pull-up (idle = HIGH, pressed = LOW):
 *
 *   Falling — fires once, exactly on the press (HIGH -> LOW transition).
 *             The mode every earlier project in this series used.
 *   Rising  — fires once, exactly on the RELEASE (LOW -> HIGH). Holding
 *             the button down produces nothing; letting go does.
 *   Change  — fires on BOTH the press AND the release — two triggers per
 *             physical button cycle instead of one. A handler using this
 *             mode has to check the pin's current level itself if it
 *             needs to know which edge just happened.
 *   Low     — LEVEL-triggered, not edge-triggered: fires continuously
 *             for as long as the pin reads LOW, not just once at the
 *             transition. Hold the button down during this phase and
 *             watch the counter climb far faster than a human could
 *             possibly press it — the interrupt keeps re-firing every
 *             time it's re-enabled while the level condition still
 *             holds. This is the one AVR datasheet gotcha every external
 *             interrupt user eventually meets: Low is very rarely what
 *             you actually want, and this project exists partly just to
 *             make that visible before it surprises you in a real
 *             project.
 *
 * Interrupt concept introduced:
 *   - Interrupt.setSense(source, sense) — changes an ALREADY-ATTACHED
 *     source's trigger condition live, without detaching or re-attaching
 *     it (the handler and the EIMSK enable bit are both left alone).
 *     Internally this is exactly the second half of what attach() does
 *     (see interrupt.hpp) — attach() calls it once at registration time;
 *     this project calls it again, repeatedly, on a source that's
 *     already running.
 *
 * Interrupt concepts reused from projects 1-2:
 *   - Interrupt.attach(), Interrupt.enableGlobal().
 *
 * g_triggerCount is a 16-bit volatile updated from the ISR — sized to
 * survive a few seconds of Low mode's rapid re-firing without wrapping —
 * so, unlike project 2's single-byte counters, reading and clearing it
 * in main() is wrapped in ATOMIC_BLOCK_START/END (see registers.hpp,
 * the same tool the timer series used for its 32-bit millis() counter).
 */

#include <avr/pgmspace.h>
#include <util/delay.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/interrupt.hpp>
#include <mikroduino/usart.hpp>
#include <mikroduino/registers.hpp>

using namespace MikroDuino;

static constexpr uint8_t LED    = PB5;
static constexpr uint8_t BUTTON = PD2;

static volatile uint16_t g_triggerCount = 0;

void onTrigger() {
    GPIO::toggle(LED);
    ++g_triggerCount;
}

static void delay_ms(uint16_t ms) { while (ms--) _delay_ms(1); }

static const char NAME_FALLING[] PROGMEM = "Falling (press only)";
static const char NAME_RISING[]  PROGMEM = "Rising  (release only)";
static const char NAME_CHANGE[]  PROGMEM = "Change  (press AND release)";
static const char NAME_LOW[]     PROGMEM = "Low     (level-triggered — fires repeatedly while held!)";

struct SenseDemo {
    IntSense sense;
    const char* name;   // PROGMEM
};

static const SenseDemo MODES[4] = {
    { IntSense::Falling, NAME_FALLING },
    { IntSense::Rising,  NAME_RISING  },
    { IntSense::Change,  NAME_CHANGE  },
    { IntSense::Low,     NAME_LOW     },
};

int main() {
    GPIO::output(LED);
    GPIO::clear(LED);
    GPIO::inputPullup(BUTTON);

    USART0.begin(9600);
    USART0.writeLine_P(PSTR("IntSense mode explorer"));
    USART0.writeLine_P(PSTR("========================"));
    USART0.writeLine_P(PSTR("Each mode runs for 4s. Press/hold the button and watch the count."));
    USART0.writeLine_P(PSTR(""));

    uint8_t modeIdx = 0;
    Interrupt.attach(IntSource::INT0, onTrigger, MODES[0].sense);
    Interrupt.enableGlobal();

    USART0.write_P(PSTR("Mode: "));
    USART0.writeLine_P(MODES[0].name);

    while (true) {
        delay_ms(4000);

        // g_triggerCount is a 16-bit volatile shared with the ISR — a
        // 2-byte read/write is NOT atomic on an 8-bit AVR (contrast the
        // single-byte counters in project 2, which needed no such care),
        // so both the read and the clear are wrapped here.
        uint16_t count;
        ATOMIC_BLOCK_START;
        count = g_triggerCount;
        g_triggerCount = 0;
        ATOMIC_BLOCK_END;

        USART0.write_P(PSTR("  triggers in that window: "));
        USART0.writeInt(static_cast<int32_t>(count));
        USART0.writeLine_P(PSTR(""));
        USART0.writeLine_P(PSTR(""));

        modeIdx = static_cast<uint8_t>((modeIdx + 1) % 4);
        Interrupt.setSense(IntSource::INT0, MODES[modeIdx].sense);

        USART0.write_P(PSTR("Mode: "));
        USART0.writeLine_P(MODES[modeIdx].name);
    }
}
