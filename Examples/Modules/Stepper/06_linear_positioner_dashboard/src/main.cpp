/*
 * Linear Positioner Dashboard — Stepper + LCD + Button + ADC + EEPROM —
 * MikroDuino Module SDK (capstone)
 *
 * Project 6 of 6 in the examples/Modules/Stepper series. Everything the
 * previous five projects introduced, combined into one on-screen axis
 * controller: two jog buttons move the carriage by hand at a pot-set
 * speed (project 3's hold-to-jog latch, project 4's live speed control),
 * a third button saves and recalls up to three EEPROM-backed preset
 * positions, and the LCD shows the live position and active slot — the
 * same "LCD + Button + EEPROM settings, gesture-driven single button"
 * structure examples/Modules/DCMotor/06 and LCD/06 built, applied here
 * to axis presets instead of motor limits or display preferences.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌──────────────┬───────┬──────────────────────────────────────────┐
 *   │ LCD RS       │ PB0   │ Same 4-bit wiring as the whole LCD series  │
 *   │ LCD EN       │ PB1   │                                            │
 *   │ LCD D4-D7    │ PB2-5 │                                            │
 *   │ STEP         │ PD2   │ Driver STEP input                          │
 *   │ DIR          │ PD3   │ Driver DIR input                           │
 *   │ /EN          │ PD4   │ Driver enable, active LOW                  │
 *   │ CW btn       │ PD5   │ Other leg to GND — hold = jog CW           │
 *   │ CCW btn      │ PD6   │ Other leg to GND — hold = jog CCW          │
 *   │ Preset btn   │ PD7   │ Other leg to GND — see gestures below      │
 *   │ Speed pot    │ PC0   │ ADC0 — wiper; outer legs to 5V/GND         │
 *   └──────────────┴───────┴──────────────────────────────────────────┘
 *
 * Preset button gestures (same event vocabulary as project 3 and
 * DCMotor/06's ctrlButton):
 *   clicked()       -> cycle the active preset slot: 1 -> 2 -> 3 -> 1
 *   doubleClicked() -> SAVE the current position into the active slot,
 *                       persisted to EEPROM so it survives a reset
 *   longPressed()   -> RECALL: move (blocking) to the active slot's
 *                       stored position
 *
 * Jog design reused from project 3: holding CW or CCW past the button's
 * longPressMs threshold latches continuous stepping, one step(1) call
 * per main-loop pass, until released() clears the latch. The driver is
 * only enable()d while something is actually moving — jogging or a
 * preset recall — and disable()d the instant motion stops, the same
 * power-saving pattern project 2 introduced.
 *
 * ADC concepts reused from project 4:
 *   - ADCDriver::begin(), ADCDriver::read(channel), and re-calling
 *     setRPM() live so the pot governs jog/recall speed in real time.
 *
 * EEPROM concepts reused from DCMotor/06 and LCD/06:
 *   - The 0xA5 magic-byte "first boot" pattern: on power-up, read a
 *     small settings struct back from EEPROM; if its magic byte doesn't
 *     match, this is a blank/first-ever boot, so all three presets
 *     default to position 0 instead of trusting garbage.
 *   - EEPROM.update(addr, struct) — skips the physical write if the
 *     stored bytes already match, extending EEPROM endurance versus an
 *     unconditional write on every save.
 *
 * Timing model: identical millis() technique to examples/Modules/
 * DCMotor/04 and /06 — a Timer0 overflow ISR maintains a free-running
 * millisecond counter, and buttons are updated on a strict 1 ms cadence
 * gated off it. This dashboard's loop body varies wildly in how long it
 * takes per pass (a bare ADC read vs. a blocking multi-second preset
 * recall), so the naive "call update() once per loop pass" DCMotor/01-03
 * got away with would stretch debounce and long-press timing
 * unpredictably; gating off millis() keeps button timing correct no
 * matter what else the loop is doing.
 */

#include <avr/interrupt.h>
#include <mikroduino/adc.hpp>
#include <mikroduino/eeprom.hpp>
#include <mikroduino/timer.hpp>
#include <mikroduino/registers.hpp>
#include <Button.hpp>
#include <LCD.hpp>
#include <Stepper.hpp>

using namespace MikroDuino;

CharLCD   lcd(PB0, PB1, PB2, PB3, PB4, PB5);
Stepper   motor(PD2, PD3, PD4);
Button    cwButton(PD5);
Button    ccwButton(PD6);
Button    presetButton(PD7);
ADCDriver adc;

