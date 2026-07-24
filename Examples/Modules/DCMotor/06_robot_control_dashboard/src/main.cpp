/*
 * Robot Motor Control Dashboard — DCMotor + LCD + Button + ADC + EEPROM —
 * MikroDuino Module SDK (capstone)
 *
 * Project 6 of 6 in the examples/Modules/DCMotor series. Everything the
 * previous five projects introduced, combined into one on-screen motor
 * dashboard: a potentiometer sets throttle, an LCD shows gear/limit/speed
 * plus a live bar graph, and a single button handles gear selection,
 * a persisted speed-limit setting, and an emergency-stop latch — the
 * same "LCD + Button + EEPROM settings, one physical button" structure
 * examples/Modules/LCD/06_settings_menu built, here applied to a motor
 * instead of display preferences.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌──────────────┬───────┬──────────────────────────────────────────┐
 *   │ LCD RS       │ PB0   │ Same 4-bit wiring as the whole LCD series  │
 *   │ LCD EN       │ PB1   │                                            │
 *   │ LCD D4-D7    │ PB2-5 │                                            │
 *   │ Motor IN1    │ PD4   │ H-bridge direction input 1                 │
 *   │ Motor IN2    │ PD5   │ H-bridge direction input 2                 │
 *   │ Motor ENA    │ PD6   │ OC0A — PWM speed control                   │
 *   │ Throttle pot │ PC0   │ ADC0 — wiper; outer legs to 5V/GND         │
 *   │ Control btn  │ PD2   │ Other leg to GND — see gestures below      │
 *   └──────────────┴───────┴──────────────────────────────────────────┘
 *
 * Button gestures (all on the one PD2 button, same vocabulary as project
 * 3's clicked()/longPressed() plus doubleClicked() from LCD/06's menu):
 *   clicked()       -> flip gear: FORWARD <-> REVERSE
 *   doubleClicked() -> cycle the speed LIMIT: 25% -> 50% -> 75% -> 100%,
 *                       persisted to EEPROM so it survives a reset
 *   longPressed()   -> latch/unlatch an emergency brake; heldMs() drives
 *                       a live countdown bar on the LCD while arming,
 *                       the exact technique LCD/06's reset-to-defaults
 *                       gesture used
 *
 * Throttle math: the pot (0-1023) maps to 0-100%, then that percentage
 * is scaled down by whichever speed LIMIT is currently active (project
 * 3 introduced the raw pot-to-percent mapping; the limit scaling is new
 * here). The scaled result becomes the RAMP TARGET, and project 4's
 * soft-start/soft-stop ramp — unchanged — eases the motor's actual
 * speed toward it a couple of percent at a time, so twisting the pot
 * quickly still produces a smooth, gearbox-friendly acceleration curve
 * rather than a step change.
 *
 * EEPROM concepts reused from LCD/06 and Button/06:
 *   - The 0xA5 magic-byte "first boot" pattern: on power-up, read a
 *     small settings struct back from EEPROM; if its magic byte doesn't
 *     match, this is a blank/first-ever boot, so write sane defaults
 *     instead of trusting garbage.
 *   - EEPROM.update(addr, struct) — skips the physical write if the
 *     stored bytes already match, extending EEPROM endurance versus an
 *     unconditional write on every change.
 *
 * DCMotor concepts reused from every earlier project:
 *   - DCMotor(in1, in2, pwmPin), begin(), speed(pct) (projects 1-2),
 *     brake() (projects 1, 3, 4), and the soft-start/soft-stop ramp
 *     built entirely from speed() (project 4) — DCMotor supplies only
 *     the primitives; every project in this series has been about what
 *     application logic gets built on top of them.
 */

#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <mikroduino/adc.hpp>
#include <mikroduino/eeprom.hpp>
#include <mikroduino/timer.hpp>
#include <mikroduino/registers.hpp>
#include <Button.hpp>
#include <DCMotor.hpp>
#include <LCD.hpp>

using namespace MikroDuino;

CharLCD   lcd(PB0, PB1, PB2, PB3, PB4, PB5);
DCMotor   motor(PD4, PD5, PD6);
Button    ctrlButton(PD2);
ADCDriver adc;

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

// ── EEPROM-backed settings ──────────────────────────────────────────────

struct Settings {
    uint8_t magic;
    uint8_t limitIndex;   // 0=25% 1=50% 2=75% 3=100%
};

static constexpr uint16_t EEPROM_ADDR = 0x000;
static constexpr uint8_t  MAGIC       = 0xA5;
static constexpr uint8_t  LIMITS[4]   = { 25, 50, 75, 100 };

static Settings g_settings;

static void saveSettings() {
    EEPROM.update(EEPROM_ADDR, g_settings);
}

static void loadOrInitSettings() {
    g_settings = EEPROM.get<Settings>(EEPROM_ADDR);
    if (g_settings.magic != MAGIC) {
        g_settings.magic      = MAGIC;
        g_settings.limitIndex = 3;   // default: no limit (100%)
        saveSettings();
    }
}

