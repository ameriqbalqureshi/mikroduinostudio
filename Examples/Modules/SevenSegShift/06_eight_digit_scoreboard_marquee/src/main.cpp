/*
 * SevenSegShift Eight-Digit Scoreboard — N=8, Boot Marquee, Button, EEPROM,
 * beginTimer2() — MikroDuino Module SDK (capstone)
 *
 * Project 6 of 6 in the examples/Modules/SevenSegShift series. Everything
 * the previous five projects introduced, combined — AND pushed to N=8,
 * on the exact same 3 pins projects 1-5 used for N=4. That's the whole
 * point of this module: SevenSegMux (sdk/modules/SevenSeg) would need
 * 7+8=15 MCU pins to drive 8 digits directly; SevenSegShift needs 3,
 * whether N is 2 or 8, because both the segment byte and the
 * digit-select byte travel through the same two cascaded 74HC595s
 * regardless of how many of the digit-select bits are actually wired to
 * a digit on the board.
 *
 * Hardware (ATmega328P @ 16 MHz, an 8-digit 7-segment module with two
 * onboard 74HC595s, plus one push button):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ DATA/SER│ PD4   │ Same 3 pins as projects 1-5 — an 8-digit    │
 *   │ CLK/SRCLK│ PD5  │ board needs no extra MCU pins over a         │
 *   │ LATCH/RCLK│PD6  │ 4-digit one.                                  │
 *   │ Button  │ PC0   │ Other leg to GND — active-LOW, internal      │
 *   │         │       │ pull-up (Button's default)                   │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * Boot sequence: a short marquee scrolls the word "Scoreboard" across
 * all 8 digits using setChar() in a sliding 8-character window over a
 * longer padded string — something a 4-digit display genuinely can't
 * do (there's no room to show more than one letter's worth of context
 * at a time). beginTimer2() is already running at this point, so the
 * blocking _delay_ms() between marquee frames doesn't freeze the
 * display — Timer2 keeps refreshing it in the background exactly as it
 * does for the rest of the program.
 *
 * Game/UI flow, all on the one PC0 button:
 *   clicked()       -> score++ (wraps 65535 -> 0; printed across all
 *                       8 digits with leadingZero=true, so it always
 *                       reads e.g. "00001234")
 *   doubleClicked() -> toggle decimal <-> hex display of the same score
 *                       (printHex() always shows exactly N=8 hex
 *                       digits; since score is a uint16_t, only the
 *                       rightmost 4 are ever non-zero — printHex() has
 *                       no narrower-width option, unlike print()'s
 *                       leadingZero flag)
 *   longPressed()   -> if score beats the EEPROM-stored best, saves it;
 *                       then shows "  Stored" for ~900 ms and resets
 *                       score to 0 for the next round
 *
 * SevenSegShift<N> concepts reused from projects 1-5:
 *   - SevenSegShift<8>(dataPin, clockPin, latchPin), begin(),
 *     beginTimer2() (project 4's automatic Timer2 refresh — no manual
 *     refresh() call or ISR needed anywhere in this file), setChar
 *     (project 1), print(uint16_t, leadingZero) and printHex(uint16_t)
 *     (project 5).
 *
 * Button concepts reused from project 5:
 *   - Button(pin), begin(), update() (on its own accurate 1 ms
 *     schedule via the Timer0 millis() technique), clicked(),
 *     doubleClicked(), longPressed().
 *
 * EEPROM concepts reused from examples/Modules/SevenSeg/06's own
 * capstone:
 *   - EEPROM.get<Settings>(addr) / EEPROM.update(addr, settings), with
 *     a magic byte to detect first-ever boot (blank/foreign EEPROM
 *     contents).
 */

#include <util/delay.h>
#include <avr/interrupt.h>
#include <mikroduino/timer.hpp>
#include <mikroduino/eeprom.hpp>
#include <mikroduino/registers.hpp>
#include <Button.hpp>
#include <SevenSegShift.hpp>