static constexpr uint16_t STEPS_PER_REV = 200;
static constexpr uint16_t MIN_RPM = 10;
static constexpr uint16_t MAX_RPM = 120;
static constexpr uint8_t  PRESET_COUNT = 3;

// ── EEPROM-backed presets ──────────────────────────────────────────────

struct Settings {
    uint8_t magic;
    int32_t presets[PRESET_COUNT];
};

static constexpr uint16_t EEPROM_ADDR = 0x000;
static constexpr uint8_t  MAGIC       = 0xA5;

static Settings g_settings;

// ── millis() clock (identical technique to examples/Modules/DCMotor/04, /06) ──

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

static void savePresets() {
    EEPROM.update(EEPROM_ADDR, g_settings);
}

static void loadOrInitSettings() {
    g_settings = EEPROM.get<Settings>(EEPROM_ADDR);
    if (g_settings.magic != MAGIC) {
        g_settings.magic = MAGIC;
        for (uint8_t i = 0; i < PRESET_COUNT; ++i) g_settings.presets[i] = 0;
        savePresets();
    }
}

// ── Dashboard state ──────────────────────────────────────────────────────

static int32_t g_position     = 0;
static uint8_t g_activeSlot   = 0;
static uint16_t g_rpm         = MIN_RPM;
static bool    g_driverEnabled = false;

static void redraw() {
    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print("Pos:");
    lcd.print(g_position);
    lcd.print(" S");
    lcd.print(static_cast<int32_t>(g_activeSlot + 1));

    lcd.setCursor(0, 1);
    lcd.print("Sv:");
    lcd.print(g_settings.presets[g_activeSlot]);
    lcd.print(" ");
    lcd.print(static_cast<int32_t>(g_rpm));
    lcd.print("rpm");
}

int main() {
    lcd.begin();
    motor.begin();
    cwButton.begin();
    ccwButton.begin();
    presetButton.begin();
    adc.begin();
    loadOrInitSettings();

    Timer0.mode(TimerMode::Normal);
    Timer0.prescaler(TimerPrescaler::DIV64);
    Timer0.start();
    Timer0.enableOverflow();
    sei();

    redraw();

    bool cwJogging  = false;
    bool ccwJogging = false;

    uint32_t lastUpdateMs = millis();

    while (true) {
        uint32_t now = millis();

        while (now - lastUpdateMs >= 1) {
            cwButton.update();
            ccwButton.update();
            presetButton.update();
            ++lastUpdateMs;
        }

        // ---- Speed pot, applied live to whatever moves next ----
        uint16_t raw = adc.read(0);
        uint16_t rpm = static_cast<uint16_t>(MIN_RPM +
            (static_cast<uint32_t>(raw) * (MAX_RPM - MIN_RPM)) / 1023u);
        if (rpm != g_rpm) {
            g_rpm = rpm;
            motor.setRPM(g_rpm, STEPS_PER_REV);
        }

        // ---- Jog latches (project 3's hold-to-jog pattern) ----
        if (cwButton.longPressed())  cwJogging  = true;
        if (cwButton.released())     cwJogging  = false;
        if (ccwButton.longPressed()) ccwJogging = true;
        if (ccwButton.released())    ccwJogging = false;

        bool jogging = cwJogging || ccwJogging;
        if (jogging && !g_driverEnabled) {
            motor.enable(true);
            g_driverEnabled = true;
        }

        bool moved = false;
        if (cwJogging) {
            motor.step(1);
            ++g_position;
            moved = true;
        } else if (ccwJogging) {
            motor.step(-1);
            --g_position;
            moved = true;
        }

        if (!jogging && g_driverEnabled) {
            motor.enable(false);
            g_driverEnabled = false;
        }

        // ---- Preset slot cycling ----
        if (presetButton.clicked()) {
            g_activeSlot = static_cast<uint8_t>((g_activeSlot + 1) % PRESET_COUNT);
            moved = true;
        }

        // ---- Save current position into the active slot ----
        if (presetButton.doubleClicked()) {
            g_settings.presets[g_activeSlot] = g_position;
            savePresets();
            moved = true;
        }

        // ---- Recall: blocking move to the active slot's position ----
        if (presetButton.longPressed()) {
            int32_t target = g_settings.presets[g_activeSlot];
            int32_t delta  = target - g_position;
            if (delta != 0) {
                lcd.setCursor(0, 1);
                lcd.print("Moving...       ");
                motor.enable(true);
                g_driverEnabled = true;
                motor.step(delta);
                g_position = target;
                motor.enable(false);
                g_driverEnabled = false;
            }
            moved = true;
        }

        if (moved) redraw();
    }
}
