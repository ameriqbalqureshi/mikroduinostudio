/*
 * EEPROM Example 3 (Intermediate) — Device Configuration Profile
 * MikroDuino SDK
 *
 * Builds on Example 2 (single saved byte) by persisting a whole struct with
 * put<T>()/get<T>(), and introduces the "magic byte" pattern almost every
 * real device uses to tell a genuinely-configured EEPROM apart from a
 * factory-blank one (or one written by a different firmware version).
 *
 * Real-world application: the configuration profile of an embedded device
 * (think a WiFi module, thermostat, or sensor node) — a device name, an
 * operating mode, and an alarm threshold that must survive power loss and
 * firmware updates, plus a physical "factory reset" button that wipes the
 * profile back to defaults.
 *
 * Hardware (ATmega328P / Arduino Nano @ 16 MHz):
 *
 *   ┌────────────────┬───────┬──────────────────────────────────────────┐
 *   │ Signal         │ Pin   │ Wiring                                   │
 *   ├────────────────┼───────┼──────────────────────────────────────────┤
 *   │ Mode button    │ PD2   │ Button to GND, 10 kΩ external pull-up.   │
 *   │                │       │ Cycles operating mode 0 -> 1 -> 2 -> 0   │
 *   │ Threshold pot  │ A0    │ Sets alarm threshold 0-1023               │
 *   │ Factory reset  │ PD3   │ Button to GND, 10 kΩ external pull-up.   │
 *   │                │       │ Hold 2s to wipe the saved profile         │
 *   │ CONFIG_OK LED  │ PD6   │ Lit whenever a valid profile is loaded    │
 *   │ USART0 TX      │ PD1   │ To PC via USB-serial, 9600 8N1            │
 *   └────────────────┴───────┴──────────────────────────────────────────┘
 *
 * EEPROM features used in this example (new ones vs. Examples 1-2 in bold):
 *   EEPROM.put<T>(addr, val)  [NEW]  — write a whole struct in one call
 *   EEPROM.get<T>(addr)       [NEW]  — read a whole struct back in one call
 *   EEPROM.update<T>(addr,val)[NEW]  — struct-aware update(): only the bytes
 *                                      that actually changed get re-written
 *   EEPROM.erase(addr)               — used to invalidate just the magic
 *                                      byte on factory reset, which is much
 *                                      cheaper than erasing the whole struct
 *                                      and forces reinitialisation on next boot
 *
 * The magic-byte pattern: the first byte of the stored struct is a constant
 * (0xC0 here) that only ever gets written by *this* firmware's "save
 * defaults" path. On boot we check it first:
 *   - matches   -> EEPROM holds a valid profile, load it as-is
 *   - anything else (0xFF blank, or leftover data from other firmware)
 *               -> treat as "unconfigured", write fresh defaults
 */

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/adc.hpp>
#include <mikroduino/usart.hpp>
#include <mikroduino/eeprom.hpp>

using namespace MikroDuino;

#define WP(s)   USART0.write_P(PSTR(s))
#define WLP(s)  USART0.writeLine_P(PSTR(s))

static constexpr uint8_t  MODE_BTN    = PD2;
static constexpr uint8_t  RESET_BTN   = PD3;
static constexpr uint8_t  CH_THRESH   = 0;     // A0
static constexpr uint8_t  CONFIG_LED  = PD6;
static constexpr uint16_t ADDR_CONFIG = 0x000;

static constexpr uint8_t CONFIG_MAGIC = 0xC0;

// __attribute__((packed)) removes implicit padding so sizeof(DeviceConfig)
// is deterministic across compilers/MCUs -- important since the layout is
// what actually gets stored byte-for-byte in EEPROM.
struct DeviceConfig {
    uint8_t  magic;        // must equal CONFIG_MAGIC for the rest to be trusted
    char     name[8];      // device label, e.g. "NODE-01"
    uint8_t  mode;         // 0=Idle 1=Active 2=Alarm
    uint16_t threshold;    // last-saved alarm threshold (raw ADC units)
} __attribute__((packed));

static void delay_ms(uint16_t ms) { while (ms--) _delay_ms(1); }

static DeviceConfig make_defaults() {
    DeviceConfig cfg;
    cfg.magic = CONFIG_MAGIC;
    cfg.name[0]='N'; cfg.name[1]='O'; cfg.name[2]='D'; cfg.name[3]='E';
    cfg.name[4]='-'; cfg.name[5]='0'; cfg.name[6]='1'; cfg.name[7]='\0';
    cfg.mode      = 0;
    cfg.threshold = 512;
    return cfg;
}

