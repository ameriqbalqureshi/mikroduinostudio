/*
 * IRRemote Remote-Controlled DC Motor Car — DCMotor NO_PWM + Auto-Stop-
 * On-Release — MikroDuino Module SDK
 *
 * Project 5 of 6 in the examples/Modules/IRRemote series. Points project
 * 4's "detect a release from NEC's repeat frames" technique at something
 * that actually matters: a two-wheel remote-controlled car that MUST
 * stop driving the instant the operator lets go of the remote, even
 * though NEC itself never sends an explicit "button up" signal.
 *
 * Hardware (ATmega328P @ 16 MHz, two independent H-bridge motor
 * channels — e.g. both channels of one L298N):
 *
 *   ┌──────────────┬───────┬──────────────────────────────────────────┐
 *   │ IR OUT       │ PD2   │ INT0, same fixed pin as projects 3-4        │
 *   │ LEFT IN1     │ PD4   │ Left motor direction input 1                │
 *   │ LEFT IN2     │ PD5   │ Left motor direction input 2                │
 *   │ RIGHT IN1    │ PD6   │ Right motor direction input 1               │
 *   │ RIGHT IN2    │ PD7   │ Right motor direction input 2               │
 *   │ TXD          │ PD1   │ USB-serial adapter, 9600 8N1                │
 *   └──────────────┴───────┴──────────────────────────────────────────┘
 *
 * Driven with the reference remote's number pad as a D-pad — a common
 * convention on cheap Arduino "IR remote car" kits built around this
 * exact 21-key remote:
 *   2 = forward, 8 = backward, 4 = pivot left, 6 = pivot right,
 *   5 = stop (explicit, in addition to the automatic release-stop below)
 *
 * Why NO_PWM (full-speed-only) motors, not project 3's DCMotor(in1, in2,
 * pwmPin) three-argument form: IRRemote already owns Timer2 for its NEC
 * pulse timing (see project 3's header comment), and this project also
 * wants Timer0 free for its own millis()-based release watchdog — which
 * leaves NEITHER of DCMotor's two supported PWM timers (Timer0 or
 * Timer2; see DCMotor.hpp's own header comment) available for speed
 * control. Rather than fight over a timer, this project simply doesn't
 * ask for one: DCMotor(in1, in2) with no third argument drives IN1/IN2
 * as plain digital direction pins and speed(pct) becomes full-speed-
 * forward, full-speed-reverse, or coast — nothing in between, and no
 * timer touched at all. The capstone (project 6) solves the same
 * problem differently, by reaching for Timer1 instead.
 *
 * New concept: reusing project 4's release-timeout technique for
 * something safety-critical instead of a status message. Every accepted
 * D-pad frame (new press OR matching repeat) stamps `lastFrameAt`; a
 * plain millis() check elsewhere in the loop calls `stopDriving()` the
 * instant `now - lastFrameAt` exceeds RELEASE_TIMEOUT_MS — exactly
 * project 4's pattern, but now driving two motors to a hard coast()
 * instead of just leaving an LED's brightness wherever it was. A car
 * that kept driving forward because the last "forward" repeat happened
 * to get lost in the air is a real hazard project 4's LED never had.
 *
 * IRRemote concepts reused from projects 3-4:
 *   - IRRemote::begin(), sei(), available(), read() — identical usage.
 *   - c.repeat: a brand new press (repeat==false) can switch to ANY
 *     direction immediately (e.g. forward straight into reverse without
 *     an explicit stop first); a repeat only refreshes lastFrameAt if it
 *     matches the direction already being driven, exactly like project
 *     4's matching-repeat guard.
 *
 * DCMotor concepts reused from the examples/Modules/DCMotor series:
 *   - DCMotor(in1, in2), begin(), speed(pct), coast() — this project
 *     only ever calls speed(100)/speed(-100)/coast(), since NO_PWM mode
 *     has no in-between speed to ask for.
 */

