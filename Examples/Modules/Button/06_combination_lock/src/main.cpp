/*
 * Single-Button Combination Lock, EEPROM-Backed — MikroDuino Module SDK
 * (capstone)
 *
 * Project 6 of 6 in the examples/Modules/Button series. One button
 * enters a 4-digit code using every gesture Button offers, checks it
 * against a code persisted in on-chip EEPROM, and — on a correct entry —
 * offers a 3-second window to save a new code. A second, plain-GPIO
 * button (deliberately NOT wrapped in Button) provides a hold-2-seconds
 * factory reset, so this project closes the series by showing both when
 * to reach for Button and when a hand-rolled check is simpler.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌──────────────┬───────┬──────────────────────────────────────────┐
 *   │ Lock button  │ PD2   │ Other leg to GND — every gesture below    │
 *   │ Admin/reset  │ PD3   │ Other leg to GND — hold 2s: factory reset │
 *   │ LED          │ PB5   │ Status: solid=idle, fast blink=arming a   │
 *   │              │       │ commit, brief flash=digit committed,      │
 *   │              │       │ long solid=unlocked, rapid triple=denied  │
 *   │ TXD          │ PD1   │ USB-serial adapter — full transcript      │
 *   └──────────────┴───────┴──────────────────────────────────────────┘
 *
 * Gesture vocabulary (all on the one lock button):
 *   clicked()       -> advance the current digit (0-9, wraps)
 *   longPressed()   -> COMMIT the current digit and move to the next
 *                       position; heldMs() drives an accelerating "arming"
 *                       blink while held, exactly like project 3
 *   doubleClicked() -> clear the in-progress entry and start over
 *
 * Button concepts reused from the whole series:
 *   - The FULL constructor signature this time — Button(pin, activeLow,
 *     debounceMs, longPressMs, doubleClickMs) — spelled out explicitly
 *     rather than relying on defaults or setLongPressMs()/
 *     setDoubleClickMs() afterward, the way projects 1-2 and 3-4 each did
 *     it. Both ways of arriving at the same configuration are valid;
 *     this project shows the constructor form for completeness.
 *   - pressed()/released()/clicked()/doubleClicked()/longPressed()/
 *     isDown()/heldMs() — literally every Button method used in one
 *     coherent flow, not as isolated demonstrations.
 *
 * Non-Button concepts reused/introduced:
 *   - The Timer0-overflow millis() clock (examples/timer/02), driving
 *     both the non-blocking update() cadence and the 3-second
 *     change-code window.
 *   - EEPROMDriver (mikroduino/eeprom.hpp — see examples/i2c's EEPROM
 *     projects for the external-I2C-EEPROM equivalent; this is the
 *     ATmega's own ON-CHIP EEPROM, a completely different peripheral
 *     reached through a completely different, register-based API) for
 *     persisting the code across resets and power cycles, using the
 *     same 0xFF-sentinel "first boot" detection pattern as
 *     examples/eeprom_demo.
 *   - The admin/reset button on PD3 is read with GPIO::read() directly
 *     and timed by hand against millis() — NOT a Button object. A
 *     hold-for-2-seconds gesture is inherently tolerant of a few
 *     milliseconds of mechanical contact bounce right at the edge (the
 *     2000 ms threshold dwarfs any realistic bounce duration), so the
 *     full debounce/event machinery Button provides would be pure
 *     overhead here — sometimes the right amount of code really is a
 *     four-line manual check, and knowing which situation you're in is
 *     as much a part of using this SDK well as knowing the module API
 *     itself.
 */

#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/usart.hpp>
#include <mikroduino/timer.hpp>
#include <mikroduino/eeprom.hpp>
#include <mikroduino/registers.hpp>
#include <Button.hpp>

using namespace MikroDuino;

static constexpr uint8_t LED        = PB5;
static constexpr uint8_t ADMIN_PIN  = PD3;

// Full constructor form: pin, activeLow, debounceMs, longPressMs, doubleClickMs.
Button lockButton(PD2, true, 25, 1200, 400);

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

static void delay_ms(uint16_t ms) { while (ms--) _delay_ms(1); }

// ── EEPROM-backed code storage ──────────────────────────────────────────

struct LockCode {
    uint8_t magic;
    uint8_t digits[4];
};

static constexpr uint16_t EEPROM_ADDR = 0x000;
static constexpr uint8_t  MAGIC       = 0xA5;
static const uint8_t DEFAULT_DIGITS[4] = { 1, 2, 3, 4 };

