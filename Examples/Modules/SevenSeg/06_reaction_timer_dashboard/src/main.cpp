/*
 * SevenSeg Reaction Timer Dashboard — SevenSegMux + Button + EEPROM +
 * beginTimer2() — MikroDuino Module SDK (capstone)
 *
 * Project 6 of 6 in the examples/Modules/SevenSeg series. Everything
 * the previous five projects introduced, combined into a one-button
 * reaction-time game: beginTimer2() (project 4) keeps the display
 * refreshing entirely in the background, a millis() clock (project 5)
 * times both the random arming delay and the reaction itself, setChar
 * (project 1) spells out "FAIL" on a false start, and a persisted best
 * score in EEPROM survives a reset — the same "one physical button,
 * multiple gestures, EEPROM-backed setting" structure
 * examples/Modules/HCSR04/06_parking_sensor_dashboard and
 * examples/Modules/DHT22/06_datalogger_dashboard both built, applied
 * here to a game instead of a sensor dashboard.
 *
 * Hardware (ATmega328P @ 16 MHz, same 4-digit shared-bus wiring as
 * projects 2-5, plus one push button):
 *
 *   ┌──────────────┬───────┬──────────────────────────────────────────┐
 *   │ Segment a-g  │ PD0-6 │ Shared bus (see project 2's transistor      │
 *   │              │       │ note — applies here too)                    │
 *   │ Digit 0-3    │ PB0-3 │ Same as projects 2-5                         │
 *   │ Button       │ PC0   │ Other leg to GND — active-LOW, internal      │
 *   │              │       │ pull-up (Button's default)                   │
 *   └──────────────┴───────┴──────────────────────────────────────────┘
 *
 * Game flow, all on the one PC0 button:
 *   Idle    "0000"  clicked() -> arm: waits a pseudo-random 800-2499 ms
 *                    (see armReadyMs()'s comment — not a true RNG, just
 *                    enough unpredictability that a player can't time
 *                    a click against a fixed delay)
 *   Armed   "----"  clicked() NOW is a false start -> FalseStart.
 *                    Once the random wait elapses on its own -> Live.
 *   Live    "0000"  the GO signal. clicked() stops the clock: the
 *                    elapsed ms since Live started becomes this round's
 *                    reaction time, saved as the new best in EEPROM if
 *                    it beats the stored one -> Result.
 *   Result  NNNN ms clicked() immediately arms the next round (same as
 *                    Idle's clicked() -> Armed).
 *   FalseStart "FAIL" ignores clicks; auto-returns to Idle after 1.2 s.
 *
 *   doubleClicked() (Idle/Result only) -> shows the best time (or
 *                    "----" if no round has finished yet) for 1.2 s,
 *                    then returns to whatever was on screen before.
 *   longPressed()   (Idle only) -> resets the stored best back to
 *                    "no score yet", confirmed with a brief "----".
 *
 * SevenSegMux<N> concepts reused from projects 1-5:
 *   - SevenSegMux<4>(segPins, digitPins), begin(), beginTimer2()
 *     (project 4's automatic Timer2 refresh — no manual refresh() call
 *     or ISR needed anywhere in this file).
 *   - print(uint16_t, leadingZero) for both the reaction time and the
 *     best-time overlay.
 *   - setChar(pos, c) (project 1) spells "FAIL" one digit at a time
 *     on a false start, and "-" on all 4 digits for both the armed
 *     countdown and the "no score yet" / reset-confirmation states.
 *
 * Button concepts reused from project 5 / HCSR04/06:
 *   - Button(pin), begin(), update() (on its own accurate 1 ms
 *     schedule via the same Timer0 millis() technique), clicked(),
 *     doubleClicked(), longPressed().
 *
 * EEPROM concepts reused from HCSR04/06:
 *   - EEPROM.get<Settings>(addr) / EEPROM.update(addr, settings), with
 *     a magic byte to detect first-ever boot (blank/foreign EEPROM
 *     contents) the same way HCSR04/06 did for its unit setting.
 */

#include <avr/interrupt.h>
#include <mikroduino/timer.hpp>
#include <mikroduino/eeprom.hpp>
#include <mikroduino/registers.hpp>
#include <Button.hpp>
#include <SevenSeg.hpp>

using namespace MikroDuino;

static const uint8_t SEGS[7]   = { PD0, PD1, PD2, PD3, PD4, PD5, PD6 };
static const uint8_t DIGITS[4] = { PB0, PB1, PB2, PB3 };

SevenSegMux<4> mx(SEGS, DIGITS);
Button         goButton(PC0);

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

static constexpr uint16_t NO_BEST     = 0xFFFFu;
static constexpr uint16_t EEPROM_ADDR = 0x000;
static constexpr uint8_t  MAGIC       = 0x57;

struct Settings {
    uint8_t  magic;
    uint16_t bestMs;
};

static Settings g_settings;