#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <mikroduino/usart.hpp>
#include <mikroduino/timer.hpp>
#include <mikroduino/registers.hpp>
#include <IRRemote.hpp>
#include <DCMotor.hpp>

using namespace MikroDuino;

static constexpr uint8_t DEVICE_ADDRESS = 0x00;
static constexpr uint8_t CMD_FORWARD    = 0x18;   // '2'
static constexpr uint8_t CMD_BACKWARD   = 0x52;   // '8'
static constexpr uint8_t CMD_PIVOT_LEFT = 0x08;   // '4'
static constexpr uint8_t CMD_PIVOT_RIGHT= 0x5A;   // '6'
static constexpr uint8_t CMD_STOP       = 0x1C;   // '5'

static constexpr uint16_t RELEASE_TIMEOUT_MS = 200;   // > NEC's ~108 ms repeat gap

DCMotor leftMotor (PD4, PD5);    // NO_PWM: direction only, full speed
DCMotor rightMotor(PD6, PD7);    // NO_PWM: direction only, full speed

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

enum Direction : uint8_t { NONE, FORWARD, BACKWARD, PIVOT_LEFT, PIVOT_RIGHT };

static void drive(Direction dir) {
    switch (dir) {
        case FORWARD:     leftMotor.speed(100);  rightMotor.speed(100);  break;
        case BACKWARD:    leftMotor.speed(-100); rightMotor.speed(-100); break;
        case PIVOT_LEFT:  leftMotor.speed(-100); rightMotor.speed(100);  break;
        case PIVOT_RIGHT: leftMotor.speed(100);  rightMotor.speed(-100); break;
        case NONE:        leftMotor.coast();     rightMotor.coast();    break;
    }
}

static Direction commandToDirection(uint8_t cmd) {
    switch (cmd) {
        case CMD_FORWARD:     return FORWARD;
        case CMD_BACKWARD:    return BACKWARD;
        case CMD_PIVOT_LEFT:  return PIVOT_LEFT;
        case CMD_PIVOT_RIGHT: return PIVOT_RIGHT;
        default:              return NONE;
    }
}

int main() {
    USART0.begin(9600);
    USART0.writeLine_P(PSTR("IRRemote remote-controlled DC motor car"));
    USART0.writeLine_P(PSTR("========================================"));
    USART0.writeLine_P(PSTR("2=fwd 8=back 4=pivot-L 6=pivot-R 5=stop"));
    USART0.writeLine_P(PSTR(""));

    IRRemote::begin();

    leftMotor.begin();
    rightMotor.begin();

    Timer0.mode(TimerMode::Normal);
    Timer0.prescaler(TimerPrescaler::DIV64);
    Timer0.start();
    Timer0.enableOverflow();
    sei();

    Direction current     = NONE;
    uint32_t  lastFrameAt = millis();

    while (true) {
        uint32_t now = millis();

        if (IRRemote::available()) {
            NECCode c = IRRemote::read();

            if (c.valid && c.address == DEVICE_ADDRESS) {
                if (c.command == CMD_STOP && !c.repeat) {
                    current = NONE;
                    drive(current);
                    USART0.writeLine_P(PSTR("STOP"));
                } else if (!c.repeat) {
                    // Brand new press: switch direction immediately,
                    // even straight from one direction to another.
                    Direction next = commandToDirection(c.command);
                    if (next != NONE) {
                        current = next;
                        lastFrameAt = now;
                        drive(current);
                    }
                } else if (current != NONE && commandToDirection(c.command) == current) {
                    // Matching repeat: the same button is still held down.
                    lastFrameAt = now;
                }
            }
        }

        // ---- Safety: no matching frame for RELEASE_TIMEOUT_MS -> stop ----
        if (current != NONE && (now - lastFrameAt) > RELEASE_TIMEOUT_MS) {
            current = NONE;
            drive(current);
            USART0.writeLine_P(PSTR("released - auto-stopped"));
        }
    }
}
