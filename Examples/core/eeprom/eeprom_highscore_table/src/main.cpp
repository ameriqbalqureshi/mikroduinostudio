/*
 * EEPROM Example 5 (Advanced) — Arcade High-Score Table
 * MikroDuino SDK
 *
 * Builds on Example 3's typed struct (config profile) by persisting a
 * *sorted array* of records, and introduces setProgrammingMode() — a
 * 328P-specific optimisation for bulk rewrites that splits the usual atomic
 * erase+write cycle into two cheaper half-cycles.
 *
 * Real-world application: exactly what it says — the top-5 high-score table
 * on an arcade cabinet or handheld game, which must survive being switched
 * off between play sessions. A NEW SCORE button simulates a completed game
 * round (since there's no real game engine here) and inserts the result
 * into the table if it's good enough to make the top 5; a RESET button
 * wipes the table back to empty for a "new tournament".
 *
 * Hardware (ATmega328P / Arduino Nano @ 16 MHz):
 *
 *   ┌─────────────────┬───────┬────────────────────────────────────────────┐
 *   │ Signal          │ Pin   │ Wiring                                     │
 *   ├─────────────────┼───────┼────────────────────────────────────────────┤
 *   │ NEW SCORE button│ PD2   │ Button to GND, 10 kΩ pull-up. Simulates a  │
 *   │                 │       │ finished game round with a random score    │
 *   │ RESET button    │ PD3   │ Button to GND, 10 kΩ pull-up. Hold 2s to   │
 *   │                 │       │ wipe the whole table                       │
 *   │ HIGH_SCORE LED  │ PD6   │ Flashes when a new score makes the top 5   │
 *   │ USART0 TX       │ PD1   │ To PC via USB-serial, 9600 8N1              │
 *   └─────────────────┴───────┴────────────────────────────────────────────┘
 *
 * EEPROM features used in this example (new one vs. Examples 1-4 in bold):
 *   EEPROM.get<T>(addr) / put<T>(addr, val)   — load/initialise the table
 *   EEPROM.update<T>(addr, val)               — save the table after a new
 *                                                entry, rewriting only the
 *                                                slots that actually shifted
 *   EEPROM.setProgrammingMode(mode)     [NEW]  — 328P-only EECR[EEPM1:EEPM0]
 *                                                field; used here to make a
 *                                                full-table wipe cheaper by
 *                                                splitting each cell's cycle
 *                                                into EraseOnly (1.8 ms) then
 *                                                WriteOnly (1.8 ms) instead of
 *                                                the atomic EraseWrite (3.4 ms)
 *   EEPROM.write(addr, data)                  — used directly (not update())
 *                                                during the bulk wipe, since
 *                                                every cell is known to need
 *                                                writing
 *
 * NOTE: score "randomness" here is simulated with a tiny xorshift PRNG
 * seeded from the table's own lifetime submission count -- there's no real
 * game engine driving this demo, just a stand-in for "some score arrived".
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

static constexpr uint8_t NEW_SCORE_BTN = PD2;
static constexpr uint8_t RESET_BTN     = PD3;
static constexpr uint8_t HISCORE_LED   = PD6;

static constexpr uint8_t TABLE_MAGIC = 0x5C;
static constexpr uint8_t NUM_SCORES  = 5;

struct ScoreEntry {
    uint16_t score;
    char     initials[4];   // 3 letters + null terminator
} __attribute__((packed));

struct HighScoreTable {
    uint8_t    magic;
    ScoreEntry entries[NUM_SCORES];   // kept sorted descending by score
} __attribute__((packed));

static constexpr uint16_t ADDR_TABLE = 0x000;

static void delay_ms(uint16_t ms) { while (ms--) _delay_ms(1); }

static HighScoreTable make_empty_table() {
    HighScoreTable t;
    t.magic = TABLE_MAGIC;
    for (uint8_t i = 0; i < NUM_SCORES; ++i) {
        t.entries[i].score = 0;
        t.entries[i].initials[0] = '-'; t.entries[i].initials[1] = '-';
        t.entries[i].initials[2] = '-'; t.entries[i].initials[3] = '\0';
    }
    return t;
}

static void print_table(const HighScoreTable& t) {
    WLP("");
    WLP("=== HIGH SCORES ===");
    for (uint8_t i = 0; i < NUM_SCORES; ++i) {
        USART0.writeInt(i + 1); WP(". ");
        USART0.write(t.entries[i].initials);
        WP("  "); USART0.writeInt(static_cast<int32_t>(t.entries[i].score));
        WLP("");
    }
    WLP("===================");
}

// Tiny xorshift16 PRNG -- deterministic but "random enough" to demo the
// insertion logic without pulling in a full RNG library.
static uint16_t prng_next(uint16_t& state) {
    state ^= static_cast<uint16_t>(state << 7);
    state ^= static_cast<uint16_t>(state >> 9);
    state ^= static_cast<uint16_t>(state << 8);
    return state;
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

// Insert a new score if it beats the current lowest top-5 entry. Returns
// true if the table changed (i.e. it's worth saving + celebrating).
static bool try_insert(HighScoreTable& t, uint16_t score, const char* initials) {
    if (score <= t.entries[NUM_SCORES - 1].score) return false;   // didn't make the cut

    // Shift every entry below the insertion point down by one, dropping the
    // previous last place, then insert the new entry in its sorted position.
    uint8_t pos = NUM_SCORES - 1;
    while (pos > 0 && t.entries[pos - 1].score < score) {
        t.entries[pos] = t.entries[pos - 1];
        --pos;
    }
    t.entries[pos].score = score;
    t.entries[pos].initials[0] = initials[0];
    t.entries[pos].initials[1] = initials[1];
    t.entries[pos].initials[2] = initials[2];
    t.entries[pos].initials[3] = '\0';
    return true;
}

// Wipe the whole table using a split erase+write cycle. On the ATmega328P
// this halves the per-cell time (1.8 ms + 1.8 ms vs. one atomic 3.4 ms) for
// a bulk operation where every byte is known to need rewriting -- unlike
// update(), which would have to read-then-maybe-write each byte anyway.
static void wipe_table_split_cycle(HighScoreTable& t) {
#if defined(EEPM0) && defined(EEPM1)
    WLP("Wiping table using setProgrammingMode() split erase+write...");

    const uint16_t len = sizeof(HighScoreTable);

    // Pass 1: EraseOnly -- every cell in the table region becomes 0xFF.
    // The data argument to write() is irrelevant in this mode.
    EEPROM.setProgrammingMode(EEPROMMode::EraseOnly);
    for (uint16_t i = 0; i < len; ++i) {
        EEPROM.write(static_cast<uint16_t>(ADDR_TABLE + i), 0x00);
    }

    // Pass 2: WriteOnly -- programs the real default bytes into cells that
    // are now guaranteed already-erased (WriteOnly requires that).
    t = make_empty_table();
    EEPROM.setProgrammingMode(EEPROMMode::WriteOnly);
    EEPROM.writeBlock(ADDR_TABLE, &t, sizeof(t));

    // Restore the safe atomic default -- required before any further plain
    // write()/update() calls elsewhere in the program.
    EEPROM.setProgrammingMode(EEPROMMode::EraseWrite);
#else
    // MCUs without EEPM bits (ATmega32/16/64/128): fall back to an ordinary
    // atomic write for each byte -- functionally identical, just no timing
    // benefit from splitting the cycle.
    WLP("setProgrammingMode() unavailable on this MCU -- using plain write().");
    t = make_empty_table();
    EEPROM.writeBlock(ADDR_TABLE, &t, sizeof(t));
#endif
}

int main() {
    GPIO::input(NEW_SCORE_BTN);
    GPIO::input(RESET_BTN);
    GPIO::output(HISCORE_LED);
    GPIO::clear(HISCORE_LED);

    USART0.begin(9600);
    WLP("");
    WLP("MikroDuino EEPROM Example 5 — Arcade High-Score Table");

    HighScoreTable table = EEPROM.get<HighScoreTable>(ADDR_TABLE);
    if (table.magic != TABLE_MAGIC) {
        WLP("No valid table found -- initialising empty scoreboard.");
        table = make_empty_table();
        EEPROM.put(ADDR_TABLE, table);
    }
    print_table(table);

    uint16_t prng_state = static_cast<uint16_t>(0xACE1u ^ table.entries[0].score);
    bool newScoreWasDown = false, resetWasDown = false;
    uint16_t resetHoldMs = 0;

    while (true) {
        if (pressed_edge(NEW_SCORE_BTN, newScoreWasDown)) {
            uint16_t score = static_cast<uint16_t>(prng_next(prng_state) % 1000u);
            char initials[4] = { static_cast<char>('A' + (score % 26)),
                                  static_cast<char>('A' + ((score / 7) % 26)),
                                  static_cast<char>('A' + ((score / 13) % 26)),
                                  '\0' };

            WP("New game finished -- score = "); USART0.writeInt(static_cast<int32_t>(score));
            WLP("");

            if (try_insert(table, score, initials)) {
                // update<T>() compares the whole struct byte-for-byte and
                // rewrites only the entries that actually shifted position --
                // typically just the tail of the array, not the whole table.
                EEPROM.update(ADDR_TABLE, table);
                EEPROM.waitReady();

                GPIO::set(HISCORE_LED); delay_ms(150); GPIO::clear(HISCORE_LED);
                WLP("New high score!");
                print_table(table);
            } else {
                WLP("Didn't make the top 5.");
            }
        }

        if (!GPIO::read(RESET_BTN)) {
            resetHoldMs += 20;
            if (resetHoldMs == 20) WLP("Hold RESET for 2s to wipe the high-score table...");
            if (resetHoldMs >= 2000) {
                wipe_table_split_cycle(table);
                print_table(table);
                resetHoldMs = 0;
                while (!GPIO::read(RESET_BTN)) delay_ms(10);
            }
        } else {
            resetHoldMs = 0;
        }
        (void)pressed_edge(RESET_BTN, resetWasDown);

        delay_ms(20);
    }
}
