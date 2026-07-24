/*
 * Two DCMotor Instances, Arcade-Drive Mixing — MikroDuino Module SDK
 *
 * Project 5 of 6 in the examples/Modules/DCMotor series. Wires up TWO
 * completely independent DCMotor objects — a LEFT and a RIGHT wheel
 * motor — and drives them from a pair of potentiometers standing in for
 * a joystick: one axis sets forward/reverse THROTTLE, the other sets
 * left/right TURN, and a small mixing formula combines the two into
 * per-wheel speeds. This is the classic "arcade drive" used by countless
 * real tank-steer robots. Exactly like examples/Modules/Button/05's two
 * independent Button objects, nothing about using two DCMotor instances
 * at once is special — this project exists mainly to make that concrete.
 *
 * Hardware (ATmega328P @ 16 MHz, two independent H-bridge channels —
 * e.g. both channels of one L298N, or two separate driver boards):
 *
 *   ┌──────────────┬───────┬──────────────────────────────────────────┐
 *   │ LEFT IN1     │ PD4   │ Left motor direction input 1               │
 *   │ LEFT IN2     │ PD5   │ Left motor direction input 2               │
 *   │ LEFT ENA     │ PD6   │ OC0A — left motor PWM speed                │
 *   │ RIGHT IN1    │ PB0   │ Right motor direction input 1              │
 *   │ RIGHT IN2    │ PB1   │ Right motor direction input 2              │
 *   │ RIGHT ENB    │ PD3   │ OC2B — right motor PWM speed                │
 *   │ Throttle pot │ PC0   │ ADC0 — wiper; outer legs to 5V/GND         │
 *   │ Turn pot     │ PC1   │ ADC1 — wiper; outer legs to 5V/GND         │
 *   │ TXD          │ PD1   │ USB-serial adapter, 9600 8N1                │
 *   └──────────────┴───────┴──────────────────────────────────────────┘
 *
 * Why LEFT uses OC0A/PD6 and RIGHT uses OC2B/PD3: DCMotor.hpp documents
 * four ATmega328P pins wired to a usable PWM timer channel, split across
 * two independent timers (Timer0: OC0A/OC0B, Timer2: OC2A/OC2B). Picking
 * one channel from EACH timer, rather than two channels off the same
 * timer, means each motor's PWM frequency and duty cycle are configured
 * completely independently — not that it matters for this project's
 * fixed ~977 Hz on both, but it's the more general pattern to reach for.
 *
 * Timing model: like project 4, a fixed-rate loop driven by millis()
 * (identical Timer0-overflow technique to examples/timer/02 and
 * examples/Modules/Button/03) — here used to sample both pots and re-mix
 * both motors on a steady CONTROL_PERIOD_MS cadence, the same idea a
 * real RC receiver or motor controller uses internally rather than
 * reacting to every single loop iteration's ADC noise.
 *
 * Arcade-drive mixing, the actual point of this project:
 *   leftSpeed  = clamp(throttle + turn, -100, 100)
 *   rightSpeed = clamp(throttle - turn, -100, 100)
 * Full-forward throttle with turn centered drives both wheels equally
 * forward. Turn alone (throttle centered) spins the two wheels in
 * opposite directions — an in-place pivot. Any blend of the two mixes
 * proportionally, exactly like a real two-joystick-axis tank drive.
 *
 * DCMotor concepts reused from projects 1-4:
 *   - DCMotor(in1, in2, pwmPin), begin(), speed(pct) — called on TWO
 *     independent instances every control period, each fed its own
 *     mixed value.
 *
 * ADC concepts reused from project 3:
 *   - ADCDriver::begin(), read(channel) — this time on two channels
 *     (0 and 1) instead of one.
 */

#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <mikroduino/adc.hpp>
#include <mikroduino/usart.hpp>
#include <mikroduino/timer.hpp>
#include <mikroduino/registers.hpp>
#include <DCMotor.hpp>

using namespace MikroDuino;

DCMotor left (PD4, PD5, PD6);   // OC0A
DCMotor right(PB0, PB1, PD3);   // OC2B
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

// Reads a 0-1023 ADC channel and re-centres it to -100..+100, treating
// the pot's midpoint (~512) as zero — the "spring-centred joystick axis"
// convention both throttle and turn use.
static int8_t readAxis(uint8_t channel) {
    int16_t raw      = static_cast<int16_t>(adc.read(channel)) - 512;   // -512..+511
    int16_t centered = (raw * 100) / 512;
    if (centered >  100) centered =  100;
    if (centered < -100) centered = -100;
    return static_cast<int8_t>(centered);
}

static int8_t clampPct(int16_t v) {
    if (v >  100) return  100;
    if (v < -100) return -100;
    return static_cast<int8_t>(v);
}

static constexpr uint16_t CONTROL_PERIOD_MS = 20;   // 50 Hz control loop

int main() {
    USART0.begin(9600);
    USART0.writeLine_P(PSTR("DCMotor dual-motor arcade drive"));
    USART0.writeLine_P(PSTR("================================="));
    USART0.writeLine_P(PSTR("Throttle pot (ADC0) + turn pot (ADC1) -> left/right wheel mix"));
    USART0.writeLine_P(PSTR(""));

    left.begin();
    right.begin();
    adc.begin();

    Timer0.mode(TimerMode::Normal);
    Timer0.prescaler(TimerPrescaler::DIV64);
    Timer0.start();
    Timer0.enableOverflow();
    sei();

    uint32_t nextTickAt = millis();
    int8_t   lastLeft = 0, lastRight = 0;

    while (true) {
        uint32_t now = millis();
        if (now < nextTickAt) continue;
        nextTickAt = now + CONTROL_PERIOD_MS;

        int8_t throttle = readAxis(0);
        int8_t turn     = readAxis(1);

        int8_t leftSpeed  = clampPct(static_cast<int16_t>(throttle) + turn);
        int8_t rightSpeed = clampPct(static_cast<int16_t>(throttle) - turn);

        left.speed(leftSpeed);
        right.speed(rightSpeed);

        if (leftSpeed != lastLeft || rightSpeed != lastRight) {
            lastLeft  = leftSpeed;
            lastRight = rightSpeed;
            USART0.write_P(PSTR("L="));
            USART0.writeInt(leftSpeed);
            USART0.write_P(PSTR("%  R="));
            USART0.writeInt(rightSpeed);
            USART0.writeLine_P(PSTR("%"));
        }
    }
}