using namespace MikroDuino;

SevenSegShift<8> board(PD4, PD5, PD6);
Button           scoreButton(PC0);

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

// ── EEPROM-backed best score ─────────────────────────────────────────────

static constexpr uint16_t EEPROM_ADDR = 0x000;
static constexpr uint8_t  MAGIC       = 0x58;   // distinct from other examples' EEPROM layouts

struct Settings {
    uint8_t  magic;
    uint16_t bestScore;
};

static Settings g_settings;

static void saveSettings() {
    EEPROM.update(EEPROM_ADDR, g_settings);
}

static void loadOrInitSettings() {
    g_settings = EEPROM.get<Settings>(EEPROM_ADDR);
    if (g_settings.magic != MAGIC) {
        g_settings.magic     = MAGIC;
        g_settings.bestScore = 0;
        saveSettings();
    }
}

// ── Boot marquee — scrolls "Scoreboard" across all 8 digits once ─────────
// Every character here is one setChar() recognises: 'S' (uppercase-only
// entry), 'c'/'e'/'b'/'a'/'d' (hex letters, same glyph either case), and
// 'o'/'r' (lowercase-only entries) — see SevenSegShift.hpp's charToSeg().

static void playBootMarquee() {
    static const char MARQUEE[] = "        Scoreboard        ";
    constexpr uint8_t MARQUEE_LEN = sizeof(MARQUEE) - 1;   // excludes '\0'

    for (uint8_t start = 0; start + 8u <= MARQUEE_LEN; ++start) {
        for (uint8_t pos = 0; pos < 8u; ++pos) board.setChar(pos, MARQUEE[start + pos]);
        _delay_ms(150);   // Timer2's beginTimer2() keeps refreshing during this blocking wait
    }
}

// ── UI state ─────────────────────────────────────────────────────────────

enum class Overlay : uint8_t { None, Stored };

static uint16_t g_score        = 0;
static bool     g_hexMode      = false;
static Overlay  g_overlay      = Overlay::None;
static uint32_t g_overlayUntil = 0;

static void redraw() {
    if (g_overlay == Overlay::Stored) {
        static const char STORED[8] = { ' ', ' ', 'S', 't', 'o', 'r', 'e', 'd' };
        for (uint8_t pos = 0; pos < 8u; ++pos) board.setChar(pos, STORED[pos]);
        return;
    }
    if (g_hexMode) board.printHex(g_score);
    else            board.print(g_score, true);
}

int main() {
    board.begin();
    board.beginTimer2();   // default prescaler 64 -> ~1/8 * 1 kHz per-digit refresh
    sei();

    playBootMarquee();

    scoreButton.begin();
    loadOrInitSettings();

    Timer0.mode(TimerMode::Normal);
    Timer0.prescaler(TimerPrescaler::DIV64);
    Timer0.start();
    Timer0.enableOverflow();

    redraw();

    uint32_t lastUpdateMs = millis();

    while (true) {
        uint32_t now = millis();
        while (now - lastUpdateMs >= 1) {
            scoreButton.update();
            ++lastUpdateMs;
        }

        bool dirty = false;

        if (scoreButton.clicked()) {
            ++g_score;   // wraps 65535 -> 0 naturally
            dirty = true;
        }
        if (scoreButton.doubleClicked()) {
            g_hexMode = !g_hexMode;
            dirty = true;
        }
        if (scoreButton.longPressed()) {
            if (g_score > g_settings.bestScore) {
                g_settings.bestScore = g_score;
                saveSettings();
            }
            g_score        = 0;
            g_overlay      = Overlay::Stored;
            g_overlayUntil = now + 900u;
            dirty = true;
        }

        if (g_overlay != Overlay::None && now >= g_overlayUntil) {
            g_overlay = Overlay::None;
            dirty = true;
        }

        if (dirty) redraw();
    }
}
