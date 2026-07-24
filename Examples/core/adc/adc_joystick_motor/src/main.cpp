/*
 * ADC Example 6 (Capstone) — Analog Joystick Robot Controller
 * MikroDuino SDK
 *
 * The capstone of this ADC series: two analog channels drive a full
 * differential-drive robot, tying the ADC driver together with GPIO
 * (H-bridge direction control), PWM1 (motor speed), and USART (telemetry) —
 * the same peripherals covered in their own dedicated example sets.
 * Compared to Examples 1-5, the new ADC-specific technique here is
 * *auto-calibration*: every potentiometer/joystick has some center-position
 * error, so instead of assuming raw 512 = "centered", we measure the actual
 * idle center at startup and use it as the reference for a dead-zone.
 *
 * Real-world application: a 2-wheel-drive robot / RC car controller — the
 * same X/Y analog joystick + H-bridge + PWM architecture used in hobby
 * robotics, wheelchairs, and pan/tilt camera rigs.
 *
 * Hardware (ATmega328P / Arduino Nano @ 16 MHz):
 *
 *   ┌────────────────┬───────┬────────────────────────────────────────────┐
 *   │ Signal         │ Pin   │ Wiring                                     │
 *   ├────────────────┼───────┼────────────────────────────────────────────┤
 *   │ Joystick X     │ A6    │ Wiper of X-axis pot; ends to GND and VCC   │
 *   │ Joystick Y     │ A7    │ Wiper of Y-axis pot; ends to GND and VCC   │
 *   │ Left motor PWM │ PB1   │ OC1A -> H-bridge (e.g. L298N) ENA          │
 *   │ Left DIR FWD   │ PD2   │ H-bridge IN1                               │
 *   │ Left DIR REV   │ PD3   │ H-bridge IN2                               │
 *   │ Right motor PWM│ PB2   │ OC1B -> H-bridge ENB                       │
 *   │ Right DIR FWD  │ PD4   │ H-bridge IN3                               │
 *   │ Right DIR REV  │ PD5   │ H-bridge IN4                               │
 *   │ USART0 TX      │ PD1   │ Telemetry to PC via USB-serial, 9600 8N1  │
 *   └────────────────┴───────┴────────────────────────────────────────────┘
 *
 * ADC features used (all reused from Examples 1-5, applied to a real
 * control loop instead of a display):
 *   ADC_Driver.begin(ref, prescaler, leftAdjust)
 *   ADC_Driver.read(channel)   — polled every loop for both joystick axes
 *
 * The new idea in this example is entirely in how the raw ADC counts are
 * *used*: startup auto-calibration, a dead-zone, and mixing two analog axes
 * into differential motor commands.
 */

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/adc.hpp>
#include <mikroduino/pwm.hpp>
#include <mikroduino/usart.hpp>

using namespace MikroDuino;

#define WP(s)   USART0.write_P(PSTR(s))
#define WLP(s)  USART0.writeLine_P(PSTR(s))

// ── ADC channels ────────────────────────────────────────────────────────
static constexpr uint8_t CH_X = 6;   // A6 — joystick X (steering)
static constexpr uint8_t CH_Y = 7;   // A7 — joystick Y (throttle)

// ── Motor driver pins ───────────────────────────────────────────────────
static constexpr uint8_t LEFT_DIR_FWD  = PD2;
static constexpr uint8_t LEFT_DIR_REV  = PD3;
static constexpr uint8_t RIGHT_DIR_FWD = PD4;
static constexpr uint8_t RIGHT_DIR_REV = PD5;

// Joystick counts are noisy right around center; anything within this many
// counts of the calibrated center is treated as exactly centered.
static constexpr int16_t DEAD_ZONE = 25;

// ── Auto-calibration ────────────────────────────────────────────────────
// Every joystick's mechanical center sits at a slightly different raw ADC
// value. Instead of hard-coding 512, sample the idle position at startup
// and use the measured value as "zero" for both axes.
struct Calibration {
    uint16_t centerX;
    uint16_t centerY;
};

