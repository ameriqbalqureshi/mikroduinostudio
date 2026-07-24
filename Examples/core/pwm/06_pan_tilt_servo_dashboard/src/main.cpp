/*
 * PWM Pan-Tilt Servo Dashboard — MikroDuino SDK (capstone)
 *
 * Project 6 of 6 in the examples/pwm series. Two servos, one per Timer1
 * channel, each driven by its own potentiometer — a classic pan/tilt
 * camera-mount control scheme — plus an idle-detach power/jitter-saving
 * behaviour that exercises stopA()/stopB() in a genuinely useful way
 * rather than just as an API demonstration.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌─────────────┬───────┬──────────────────────────────────────────┐
 *   │ Signal      │ Pin   │ Wiring                                    │
 *   ├─────────────┼───────┼──────────────────────────────────────────┤
 *   │ OC1A        │ PB1   │ Pan servo signal wire                     │
 *   │ OC1B        │ PB2   │ Tilt servo signal wire                    │
 *   │ —           │ —     │ Both servos: +5V from an external supply, │
 *   │             │       │ GND common with the ATmega — see project  │
 *   │             │       │ 5's wiring note; two servos draw more     │
 *   │             │       │ current than the board's own regulator    │
 *   │             │       │ can safely supply.                        │
 *   │ ADC0        │ PC0   │ Pan potentiometer wiper                    │
 *   │ ADC1        │ PC1   │ Tilt potentiometer wiper                   │
 *   │ TXD         │ PD1   │ USB-serial adapter                        │
 *   └─────────────┴───────┴──────────────────────────────────────────┘
 *
 * Idle-detach behaviour: real pan/tilt rigs built with hobby servos have
 * a well-known annoyance — a servo actively holding a commanded position
 * draws current continuously and can audibly "buzz"/jitter even when
 * nothing should be moving, because it's constantly making tiny
 * corrections against the PWM signal. The common fix (the same one
 * Arduino's Servo library exposes as detach()) is to stop sending pulses
 * once the target position has been stable for a while, letting the
 * servo's own internal position hold (friction in its gearing) take
 * over, and to resume pulses the instant the input moves again. This
 * project reuses exactly the same PWM1Driver calls project 2 introduced
 * for that: stopA()/stopB() to detach, and a plain rawA()/rawB() call to
 * reattach (no separate "start" API needed — see project 2's header
 * comment on why).
 *
 * PWM concepts reused from the whole series:
 *   - PWM1.begin(50, PWMType::FastPWM) — one shared 50 Hz servo period
 *     for both channels (project 5).
 *   - PWM1.top(), PWM1.rawA()/rawB() — microsecond-accurate pulse widths
 *     for each channel independently (projects 3 and 5).
 *   - PWM1.stopA()/stopB() — per-channel detach, each independent of the
 *     other (project 2), now driven by real idle-detection logic instead
 *     of a fixed demo sequence.
 *
 * Non-PWM library reused from project 5:
 *   - ADCDriver — two channels (ADC0 = pan, ADC1 = tilt) read every loop
 *     iteration to drive the two servos independently.
 */

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <mikroduino/pwm.hpp>
#include <mikroduino/adc.hpp>
#include <mikroduino/usart.hpp>

using namespace MikroDuino;

static constexpr uint8_t PAN_CHANNEL  = 0;   // ADC0 / PC0
static constexpr uint8_t TILT_CHANNEL = 1;   // ADC1 / PC1

static constexpr uint16_t SERVO_MIN_US    = 1000;
static constexpr uint16_t SERVO_MAX_US    = 2000;
static constexpr uint16_t SERVO_PERIOD_US = 20000;   // 50 Hz

// How much an ADC reading has to move (out of 0-1023) before it counts
// as intentional operator input rather than potentiometer wiper noise.
static constexpr uint16_t MOVEMENT_THRESHOLD = 4;

// Loop runs on a 100 ms cadence (see delay at the bottom); 30 consecutive
// "no movement" iterations is therefore about 3 seconds of stillness
// before a channel detaches.
static constexpr uint8_t IDLE_ITERATIONS_BEFORE_DETACH = 30;

static void delay_ms(uint16_t ms) { while (ms--) _delay_ms(1); }

static uint16_t usToTicks(uint16_t microseconds) {
    uint32_t top = PWM1.top();
    return static_cast<uint16_t>((top * microseconds) / SERVO_PERIOD_US);
}