static void saveCode(const LockCode& c) { EEPROM.put(EEPROM_ADDR, c); }

static LockCode loadOrInitCode() {
    LockCode c = EEPROM.get<LockCode>(EEPROM_ADDR);
    if (c.magic != MAGIC) {
        c.magic = MAGIC;
        for (uint8_t i = 0; i < 4; ++i) c.digits[i] = DEFAULT_DIGITS[i];
        saveCode(c);
        USART0.writeLine_P(PSTR("First boot: default code initialised (1-2-3-4)"));
    }
    return c;
}

// ── Lock state machine ──────────────────────────────────────────────────

enum class LockState : uint8_t { Entering, ChangePrompt, ChangingCode };

static LockState g_state = LockState::Entering;
static uint8_t   g_digitIndex   = 0;
static uint8_t   g_currentDigit = 0;
static uint8_t   g_codeBuffer[4];
static uint32_t  g_changePromptUntil = 0;

static LockCode g_storedCode;

// Arming-blink helper — same accelerating curve as project 3.
static constexpr uint16_t LONG_PRESS_MS = 1200;
static constexpr uint16_t BLINK_SLOW_MS = 300;
static constexpr uint16_t BLINK_FAST_MS = 40;

static uint16_t blinkPeriodFor(uint16_t heldMs) {
    if (heldMs >= LONG_PRESS_MS) return BLINK_FAST_MS;
    uint32_t span = BLINK_SLOW_MS - BLINK_FAST_MS;
    uint32_t drop = (span * heldMs) / LONG_PRESS_MS;
    return static_cast<uint16_t>(BLINK_SLOW_MS - drop);
}

static void printEntryPrompt() {
    USART0.write_P(PSTR("  digit["));
    USART0.writeInt(g_digitIndex);
    USART0.write_P(PSTR("] = "));
    USART0.writeInt(g_currentDigit);
    USART0.write_P(PSTR("   (click=+1, long-press=commit, double-click=clear entry)"));
    USART0.writeLine_P(PSTR(""));
}

static void deniedFlash() {
    // A rare, brief, deliberately blocking flourish — see header comment
    // on why a short one-off sequence like this doesn't need to be
    // folded into the non-blocking state machine.
    for (uint8_t i = 0; i < 3; ++i) {
        GPIO::set(LED); delay_ms(60);
        GPIO::clear(LED); delay_ms(60);
    }
}