static Calibration calibrate() {
    ADC_Driver.begin(ADCRef::AVCC, ADCPrescaler::DIV128, false);

    uint32_t sumX = 0, sumY = 0;
    static constexpr uint8_t SAMPLES = 32;
    for (uint8_t i = 0; i < SAMPLES; ++i) {
        sumX += ADC_Driver.read(CH_X);
        sumY += ADC_Driver.read(CH_Y);
        _delay_ms(2);
    }

    Calibration cal;
    cal.centerX = static_cast<uint16_t>(sumX / SAMPLES);
    cal.centerY = static_cast<uint16_t>(sumY / SAMPLES);
    return cal;
}

// Convert a raw ADC axis reading + its calibrated center into a signed
// -255..+255 stick deflection, with the dead-zone collapsed to exactly 0.
static int16_t axis_to_signed(uint16_t raw, uint16_t center) {
    int16_t delta = static_cast<int16_t>(raw) - static_cast<int16_t>(center);
    if (delta > -DEAD_ZONE && delta < DEAD_ZONE) return 0;

    // Scale the +-512-ish range down to +-255 for motor math. Clamp because
    // the calibrated center shifts the usable range slightly off-symmetric.
    int32_t scaled = (static_cast<int32_t>(delta) * 255) / 512;
    if (scaled > 255)  scaled = 255;
    if (scaled < -255) scaled = -255;
    return static_cast<int16_t>(scaled);
}

// Drive one motor: sign of `signedSpeed` picks direction, magnitude (0-255)
// becomes a 0-100% PWM duty cycle.
static void drive_motor(int16_t signedSpeed, uint8_t dirFwdPin, uint8_t dirRevPin,
                         void (*dutyFn)(uint8_t)) {
    uint8_t mag = static_cast<uint8_t>((signedSpeed < 0) ? -signedSpeed : signedSpeed);
    uint8_t duty_pct = static_cast<uint8_t>((static_cast<uint16_t>(mag) * 100u) / 255u);

    GPIO::write(dirFwdPin, signedSpeed > 0);
    GPIO::write(dirRevPin, signedSpeed < 0);
    dutyFn(duty_pct);
}

static void set_left_duty(uint8_t pct)  { PWM1.dutyA(pct); }
static void set_right_duty(uint8_t pct) { PWM1.dutyB(pct); }

int main() {
    GPIO::output(LEFT_DIR_FWD);
    GPIO::output(LEFT_DIR_REV);
    GPIO::output(RIGHT_DIR_FWD);
    GPIO::output(RIGHT_DIR_REV);

    USART0.begin(9600);
    WLP("MikroDuino Joystick Robot Controller — Example 6/6");
    WLP("Calibrating joystick center... keep it released.");

    Calibration cal = calibrate();

    WP("Calibrated center: X="); USART0.writeInt(cal.centerX);
    WP("  Y=");                 USART0.writeInt(cal.centerY);
    WLP("");

    // 20 kHz is above the range of human hearing, so the motors run silently
    // instead of the audible whine typical of low-frequency PWM.
    PWM1.begin(20000, PWMType::FastPWM);

    while (true) {
        uint16_t rawX = ADC_Driver.read(CH_X);
        uint16_t rawY = ADC_Driver.read(CH_Y);

        int16_t steer    = axis_to_signed(rawX, cal.centerX);
        int16_t throttle = axis_to_signed(rawY, cal.centerY);

        // Classic differential-drive mixing: throttle moves both wheels the
        // same direction, steer speeds up one side and slows the other.
        int16_t leftSpeed  = throttle + steer;
        int16_t rightSpeed = throttle - steer;

        if (leftSpeed  > 255)  leftSpeed  = 255;
        if (leftSpeed  < -255) leftSpeed  = -255;
        if (rightSpeed > 255)  rightSpeed = 255;
        if (rightSpeed < -255) rightSpeed = -255;

        drive_motor(leftSpeed,  LEFT_DIR_FWD,  LEFT_DIR_REV,  set_left_duty);
        drive_motor(rightSpeed, RIGHT_DIR_FWD, RIGHT_DIR_REV, set_right_duty);

        // Telemetry: one line every ~200 ms so it stays readable at 9600 baud.
        WP("L="); USART0.writeInt(leftSpeed);
        WP("  R="); USART0.writeInt(rightSpeed);
        WP("  (steer="); USART0.writeInt(steer);
        WP(" throttle="); USART0.writeInt(throttle);
        WLP(")");

        _delay_ms(200);
    }
}