static void saveSettings() {
    EEPROM.update(EEPROM_ADDR, g_settings);
}

static void loadOrInitSettings() {
    g_settings = EEPROM.get<Settings>(EEPROM_ADDR);
    if (g_settings.magic != MAGIC) {
        g_settings.magic  = MAGIC;
        g_settings.bestMs = NO_BEST;
        saveSettings();
    }
}

// ── Game state ───────────────────────────────────────────────────────────

enum class GameState : uint8_t { Idle, Armed, Live, Result, FalseStart };
enum class Overlay   : uint8_t { None, Best, Reset };

static GameState g_state          = GameState::Idle;
static uint32_t  g_stateEnteredMs = 0;
static uint32_t  g_armWaitMs      = 0;   // this round's random arming delay
static uint32_t  g_goAtMs         = 0;   // millis() when Live started
static uint16_t  g_lastReactionMs = 0;

static Overlay   g_overlay      = Overlay::None;
static uint32_t  g_overlayUntil = 0;

// Not a true RNG — just enough spread (800-2499 ms) off the low bits of
// millis() at the moment of arming that a player can't reliably predict
// when GO will fire. Good enough for a game; not for anything else.
static uint32_t armWaitMs(uint32_t now) {
    return 800u + (now % 1700u);
}

static void showDashes() {
    for (uint8_t pos = 0; pos < 4; ++pos) mx.setChar(pos, '-');
}

static void redraw() {
    if (g_overlay == Overlay::Best) {
        if (g_settings.bestMs == NO_BEST) showDashes();
        else                              mx.print(g_settings.bestMs, true);
        return;
    }
    if (g_overlay == Overlay::Reset) {
        showDashes();
        return;
    }

    switch (g_state) {
        case GameState::Idle:   mx.print(static_cast<uint16_t>(0), true); break;
        case GameState::Armed:  showDashes();                             break;
        case GameState::Live:   mx.print(static_cast<uint16_t>(0), true); break;
        case GameState::Result: mx.print(g_lastReactionMs, true);         break;
        case GameState::FalseStart:
            mx.setChar(0, 'F'); mx.setChar(1, 'A');
            mx.setChar(2, 'I'); mx.setChar(3, 'L');
            break;
    }
}

int main() {
    mx.begin();
    mx.beginTimer2();
    goButton.begin();
    loadOrInitSettings();

    Timer0.mode(TimerMode::Normal);
    Timer0.prescaler(TimerPrescaler::DIV64);
    Timer0.start();
    Timer0.enableOverflow();
    sei();

    redraw();

    uint32_t lastUpdateMs = millis();

    while (true) {
        uint32_t now = millis();
        while (now - lastUpdateMs >= 1) {
            goButton.update();
            ++lastUpdateMs;
        }

        bool dirty = false;

        if (goButton.clicked()) {
            switch (g_state) {
                case GameState::Idle:
                case GameState::Result:
                    g_state          = GameState::Armed;
                    g_stateEnteredMs = now;
                    g_armWaitMs      = armWaitMs(now);
                    dirty = true;
                    break;

                case GameState::Armed:
                    g_state          = GameState::FalseStart;
                    g_stateEnteredMs = now;
                    dirty = true;
                    break;

                case GameState::Live: {
                    uint32_t elapsed = now - g_goAtMs;
                    if (elapsed > 9999u) elapsed = 9999u;
                    g_lastReactionMs = static_cast<uint16_t>(elapsed);

                    if (g_settings.bestMs == NO_BEST || g_lastReactionMs < g_settings.bestMs) {
                        g_settings.bestMs = g_lastReactionMs;
                        saveSettings();
                    }

                    g_state          = GameState::Result;
                    g_stateEnteredMs = now;
                    dirty = true;
                    break;
                }

                case GameState::FalseStart:
                    break;   // ignored; auto-returns to Idle below
            }
        }

        if (goButton.doubleClicked() && (g_state == GameState::Idle || g_state == GameState::Result)) {
            g_overlay      = Overlay::Best;
            g_overlayUntil = now + 1200u;
            dirty = true;
        }

        if (goButton.longPressed() && g_state == GameState::Idle) {
            g_settings.bestMs = NO_BEST;
            saveSettings();
            g_overlay      = Overlay::Reset;
            g_overlayUntil = now + 800u;
            dirty = true;
        }

        // ---- Time-based transitions ----
        if (g_state == GameState::Armed && now - g_stateEnteredMs >= g_armWaitMs) {
            g_state          = GameState::Live;
            g_stateEnteredMs = now;
            g_goAtMs         = now;
            dirty = true;
        }
        if (g_state == GameState::FalseStart && now - g_stateEnteredMs >= 1200u) {
            g_state = GameState::Idle;
            dirty = true;
        }
        if (g_overlay != Overlay::None && now >= g_overlayUntil) {
            g_overlay = Overlay::None;
            dirty = true;
        }

        if (dirty) redraw();
    }
}