int main() {
    GPIO::output(LED);
    GPIO::clear(LED);
    GPIO::inputPullup(ADMIN_PIN);

    lockButton.begin();

    Timer0.mode(TimerMode::Normal);
    Timer0.prescaler(TimerPrescaler::DIV64);
    Timer0.start();
    Timer0.enableOverflow();
    sei();

    USART0.begin(9600);
    USART0.writeLine_P(PSTR("Single-button combination lock (EEPROM-backed)"));
    USART0.writeLine_P(PSTR("================================================="));

    g_storedCode = loadOrInitCode();
    printEntryPrompt();

    uint32_t lastUpdateMs = millis();
    uint32_t nextBlinkAt  = 0;
    bool     ledOn        = false;
    bool     confirmedFlash = false;
    uint32_t confirmedUntil = 0;

    bool     adminLastLow   = false;
    uint32_t adminDownSince = 0;
    bool     adminDidReset  = false;

    while (true) {
        uint32_t now = millis();

        while (now - lastUpdateMs >= 1) {
            lockButton.update();
            ++lastUpdateMs;
        }

        // -------------------------------------------------------------
        // Admin/reset button — plain GPIO, hand-timed, no Button object.
        // -------------------------------------------------------------
        bool adminLow = (GPIO::read(ADMIN_PIN) == false);
        if (adminLow && !adminLastLow) {
            adminDownSince = now;
            adminDidReset  = false;
        }
        if (adminLow && !adminDidReset && (now - adminDownSince >= 2000)) {
            adminDidReset = true;
            g_storedCode.magic = MAGIC;
            for (uint8_t i = 0; i < 4; ++i) g_storedCode.digits[i] = DEFAULT_DIGITS[i];
            saveCode(g_storedCode);
            g_state = LockState::Entering;
            g_digitIndex = 0; g_currentDigit = 0;
            USART0.writeLine_P(PSTR(">> FACTORY RESET: code restored to 1-2-3-4"));
            printEntryPrompt();
        }
        adminLastLow = adminLow;

        // -------------------------------------------------------------
        // Change-code prompt window expiry
        // -------------------------------------------------------------
        if (g_state == LockState::ChangePrompt && now >= g_changePromptUntil) {
            g_state = LockState::Entering;
            USART0.writeLine_P(PSTR("Window expired -> code unchanged"));
            printEntryPrompt();
        }

        // -------------------------------------------------------------
        // Lock button gestures
        // -------------------------------------------------------------
        if (lockButton.pressed()) {
            nextBlinkAt = now;
        }

        // released() has no dedicated behaviour in this project — the
        // arming blink already stops itself the instant isDown() goes
        // false (see the LED feedback block below) — but it's still
        // polled every cycle so its one-shot flag never sits stale,
        // the same "drain it even when unused" discipline project 5
        // applied for the same reason.
        lockButton.released();

        if (lockButton.clicked()) {
            if (g_state == LockState::ChangePrompt) {
                // The click that ends the prompt window IS the first
                // action of entering a new code — no event is wasted.
                g_state = LockState::ChangingCode;
                g_digitIndex = 0;
                g_currentDigit = 0;
                USART0.writeLine_P(PSTR("Entering a NEW code:"));
            }
            g_currentDigit = static_cast<uint8_t>((g_currentDigit + 1) % 10);
            printEntryPrompt();
        }

        if (lockButton.doubleClicked()) {
            g_digitIndex = 0;
            g_currentDigit = 0;
            if (g_state == LockState::ChangePrompt) {
                g_state = LockState::Entering;
                USART0.writeLine_P(PSTR("Cancelled -> code unchanged"));
            } else {
                USART0.writeLine_P(PSTR("Entry cleared"));
            }
            printEntryPrompt();
        }

        if (lockButton.longPressed()) {
            if (g_state == LockState::ChangePrompt) {
                g_state = LockState::ChangingCode;
                g_digitIndex = 0;
            }

            g_codeBuffer[g_digitIndex] = g_currentDigit;
            USART0.write_P(PSTR("  -> committed digit["));
            USART0.writeInt(g_digitIndex);
            USART0.write_P(PSTR("] = "));
            USART0.writeInt(g_currentDigit);
            USART0.writeLine_P(PSTR(""));

            confirmedFlash = true;
            confirmedUntil = now + 250;
            GPIO::set(LED);

            ++g_digitIndex;
            g_currentDigit = 0;

            if (g_digitIndex >= 4) {
                g_digitIndex = 0;

                if (g_state == LockState::ChangingCode) {
                    for (uint8_t i = 0; i < 4; ++i) g_storedCode.digits[i] = g_codeBuffer[i];
                    saveCode(g_storedCode);
                    USART0.writeLine_P(PSTR(">> New code saved."));
                    g_state = LockState::Entering;
                    printEntryPrompt();
                } else {
                    bool match = true;
                    for (uint8_t i = 0; i < 4; ++i) {
                        if (g_codeBuffer[i] != g_storedCode.digits[i]) match = false;
                    }

                    if (match) {
                        USART0.writeLine_P(PSTR(">>> UNLOCKED <<<"));
                        USART0.writeLine_P(PSTR("Click within 3s to set a new code, or wait to keep this one."));
                        g_state = LockState::ChangePrompt;
                        g_changePromptUntil = now + 3000;
                        confirmedFlash = true;
                        confirmedUntil = now + 1500;   // longer solid flash for a successful unlock
                    } else {
                        USART0.writeLine_P(PSTR(">>> DENIED <<<"));
                        confirmedFlash = false;
                        deniedFlash();
                        g_state = LockState::Entering;
                        printEntryPrompt();
                    }
                }
            } else if (g_state != LockState::ChangePrompt) {
                printEntryPrompt();
            }
        }

        // -------------------------------------------------------------
        // LED feedback: confirmation flash takes priority over the
        // arming blink, which only runs while the button is physically
        // held down.
        // -------------------------------------------------------------
        if (confirmedFlash) {
            if (now >= confirmedUntil) {
                confirmedFlash = false;
                GPIO::clear(LED);
            }
        } else if (lockButton.isDown()) {
            if (now >= nextBlinkAt) {
                ledOn = !ledOn;
                GPIO::write(LED, ledOn);
                nextBlinkAt = now + blinkPeriodFor(lockButton.heldMs());
            }
        } else {
            GPIO::clear(LED);
        }
    }
}
