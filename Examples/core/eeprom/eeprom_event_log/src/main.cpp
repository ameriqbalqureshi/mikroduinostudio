/*
 * EEPROM Example 4 (Advanced) — Wear-Leveled Event/Fault Log
 * MikroDuino SDK
 *
 * Builds on Example 3 (single struct) by storing a *sequence* of records
 * instead of one — a circular ring of slots in EEPROM, the same pattern
 * "black box" flight/fault recorders use. Introduces block-typed writes at
 * a moving address, and the wear-leveling trade-off that comes with any
 * frequently-written log: the log *payload* is spread across many slots
 * (each individual slot is written rarely), but the slot *index* itself is
 * necessarily written every single time something is logged.
 *
 * Real-world application: a diagnostic/fault logger for an embedded device
 * — e.g. logging brownout resets, threshold trips, or button-press events
 * with a running sequence number, so a technician can retrieve the last N
 * events after a field failure even though the device has no display.
 *
 * Hardware (ATmega328P / Arduino Nano @ 16 MHz):
 *
 *   ┌────────────────┬───────┬──────────────────────────────────────────┐
 *   │ Signal         │ Pin   │ Wiring                                   │
 *   ├────────────────┼───────┼──────────────────────────────────────────┤
 *   │ Event button   │ PD2   │ Button to GND, 10 kΩ pull-up. Each press │
 *   │                │       │ logs a new event                         │
 *   │ Dump button    │ PD3   │ Button to GND, 10 kΩ pull-up. Prints the │
 *   │                │       │ whole log over USART                     │
 *   │ Wipe button    │ PD4   │ Button to GND, 10 kΩ pull-up. Hold 2s to │
 *   │                │       │ erase the entire log                     │
 *   │ LOG_LED        │ PD6   │ Flashes once per logged event             │
 *   │ USART0 TX      │ PD1   │ To PC via USB-serial, 9600 8N1            │
 *   └────────────────┴───────┴──────────────────────────────────────────┘
 *
 * EEPROM features used in this example (new ones vs. Examples 1-3 in bold):
 *   EEPROM.updateBlock(addr, buf, len)  [NEW] — write one log record's raw
 *                                               bytes, skipping any that
 *                                               already match (matters when
 *                                               re-logging into a slot that
 *                                               still holds an old record)
 *   EEPROM.readBlock(addr, buf, len)           — read one record back out
 *   EEPROM.update<T>(addr, val)                — save the small header (next
 *                                               write index + record count)
 *   EEPROM.get<T>(addr) / put<T>(addr, val)    — load/init the header
 *
 * Layout: a small Header at address 0, followed by LOG_SLOTS fixed-size
 * LogEntry records immediately after it. The header's `nextSlot` field
 * always points at the oldest record, which is also where the *next* new
 * record gets written (classic ring-buffer overwrite-oldest behaviour).
 */

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/usart.hpp>
#include <mikroduino/eeprom.hpp>

using namespace MikroDuino;

#define WP(s)   USART0.write_P(PSTR(s))
#define WLP(s)  USART0.writeLine_P(PSTR(s))

static constexpr uint8_t EVENT_BTN = PD2;
static constexpr uint8_t DUMP_BTN  = PD3;
static constexpr uint8_t WIPE_BTN  = PD4;
static constexpr uint8_t LOG_LED   = PD6;

static constexpr uint8_t  LOG_SLOTS   = 16;
static constexpr uint8_t  LOG_MAGIC   = 0xE7;

struct LogEntry {
    uint8_t  valid;      // LOG_MAGIC once written, 0xFF while still blank
    uint16_t sequence;   // monotonically increasing event number
    uint8_t  eventCode;  // 1 = button event (only code used in this demo)
} __attribute__((packed));

struct LogHeader {
    uint8_t  magic;      // validates the header itself
    uint8_t  nextSlot;   // index (0..LOG_SLOTS-1) of the oldest/next-to-write slot
    uint16_t totalLogged;// lifetime count of events ever logged (for display only)
} __attribute__((packed));

static constexpr uint16_t ADDR_HEADER = 0x000;
static constexpr uint16_t ADDR_LOG0   = ADDR_HEADER + sizeof(LogHeader);

static void delay_ms(uint16_t ms) { while (ms--) _delay_ms(1); }

static uint16_t slot_addr(uint8_t slot) {
    return static_cast<uint16_t>(ADDR_LOG0 + slot * sizeof(LogEntry));
}

static bool pressed_edge(uint8_t pin, bool& wasDown) {
    bool down = !GPIO::read(pin);
    if (down && !wasDown) {
        delay_ms(20);
        if (!GPIO::read(pin)) { wasDown = true; return true; }
    }
    if (!down) wasDown = false;
    return false;
}