// ── Dashboard state ──────────────────────────────────────────────────────

static bool    g_forward   = true;
static bool    g_eStop     = false;
static int8_t  g_current   = 0;   // ramped, actually-applied speed
static int8_t  g_target    = 0;   // ramp target, from pot * limit

static constexpr uint16_t RAMP_STEP_MS  = 15;
static constexpr int8_t   RAMP_STEP_PCT = 2;

static void redraw() {
    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print(g_forward ? "FWD " : "REV ");
    lcd.print("Lim:");
    lcd.print(static_cast<int32_t>(LIMITS[g_settings.limitIndex]));
    lcd.print("%");

    lcd.setCursor(0, 1);
    if (g_eStop) {
        lcd.print("E-STOP");
    } else {
        uint8_t magnitude = static_cast<uint8_t>(g_current < 0 ? -g_current : g_current);
        uint8_t filled = static_cast<uint8_t>((static_cast<uint16_t>(magnitude) * 8) / 100);
        lcd.writeChar('[');
        for (uint8_t i = 0; i < 8; ++i) lcd.writeChar(i < filled ? '#' : ' ');
        lcd.writeChar(']');
        lcd.print(static_cast<int32_t>(magnitude));
        lcd.print("%");
    }
}

int main() {
    lcd.begin();
    motor.begin();
    ctrlButton.begin();
    adc.begin();
    loadOrInitSettings();

    Timer0.mode(TimerMode::Normal);
    Timer0.prescaler(TimerPrescaler::DIV64);
    Timer0.start();
    Timer0.enableOverflow();
    sei();

    redraw();

    uint32_t lastUpdateMs = millis();
    uint32_t nextThrottleAt = millis();
    uint32_t nextRampAt     = millis();
    bool     dirty          = false;

    static constexpr uint16_t EBRAKE_HOLD_MS = 1200;

    while (true) {
        uint32_t now = millis();

        while (now - lastUpdateMs >= 1) {
            ctrlButton.update();
            ++lastUpdateMs;
        }

        // ---- Gear flip ----
        if (ctrlButton.clicked() && !g_eStop) {
            g_forward = !g_forward;
            dirty = true;
        }

        // ---- Speed-limit cycling, persisted ----
        if (ctrlButton.doubleClicked()) {
            g_settings.limitIndex = static_cast<uint8_t>((g_settings.limitIndex + 1) % 4);
            saveSettings();
            dirty = true;
        }

        // ---- Emergency-brake latch, with an arming progress bar ----
        if (!g_eStop && ctrlButton.isDown()) {
            uint16_t held = ctrlButton.heldMs();
            uint8_t filled = static_cast<uint8_t>(
                (static_cast<uint32_t>(held > EBRAKE_HOLD_MS ? EBRAKE_HOLD_MS : held) * 8) / EBRAKE_HOLD_MS
            );
            static uint8_t lastFilled = 0xFF;
            if (filled != lastFilled) {
                lastFilled = filled;
                lcd.setCursor(0, 1);
                lcd.writeChar('[');
                for (uint8_t i = 0; i < 8; ++i) lcd.writeChar(i < filled ? '#' : ' ');
                lcd.print("] arm");
            }
        }

        if (ctrlButton.longPressed()) {
            g_eStop  = !g_eStop;
            g_target = 0;
            // Zero the ramp's starting point too, not just its target —
            // otherwise releasing the brake would resume the ramp from
            // whatever speed was active before the brake, lurching the
            // motor back up before ramping back down to 0.
            g_current = 0;
            if (g_eStop) motor.brake();
            dirty = true;
        }

        if (dirty) {
            redraw();
            dirty = false;
        }

        // ---- Throttle sampling (10 ms cadence, project 3's rationale) ----
        if (!g_eStop && now >= nextThrottleAt) {
            nextThrottleAt = now + 10;

            uint16_t raw = adc.read(0);
            uint8_t  pct = static_cast<uint8_t>((static_cast<uint32_t>(raw) * 100u) / 1023u);
            pct = static_cast<uint8_t>((static_cast<uint16_t>(pct) * LIMITS[g_settings.limitIndex]) / 100u);

            int8_t newTarget = g_forward ? static_cast<int8_t>(pct) : static_cast<int8_t>(-pct);
            if (newTarget != g_target) {
                g_target = newTarget;
                dirty = true;
            }
        }

        // ---- Soft-start/soft-stop ramp (project 4, unchanged) ----
        if (!g_eStop && now >= nextRampAt && g_current != g_target) {
            nextRampAt = now + RAMP_STEP_MS;

            int16_t next = g_current;
            if (g_current < g_target) {
                next += RAMP_STEP_PCT;
                if (next > g_target) next = g_target;
            } else {
                next -= RAMP_STEP_PCT;
                if (next < g_target) next = g_target;
            }
            g_current = static_cast<int8_t>(next);
            motor.speed(g_current);
            dirty = true;
        }
    }
}
