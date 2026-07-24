/*
 * EEPROM Example 2 (Beginner+) — Persistent Dimmer / Volume Memory
 * MikroDuino SDK
 *
 * Builds on Example 1 (boot counter) by introducing update() instead of
 * write()/put() — the key endurance-preservation technique for any value
 * that's saved far more often than it actually changes. update() reads the
 * stored byte first and only performs the ~3.4 ms EEPROM write cycle if the
 * new value actually differs, which matters because each AVR EEPROM cell is
 * only rated for ~100,000 write cycles.
 *
 * Real-world application: the "remember the last brightness/volume" feature
 * found in dimmer switches, desk lamps, and audio amplifiers. Turn the knob
 * to set a level, press SAVE to remember it, then power-cycle the board —
 * the LED comes back on at the saved level *before* you touch the knob,
 * proving the value survived the reset.
 *
 * Hardware (ATmega328P / Arduino Nano @ 16 MHz):
 *
 *   ┌───────────────┬───────┬───────────────────────────────────────────┐
 *   │ Signal        │ Pin   │ Wiring                                    │
 *   ├───────────────┼───────┼───────────────────────────────────────────┤
 *   │ Brightness pot│ A0    │ Potentiometer wiper; ends to GND and VCC  │
 *   │ Dimmer LED    │ PB1   │ OC1A (PWM1 channel A) -> LED + 220 Ω      │
 *   │ SAVE button   │ PD2   │ Button to GND, 10 kΩ external pull-up     │
 *   │ SAVED flash   │ PD6   │ LED + 220 Ω — flashes once per save       │
 *   │ USART0 TX     │ PD1   │ To PC via USB-serial, 9600 8N1            │
 *   └───────────────┴───────┴───────────────────────────────────────────┘
 *
 * EEPROM features used in this example (new one vs. Example 1 in bold):
 *   EEPROM.read(addr)          — restore the saved brightness at boot
 *   EEPROM.update(addr, data)  [NEW] — save the current brightness, but only
 *                                      if it actually changed since last save
 *   EEPROM.busy()              — used here just to show a save actually
 *                                      triggered a write cycle vs. was skipped
 */

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/adc.hpp>
#include <mikroduino/pwm.hpp>
#include <mikroduino/usart.hpp>
#include <mikroduino/eeprom.hpp>

using namespace MikroDuino;

#define WP(s)   USART0.write_P(PSTR(s))
#define WLP(s)  USART0.writeLine_P(PSTR(s))

static constexpr uint8_t  CH_POT      = 0;     // A0 — brightness knob
static constexpr uint8_t  SAVE_BTN    = PD2;   // active-low, external pull-up
static constexpr uint8_t  SAVED_LED   = PD6;
static constexpr uint16_t ADDR_LEVEL  = 0x000; // 1 byte: brightness 0-100 %

static void delay_ms(uint16_t ms) { while (ms--) _delay_ms(1); }

// Debounced "was the button just pressed" edge detector.
static bool button_pressed_edge() {
    static bool was_down = false;
    bool down = !GPIO::read(SAVE_BTN);   // active-low
    if (down && !was_down) {
        delay_ms(20);                    // debounce
        if (!GPIO::read(SAVE_BTN)) { was_down = true; return true; }
    }
    if (!down) was_down = false;
    return false;
}

int main() {
    GPIO::input(SAVE_BTN);
    GPIO::output(SAVED_LED);
    GPIO::clear(SAVED_LED);

    USART0.begin(9600);
    ADC_Driver.begin(ADCRef::AVCC, ADCPrescaler::DIV128, false);
    PWM1.begin(2000, PWMType::FastPWM);   // 2 kHz — flicker-free LED dimming

    WLP("");
    WLP("MikroDuino EEPROM Example 2 — Persistent Dimmer Memory");

    // ── Restore on boot ────────────────────────────────────────────────
    // read() returns raw 0xFF on a factory-blank cell; treat >100 as
    // "never saved before" and fall back to a sane default (50%).
    uint8_t saved_level = EEPROM.read(ADDR_LEVEL);
    if (saved_level > 100) saved_level = 50;

    WP("Restored brightness from EEPROM: ");
    USART0.writeInt(saved_level);
    WLP("%  (applying now, before the knob is read)");

    PWM1.dutyA(saved_level);

    // Hold the restored level for a moment so it's obviously visible before
    // the knob takes over live control — this is the part that proves
    // persistence actually worked, without needing a serial terminal.
    delay_ms(2000);

    WLP("Live control: turn the knob, press SAVE to remember this level.");

    uint8_t last_reported = 0xFFu;

    while (true) {
        // Live brightness follows the pot continuously.
        uint16_t raw   = ADC_Driver.read(CH_POT);
        uint8_t  level = static_cast<uint8_t>((static_cast<uint32_t>(raw) * 100u) / 1023u);
        PWM1.dutyA(level);

        if (level != last_reported) {
            WP("Level: "); USART0.writeInt(level); WLP("%");
            last_reported = level;
        }

        if (button_pressed_edge()) {
            // update() only performs the EEPROM write cycle if `level`
            // differs from what's already stored -- pressing SAVE repeatedly
            // at the same knob position costs zero extra write cycles.
            EEPROM.update(ADDR_LEVEL, level);

            bool wrote = EEPROM.busy();   // true only if a write cycle just started
            EEPROM.waitReady();

            GPIO::set(SAVED_LED);
            delay_ms(150);
            GPIO::clear(SAVED_LED);

            WP("SAVE pressed -- level "); USART0.writeInt(level);
            WP("%  ");
            if (wrote) WLP("[written to EEPROM]");
            else       WLP("[unchanged, no write needed]");
        }

        delay_ms(20);
    }
}