// Append one event: writes into the ring's next slot, then advances the
// index. Because the ring wraps, once all LOG_SLOTS are used, each new
// event silently overwrites the single oldest one -- exactly the behaviour
// you want from a rolling "last N events" black box.
static void log_event(LogHeader& hdr, uint8_t eventCode) {
    LogEntry entry;
    entry.valid     = LOG_MAGIC;
    entry.sequence  = hdr.totalLogged;
    entry.eventCode = eventCode;

    // updateBlock() instead of writeBlock(): if this slot happens to already
    // hold identical bytes from a previous wrap (rare, but possible for the
    // `valid` byte), those bytes are skipped rather than rewritten.
    EEPROM.updateBlock(slot_addr(hdr.nextSlot), &entry, sizeof(entry));

    hdr.nextSlot    = static_cast<uint8_t>((hdr.nextSlot + 1) % LOG_SLOTS);
    hdr.totalLogged = static_cast<uint16_t>(hdr.totalLogged + 1);

    // The header itself is small (4 bytes) but is, unavoidably, written on
    // every single log_event() call -- there's no way to wear-level "which
    // slot is next" without storing it somewhere. In practice this is an
    // acceptable trade-off: one cell absorbing frequent writes vs. losing
    // the ring position entirely on power loss.
    EEPROM.update(ADDR_HEADER, hdr);
}

static void dump_log(const LogHeader& hdr) {
    WLP("");
    WLP("=== Event Log Dump ===");
    WP("Lifetime events logged: "); USART0.writeInt(static_cast<int32_t>(hdr.totalLogged));
    WLP("");
    WLP("seq   code  (oldest first)");

    // Read starting from nextSlot (the oldest surviving record) and wrap
    // around LOG_SLOTS times so output comes out in chronological order.
    for (uint8_t i = 0; i < LOG_SLOTS; ++i) {
        uint8_t slot = static_cast<uint8_t>((hdr.nextSlot + i) % LOG_SLOTS);
        LogEntry entry;
        EEPROM.readBlock(slot_addr(slot), &entry, sizeof(entry));

        if (entry.valid != LOG_MAGIC) continue;   // slot never written yet

        USART0.writeInt(static_cast<int32_t>(entry.sequence));
        WP("   ");
        USART0.writeInt(entry.eventCode);
        WLP("");
    }
    WLP("=== End of log ===");
    WLP("");
}

int main() {
    GPIO::input(EVENT_BTN);
    GPIO::input(DUMP_BTN);
    GPIO::input(WIPE_BTN);
    GPIO::output(LOG_LED);
    GPIO::clear(LOG_LED);

    USART0.begin(9600);
    WLP("");
    WLP("MikroDuino EEPROM Example 4 — Wear-Leveled Event Log");

    LogHeader hdr = EEPROM.get<LogHeader>(ADDR_HEADER);
    if (hdr.magic != LOG_MAGIC) {
        WLP("No valid log header found -- initialising empty log.");
        hdr.magic       = LOG_MAGIC;
        hdr.nextSlot    = 0;
        hdr.totalLogged = 0;
        EEPROM.put(ADDR_HEADER, hdr);
    } else {
        WP("Existing log found: "); USART0.writeInt(static_cast<int32_t>(hdr.totalLogged));
        WLP(" events logged so far.");
    }

    bool eventWasDown = false, dumpWasDown = false, wipeWasDown = false;
    uint16_t wipeHoldMs = 0;

    while (true) {
        if (pressed_edge(EVENT_BTN, eventWasDown)) {
            log_event(hdr, /* eventCode = */ 1);
            GPIO::set(LOG_LED); delay_ms(80); GPIO::clear(LOG_LED);
            WP("Logged event #"); USART0.writeInt(static_cast<int32_t>(hdr.totalLogged - 1));
            WLP("");
        }

        if (pressed_edge(DUMP_BTN, dumpWasDown)) {
            dump_log(hdr);
        }

        if (!GPIO::read(WIPE_BTN)) {
            wipeHoldMs += 20;
            if (wipeHoldMs == 20) WLP("Hold wipe button for 2s to clear the log...");
            if (wipeHoldMs >= 2000) {
                hdr.magic       = LOG_MAGIC;
                hdr.nextSlot    = 0;
                hdr.totalLogged = 0;
                EEPROM.put(ADDR_HEADER, hdr);   // full write: resetting from a known state

                // Invalidate every slot's `valid` byte so dump_log() treats
                // the whole ring as empty again.
                for (uint8_t s = 0; s < LOG_SLOTS; ++s) {
                    EEPROM.update(slot_addr(s), static_cast<uint8_t>(0xFF));
                }
                WLP("Log wiped.");
                wipeHoldMs = 0;
                while (!GPIO::read(WIPE_BTN)) delay_ms(10);
            }
        } else {
            wipeHoldMs = 0;
        }
        (void)pressed_edge(WIPE_BTN, wipeWasDown);

        delay_ms(20);
    }
}