static void print_config(const DeviceConfig& cfg) {
    WP("  name=\""); USART0.write(cfg.name);
    WP("\"  mode="); USART0.writeInt(cfg.mode);
    WP("  threshold="); USART0.writeInt(static_cast<int32_t>(cfg.threshold));
    WLP("");
}

// Simple debounced "just pressed" edge detector, reused for both buttons.
static bool pressed_edge(uint8_t pin, bool& wasDown) {
    bool down = !GPIO::read(pin);   // active-low
    if (down && !wasDown) {
        delay_ms(20);
        if (!GPIO::read(pin)) { wasDown = true; return true; }
    }
    if (!down) wasDown = false;
    return false;
}

int main() {
    GPIO::input(MODE_BTN);
    GPIO::input(RESET_BTN);
    GPIO::output(CONFIG_LED);
    ADC_Driver.begin(ADCRef::AVCC, ADCPrescaler::DIV128, false);

    USART0.begin(9600);
    WLP("");
    WLP("MikroDuino EEPROM Example 3 — Device Configuration Profile");

    // ── Load or initialise ─────────────────────────────────────────────
    DeviceConfig cfg = EEPROM.get<DeviceConfig>(ADDR_CONFIG);

    if (cfg.magic != CONFIG_MAGIC) {
        WLP("No valid profile found (blank or foreign data) -- writing defaults.");
        cfg = make_defaults();
        EEPROM.put(ADDR_CONFIG, cfg);   // full write: nothing to compare against yet
    } else {
        WLP("Valid profile loaded from EEPROM:");
    }
    print_config(cfg);
    GPIO::set(CONFIG_LED);

    bool mode_was_down  = false;
    bool reset_was_down = false;
    uint16_t reset_hold_ms = 0;

    while (true) {
        // ── Mode button: cycle 0 -> 1 -> 2 -> 0, save with update<T>() ────
        if (pressed_edge(MODE_BTN, mode_was_down)) {
            cfg.mode = static_cast<uint8_t>((cfg.mode + 1) % 3);

            // update<T>() compares the new struct against what's stored and
            // only rewrites the bytes that differ -- here just the `mode`
            // byte, leaving `name` and `threshold` untouched in the NVM array.
            EEPROM.update(ADDR_CONFIG, cfg);
            EEPROM.waitReady();

            WP("Mode changed -> ");
            print_config(cfg);
        }

        // ── Threshold pot: live-update, saved only past a noise deadband ──
        // A raw ADC reading jitters by a few counts even with a steady knob;
        // gating on a minimum delta avoids hammering the EEPROM with a write
        // on nearly every loop iteration -- the same endurance concern that
        // motivated update() over write() in Example 2.
        uint16_t raw = ADC_Driver.read(CH_THRESH);
        int16_t  delta = static_cast<int16_t>(raw) - static_cast<int16_t>(cfg.threshold);
        if (delta > 4 || delta < -4) {
            cfg.threshold = raw;
            EEPROM.update(ADDR_CONFIG, cfg);   // again, only 2 bytes actually rewritten
        }

        // ── Factory reset: hold RESET_BTN for 2 seconds ──────────────────
        if (!GPIO::read(RESET_BTN)) {
            reset_hold_ms += 20;
            if (reset_hold_ms == 20) WLP("Hold factory-reset button for 2s to wipe profile...");
            if (reset_hold_ms >= 2000) {
                // erase() only needs to touch the magic byte: once it no
                // longer matches CONFIG_MAGIC, the next boot's validation
                // check will treat the whole struct as unconfigured and
                // regenerate defaults -- no need to erase the other 11 bytes.
                EEPROM.erase(ADDR_CONFIG);
                WLP("Factory reset -- magic byte erased. Resetting to defaults now.");

                cfg = make_defaults();
                EEPROM.put(ADDR_CONFIG, cfg);
                print_config(cfg);

                reset_hold_ms = 0;
                while (!GPIO::read(RESET_BTN)) delay_ms(10);   // wait for release
            }
        } else {
            reset_hold_ms = 0;
        }
        (void)pressed_edge(RESET_BTN, reset_was_down);   // keep debounce state fresh

        delay_ms(20);
    }
}
