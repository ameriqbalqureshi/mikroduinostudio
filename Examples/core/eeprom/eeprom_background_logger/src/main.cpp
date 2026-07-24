/*
 * EEPROM Example 6 (Capstone) — Non-Blocking Background Sensor Logger
 * MikroDuino SDK
 *
 * The capstone of this EEPROM series: ties the ring-buffer pattern from
 * Example 4 together with the EE_READY interrupt from the driver's
 * enableInterrupt()/disableInterrupt() pair, so that the ~3.4 ms EEPROM
 * write cycle never stalls the main loop. Every previous example in this
 * series calls write()/update()/put() and just accepts that the *next*
 * EEPROM call will block on waitReady() -- fine for a button-triggered
 * save, but not for a control loop that has other real-time work to do
 * (reading sensors, driving actuators, servicing communications) while a
 * sample is being committed to flash-backed storage.
 *
 * Real-world application: a "black box" flight/drive recorder for a small
 * robot or drone -- it continuously samples a sensor and logs it to a
 * circular EEPROM buffer, but the sampling/control loop must keep running
 * at a steady rate regardless of how long the EEPROM write takes.
 *
 * Hardware (ATmega328P TQFP / Arduino Nano @ 16 MHz):
 *
 *   ┌─────────────────┬───────┬───────────────────────────────────────────┐
 *   │ Signal          │ Pin   │ Wiring                                    │
 *   ├─────────────────┼───────┼───────────────────────────────────────────┤
 *   │ Sensor (pot)    │ A0    │ Stand-in for a real sensor (accelerometer,│
 *   │                 │       │ pressure, etc). Sampled every 500 ms.     │
 *   │ Heartbeat LED   │ PB5   │ Toggles every 50 ms, continuously -- proof│
 *   │                 │       │ the main loop never stalls, even mid-write│
 *   │ WRITING LED     │ PD6   │ Lit only while an EEPROM write cycle is   │
 *   │                 │       │ actually in progress in the background    │
 *   │ Dump button     │ PD2   │ Button to GND, 10 kΩ pull-up. Prints the  │
 *   │                 │       │ ring buffer over USART (only when idle)   │
 *   │ USART0 TX       │ PD1   │ To PC via USB-serial, 9600 8N1             │
 *   └─────────────────┴───────┴───────────────────────────────────────────┘
 *
 * EEPROM features used in this example (new one vs. Examples 1-5 in bold):
 *   EEPROM.write(addr, data)            — starts a write cycle and returns
 *                                          immediately; it is the *next*
 *                                          EEPROM call that would normally
 *                                          block on waitReady() -- here we
 *                                          simply don't make one until the
 *                                          ISR says it's safe to.
 *   EEPROM.enableInterrupt()      [NEW] — arms EERIE right after starting a
 *                                          write, while EEPE is still set
 *   ISR(EE_READY_vect)            [NEW] — fires once the write finishes;
 *                                          clears EERIE (disableInterrupt())
 *                                          so it does not keep re-firing
 *                                          while the EEPROM sits idle
 *   EEPROM.disableInterrupt()     [NEW] — called from inside the ISR
 *   EEPROM.get<T>()/update<T>()         — small ring-buffer header, same
 *                                          pattern as Example 4
 *
 * The main loop never calls EEPROM.waitReady() or any blocking EEPROM
 * function while a write is outstanding -- it just polls a flag set by the
 * ISR. Compare the heartbeat LED's steady 50 ms toggle against the WRITING
 * LED: the heartbeat never skips a beat, even while a write is committing.
 *
 * IMPORTANT single-byte detail: each sample is stored as ONE byte (via
 * ADC_Driver.read8() -- see the ADC example series), and logged with a
 * single EEPROM.write() call. That matters because write() only avoids
 * blocking for *its own* cycle -- the *next* EEPROM call still blocks on
 * waitReady() until that cycle finishes. A multi-byte value written with
 * put<uint16_t>() would issue two write() calls back-to-back, and the
 * second one would immediately block waiting for the first to finish,
 * silently defeating the whole point of this example. One write() call per
 * logged sample keeps the "start it and walk away" property intact.
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/adc.hpp>
#include <mikroduino/usart.hpp>
#include <mikroduino/eeprom.hpp>

using namespace MikroDuino;

#define WP(s)   USART0.write_P(PSTR(s))
#define WLP(s)  USART0.writeLine_P(PSTR(s))

static constexpr uint8_t CH_SENSOR    = 0;    // A0
static constexpr uint8_t HEARTBEAT_LED= PB5;
static constexpr uint8_t WRITING_LED  = PD6;
static constexpr uint8_t DUMP_BTN     = PD2;

static constexpr uint8_t RING_SLOTS  = 32;
static constexpr uint8_t RING_MAGIC  = 0xB1;

struct RingHeader {
    uint8_t  magic;
    uint8_t  nextSlot;
} __attribute__((packed));

static constexpr uint16_t ADDR_HEADER = 0x000;
static constexpr uint16_t ADDR_SLOT0  = ADDR_HEADER + sizeof(RingHeader);   // one byte per sample

static uint16_t slot_addr(uint8_t slot) {
    return static_cast<uint16_t>(ADDR_SLOT0 + slot);
}

// ATmega32/16 use EE_RDY_vect; 328P/64/128 use EE_READY_vect.
#if defined(__AVR_ATmega32__) || defined(__AVR_ATmega16__)
#  define MD_EE_READY_VECT  EE_RDY_vect
#else
#  define MD_EE_READY_VECT  EE_READY_vect
#endif

// ── ISR shared state ───────────────────────────────────────────────────
static volatile bool g_write_done = false;

ISR(MD_EE_READY_VECT) {
    // Disable immediately: EERIE fires continuously whenever EEPE is clear,
    // so leaving it armed with nothing to write would re-enter forever.
    EEPROM.disableInterrupt();
    g_write_done = true;
}

static void delay_ms(uint16_t ms) { while (ms--) _delay_ms(1); }

static bool pressed_edge(uint8_t pin, bool& wasDown) {
    bool down = !GPIO::read(pin);
    if (down && !wasDown) {
        delay_ms(20);
        if (!GPIO::read(pin)) { wasDown = true; return true; }
    }
    if (!down) wasDown = false;
    return false;
}

static void dump_ring(const RingHeader& hdr) {
    WLP("");
    WLP("=== Sensor Log (oldest first, 0-255) ===");
    // Note: a slot that has never been written yet reads back as 0xFF (the
    // factory-erased state), indistinguishable from a real sample of 255.
    // That ambiguity is harmless here -- once the ring wraps once, every
    // slot holds a real sample and the distinction stops mattering. See
    // Example 4 for the "valid" magic-byte pattern that resolves this
    // properly when it matters (e.g. a log that must never show fake data).
    for (uint8_t i = 0; i < RING_SLOTS; ++i) {
        uint8_t slot = static_cast<uint8_t>((hdr.nextSlot + i) % RING_SLOTS);
        uint8_t sample = EEPROM.read(slot_addr(slot));
        USART0.writeInt(sample);
        WP("  ");
    }
    WLP("");
    WLP("=========================================");
}

enum class LogState : uint8_t { Idle, Writing };

int main() {
    GPIO::output(HEARTBEAT_LED);
    GPIO::output(WRITING_LED);
    GPIO::clear(WRITING_LED);
    GPIO::input(DUMP_BTN);

    USART0.begin(9600);
    // leftAdjust=true so read8() returns an 8-bit sample (ADCH only) -- see
    // the ADC example series (adc_env_dashboard) for read8() in isolation.
    // One byte per sample is what keeps each logged write a single
    // EEPROM.write() call (see the file header note above).
    ADC_Driver.begin(ADCRef::AVCC, ADCPrescaler::DIV128, true);

    WLP("");
    WLP("MikroDuino EEPROM Example 6 — Non-Blocking Background Logger");

    RingHeader hdr = EEPROM.get<RingHeader>(ADDR_HEADER);
    if (hdr.magic != RING_MAGIC) {
        WLP("No valid ring header -- initialising empty log.");
        hdr.magic    = RING_MAGIC;
        hdr.nextSlot = 0;
        EEPROM.put(ADDR_HEADER, hdr);
    } else {
        WLP("Existing sensor log found.");
    }

    LogState state          = LogState::Idle;
    uint8_t  pendingSample   = 0;
    uint16_t sampleTickMs    = 0;
    uint16_t heartbeatTickMs = 0;
    bool     heartbeatOn     = false;
    bool     dumpWasDown     = false;

    while (true) {
        // ── Heartbeat: proves the main loop is still running smoothly,  ──
        // completely independent of whatever the EEPROM is doing.
        heartbeatTickMs += 5;
        if (heartbeatTickMs >= 50) {
            heartbeatTickMs = 0;
            heartbeatOn = !heartbeatOn;
            GPIO::write(HEARTBEAT_LED, heartbeatOn);
        }

        // ── State machine ──────────────────────────────────────────────
        if (state == LogState::Idle) {
            // Dumping the log only makes sense when no write is pending --
            // reading a slot that's mid-write would race the hardware.
            if (pressed_edge(DUMP_BTN, dumpWasDown)) {
                dump_ring(hdr);
            }

            sampleTickMs += 5;
            if (sampleTickMs >= 500) {
                sampleTickMs = 0;

                pendingSample = ADC_Driver.read8(CH_SENSOR);

                // A single write() call: starts the erase+write cycle and
                // returns immediately (EEPE stays set until hardware finishes
                // ~3.4 ms later). Nothing here blocks waiting for that.
                EEPROM.write(slot_addr(hdr.nextSlot), pendingSample);

                // EEPE is still 1 here (the write we just started). Arming
                // EERIE now is safe -- the ISR won't fire until EEPE clears.
                g_write_done = false;
                EEPROM.enableInterrupt();
                sei();

                state = LogState::Writing;
                GPIO::set(WRITING_LED);
            }
        } else { // LogState::Writing
            // No blocking wait here -- just check the flag the ISR sets.
            // The rest of the loop (heartbeat above) keeps running exactly
            // as fast as if no EEPROM write were in flight at all.
            if (g_write_done) {
                GPIO::clear(WRITING_LED);

                hdr.nextSlot = static_cast<uint8_t>((hdr.nextSlot + 1) % RING_SLOTS);
                // The header write is small and infrequent (once per sample,
                // not once per CPU cycle), so a plain update() here is fine
                // even though it will briefly block the *next* EEPROM call.
                EEPROM.update(ADDR_HEADER, hdr);
                EEPROM.waitReady();

                WP("Logged sample "); USART0.writeInt(static_cast<int32_t>(pendingSample));
                WP(" into slot "); USART0.writeInt(static_cast<int32_t>((hdr.nextSlot + RING_SLOTS - 1) % RING_SLOTS));
                WLP(" (write completed in background)");

                state = LogState::Idle;
            }
        }

        _delay_ms(5);
    }
}