static uint16_t potToPulseUs(uint16_t potValue) {
    return static_cast<uint16_t>(
        SERVO_MIN_US + (static_cast<uint32_t>(potValue) * (SERVO_MAX_US - SERVO_MIN_US)) / 1023u
    );
}

// Per-axis state: last reading (to detect movement), idle counter, and
// whether the channel is currently attached (actively driven) or
// detached (stopped, holding by its own gear friction).
struct ServoAxis {
    uint16_t lastPot;
    uint8_t  idleCount;
    bool     attached;
    const char* name;   // flash-resident label, printed with write_P()
};

static const char AXIS_PAN[]  PROGMEM = "PAN ";
static const char AXIS_TILT[] PROGMEM = "TILT";

static ServoAxis panAxis  = { 0, 0, true, AXIS_PAN  };
static ServoAxis tiltAxis = { 0, 0, true, AXIS_TILT };

// Update one axis: read its pot, decide attach/detach, and if attached,
// drive the servo to match. Returns the pulse width actually applied (or
// the last one, while detached) for the status line.
static uint16_t serviceAxis(ServoAxis& axis, uint8_t adcChannel,
                             void (*driveRaw)(uint16_t ticks),
                             void (*stopChannel)()) {
    uint16_t potValue = ADC_Driver.read(adcChannel);

    uint16_t delta = (potValue > axis.lastPot) ? (potValue - axis.lastPot)
                                                : (axis.lastPot - potValue);

    if (delta >= MOVEMENT_THRESHOLD) {
        axis.idleCount = 0;
        if (!axis.attached) {
            axis.attached = true;
            USART0.write_P(axis.name);
            USART0.writeLine_P(PSTR(": movement detected -> reattached"));
        }
    } else if (axis.idleCount < 0xFF) {
        ++axis.idleCount;
    }

    axis.lastPot = potValue;

    if (axis.attached && axis.idleCount >= IDLE_ITERATIONS_BEFORE_DETACH) {
        axis.attached = false;
        stopChannel();
        USART0.write_P(axis.name);
        USART0.writeLine_P(PSTR(": idle -> detached (stopA()/stopB())"));
    }

    uint16_t pulseUs = potToPulseUs(potValue);
    if (axis.attached) {
        driveRaw(usToTicks(pulseUs));
    }
    return pulseUs;
}

// PWM1.rawA()/rawB()/stopA()/stopB() are member functions on the global
// PWM1 instance, not plain functions — serviceAxis() takes plain function
// pointers so it can treat both axes identically, so each PWM1 call is
// wrapped in a tiny free function to get a pointer to.
static void driveA(uint16_t ticks) { PWM1.rawA(ticks); }
static void driveB(uint16_t ticks) { PWM1.rawB(ticks); }
static void stopA()  { PWM1.stopA(); }
static void stopB()  { PWM1.stopB(); }

int main() {
    ADC_Driver.begin();
    PWM1.begin(50, PWMType::FastPWM);

    // Establish an initial attached position on both channels before the
    // idle-detection logic starts running, so the mount doesn't sit
    // undriven (and un-positioned) on the very first loop iteration.
    panAxis.lastPot  = ADC_Driver.read(PAN_CHANNEL);
    tiltAxis.lastPot = ADC_Driver.read(TILT_CHANNEL);
    PWM1.rawA(usToTicks(potToPulseUs(panAxis.lastPot)));
    PWM1.rawB(usToTicks(potToPulseUs(tiltAxis.lastPot)));

    USART0.begin(9600);
    USART0.writeLine_P(PSTR("PWM pan-tilt servo dashboard"));
    USART0.writeLine_P(PSTR("=============================="));
    USART0.writeLine_P(PSTR("Move either potentiometer. Hold still ~3s to auto-detach that axis."));

    while (true) {
        uint16_t panPulse  = serviceAxis(panAxis,  PAN_CHANNEL,  driveA, stopA);
        uint16_t tiltPulse = serviceAxis(tiltAxis, TILT_CHANNEL, driveB, stopB);

        USART0.write_P(PSTR("pan="));
        USART0.writeInt(panPulse);
        USART0.write_P(PSTR("us ("));
        USART0.write_P(panAxis.attached ? PSTR("attached") : PSTR("detached"));
        USART0.write_P(PSTR(")  tilt="));
        USART0.writeInt(tiltPulse);
        USART0.write_P(PSTR("us ("));
        USART0.write_P(tiltAxis.attached ? PSTR("attached") : PSTR("detached"));
        USART0.writeLine_P(PSTR(")"));

        delay_ms(100);
    }
}
