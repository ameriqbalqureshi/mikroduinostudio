/*
 * Two HCSR04 Instances — Left/Right Obstacle Avoidance — MikroDuino
 * Module SDK
 *
 * Project 5 of 6 in the examples/Modules/HCSR04 series. Wires up TWO
 * completely independent HCSR04 objects, one aimed left and one aimed
 * right, and pings both once per cycle, printing not just each side's
 * own distance but a plain steering suggestion derived from comparing
 * them — exactly the kind of two-sensor decision a small obstacle-
 * avoidance robot needs before it ever touches a motor driver. Just
 * like examples/Modules/DHT22/05's two independent DHT22Driver objects
 * and examples/Modules/DCMotor/05's two independent DCMotor objects,
 * nothing about using two HCSR04 instances at once is special — each
 * one is a self-contained object over its own pair of pins, and this
 * project exists mainly to make that concrete for a sensor this time.
 *
 * Hardware (ATmega328P @ 16 MHz, two independent HC-SR04 sensors):
 *
 *   ┌───────────────┬───────┬──────────────────────────────────────┐
 *   │ LEFT TRIG     │ PD6   │ Left-facing sensor, trigger (project 1) │
 *   │ LEFT ECHO     │ PD7   │ Left-facing sensor, echo                 │
 *   │ RIGHT TRIG    │ PC0   │ Right-facing sensor, own trigger pin      │
 *   │ RIGHT ECHO    │ PC1   │ Right-facing sensor, own echo pin          │
 *   │ TXD           │ PD1   │ USB-serial adapter, 9600 8N1                │
 *   └───────────────┴───────┴──────────────────────────────────────┘
 *
 * Because each sensor has its own dedicated trigger/echo pair, there's
 * no bus contention to worry about the way a shared bus (I2C, 1-Wire
 * with multiple devices) would need — the two measureCM() calls are
 * simply run back-to-back, each a completely independent ping-and-
 * listen cycle on its own pins. The only shared constraint is the same
 * ~60 ms-minimum spacing project 1 established, which applies to EACH
 * sensor independently — pinging left then right back-to-back inside
 * one cycle is fine; it's calling measureCM() on the SAME sensor object
 * again too soon that would risk hearing an echo from the previous
 * ping.
 *
 * HCSR04 concepts reused from projects 1-4:
 *   - HCSR04(trigPin, echoPin), begin(), measureCM() — used identically
 *     on two separate instances. A 0 reading (no echo) is treated as
 *     "clear" on that side for the steering decision below, same as
 *     it's outside HC-SR04's rated range either way.
 *
 * Steering logic is ordinary application code, not part of the module:
 * if both sides read closer than STOP_CM, there's an obstacle on either
 * side and the only safe suggestion is to stop; otherwise, whichever
 * side is meaningfully closer (by more than TURN_MARGIN_CM, to avoid
 * flapping between suggestions on two nearly-equal readings) is the
 * side to steer AWAY from.
 */

#include <util/delay.h>
#include <mikroduino/usart.hpp>
#include <HCSR04.hpp>

using namespace MikroDuino;

HCSR04 left(PD6, PD7);
HCSR04 right(PC0, PC1);

static constexpr int32_t STOP_CM        = 15;   // obstacle on both sides this close: stop
static constexpr int32_t TURN_MARGIN_CM = 5;    // how much closer one side must be to steer away from it
static constexpr int32_t CLEAR_CM       = 400;  // treat "no echo" as this far away (beyond max range)

static const char* decide(int32_t leftCM, int32_t rightCM) {
    if (leftCM < STOP_CM && rightCM < STOP_CM) return "STOP (blocked both sides)";
    if (rightCM - leftCM > TURN_MARGIN_CM)     return "Steer RIGHT (obstacle on left)";
    if (leftCM - rightCM > TURN_MARGIN_CM)     return "Steer LEFT (obstacle on right)";
    return "Straight (clear)";
}

int main() {
    USART0.begin(9600);
    USART0.writeLine_P(PSTR("HCSR04 dual-sensor obstacle avoidance"));
    USART0.writeLine_P(PSTR("========================================"));
    USART0.writeLine_P(PSTR(""));

    left.begin();
    right.begin();

    while (true) {
        uint32_t leftRaw  = left.measureCM();
        uint32_t rightRaw = right.measureCM();

        int32_t leftCM  = (leftRaw  > 0) ? static_cast<int32_t>(leftRaw)  : CLEAR_CM;
        int32_t rightCM = (rightRaw > 0) ? static_cast<int32_t>(rightRaw) : CLEAR_CM;

        USART0.write_P(PSTR("Left:  "));
        if (leftRaw > 0) { USART0.writeInt(leftCM); USART0.writeLine_P(PSTR(" cm")); }
        else               USART0.writeLine_P(PSTR("no echo (clear or timeout)"));

        USART0.write_P(PSTR("Right: "));
        if (rightRaw > 0) { USART0.writeInt(rightCM); USART0.writeLine_P(PSTR(" cm")); }
        else                USART0.writeLine_P(PSTR("no echo (clear or timeout)"));

        USART0.write_P(PSTR("Decision: "));
        USART0.writeLine(decide(leftCM, rightCM));
        USART0.writeLine_P(PSTR(""));

        // >= HCSR04's ~60 ms recommended spacing, applies per-sensor.
        _delay_ms(150);
    }
}
