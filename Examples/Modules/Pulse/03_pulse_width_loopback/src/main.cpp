/*
 * Pulse — PulseMeter Pulse-Width Measurement via Loopback — MikroDuino
 * Module SDK
 *
 * Project 3 of 6 in the examples/Modules/Pulse series. Projects 1-2 only
 * used Stopwatch, which times CODE (start()/stop() bracket whatever runs
 * between them). This project introduces PulseMeter, which times a
 * SIGNAL on a GPIO pin instead — the tool for measuring an ultrasonic
 * echo, an RC receiver channel, or (as here) a PWM output, with no
 * hardware capture pin required.
 *
 * Self-contained test signal: rather than requiring external test
 * equipment, this project reuses the SDK's DCMotor module purely as a
 * convenient, CPU-free hardware PWM generator — its direction pins are
 * left unconnected (no motor is attached) and only its PWM pin matters
 * here. DCMotor's fixed-frequency PWM (Timer0, ~977 Hz, prescaler 64 —
 * see DCMotor.hpp) runs entirely in hardware once configured, which is
 * essential: PulseMeter's methods BLOCK the CPU while they measure, so
 * the signal being measured must be generated independently of the code
 * doing the measuring, or nothing could produce the next edge while the
 * CPU is stuck waiting for it.
 *
 * Hardware (ATmega328P @ 16 MHz) — connect a jumper wire GEN to MEAS:
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ GEN     │ PD6   │ OC0A — DCMotor PWM output (~977 Hz)        │
 *   │ MEAS    │ PD7   │ PulseMeter input — jumper this to GEN       │
 *   │ TXD     │ PD1   │ USB-serial adapter, 9600 8N1, for readings │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * Pulse concepts introduced:
 *   - PulseMeter(pin) / begin() — configures the pin as a plain floating
 *     digital input. No pull-up/pull-down is applied; add one externally
 *     if your signal source needs it (DCMotor's PWM output actively
 *     drives both levels, so none is needed here).
 *   - pulseWidth(polarity, timeoutUs) — measures ONE complete HIGH pulse
 *     (polarity=true) in microseconds: syncs to idle, catches the
 *     leading edge, times to the trailing edge. Returns 0 on timeout —
 *     which is exactly what you would see running this project without
 *     the GEN-to-MEAS jumper connected.
 *   - Expected vs. measured: at a fixed ~977 Hz (~1024 us period),
 *     DCMotor.speed(pct) sets the HIGH fraction to approximately pct%
 *     of that period. This project cycles the duty cycle through three
 *     presets and prints the expected width alongside PulseMeter's
 *     measured one so the accuracy is visible directly, not just
 *     asserted.
 */

#include <util/delay.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/usart.hpp>
#include <Pulse.hpp>
#include <DCMotor.hpp>

using namespace MikroDuino;

static constexpr uint8_t GEN_PIN  = PD6;   // OC0A
static constexpr uint8_t MEAS_PIN = PD7;

// in1/in2 are unused stand-in pins — no motor is attached, only the PWM
// pin (GEN_PIN) matters for this project.
DCMotor    gen(PB0, PB1, GEN_PIN);
PulseMeter pm(MEAS_PIN);

static constexpr uint16_t PWM_PERIOD_US = 1024;   // ~977 Hz fixed carrier (see header)

int main() {
    USART0.begin(9600);
    USART0.writeLine_P(PSTR("Pulse: pulse-width measurement via loopback"));
    USART0.writeLine_P(PSTR("============================================"));
    USART0.writeLine_P(PSTR("Jumper GEN (PD6) to MEAS (PD7) - measured should track expected"));
    USART0.writeLine_P(PSTR(""));

    gen.begin();
    pm.begin();

    static const int8_t DUTY_PRESETS[3] = { 25, 50, 75 };
    uint8_t presetIndex = 0;

    while (true) {
        int8_t dutyPct = DUTY_PRESETS[presetIndex];
        gen.speed(dutyPct);
        _delay_ms(20);   // let a few PWM cycles settle after changing duty

        uint32_t expectedUs = (static_cast<uint32_t>(PWM_PERIOD_US) * dutyPct) / 100;
        uint32_t measuredUs = pm.pulseWidth(true, 50000UL);   // 50 ms timeout is ample at ~1 kHz

        USART0.write_P(PSTR("duty="));
        USART0.writeInt(dutyPct);
        USART0.write_P(PSTR("%  expected="));
        USART0.writeInt(static_cast<int32_t>(expectedUs));
        USART0.write_P(PSTR("us  measured="));
        if (measuredUs == 0) {
            USART0.writeLine_P(PSTR("TIMEOUT (check the GEN-to-MEAS jumper)"));
        } else {
            USART0.writeInt(static_cast<int32_t>(measuredUs));
            USART0.writeLine_P(PSTR("us"));
        }

        presetIndex = static_cast<uint8_t>((presetIndex + 1) % 3);
        _delay_ms(800);
    }
}
