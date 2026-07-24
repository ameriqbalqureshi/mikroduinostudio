/*
 * Servo Custom Pulse Range — writeMicroseconds(), readMicroseconds() —
 * MikroDuino Module SDK
 *
 * Project 2 of 6 in the examples/Modules/Servo series. Not every servo
 * agrees on 1000-2000 us for its full 0-180 degree travel — many
 * "digital" and wide-range hobby servos are rated for something wider,
 * like 600-2400 us, and using the wrong range either wastes part of
 * the servo's real travel or, worse, commands it past its mechanical
 * end-stop. This project overrides the constructor's default range and
 * drives the servo directly in microseconds instead of degrees, so the
 * exact pulse width being sent is never in doubt.
 *
 * Hardware: identical to project 1 (ATmega328P @ 16 MHz):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ OC1A    │ PB1   │ Servo signal wire — Servo channel 0        │
 *   │ —       │ —     │ External 5V supply + common GND (see       │
 *   │         │       │ project 1's wiring note)                    │
 *   │ TXD     │ PD1   │ USB-serial adapter, 9600 8N1                │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * Servo concepts introduced:
 *   - Servo(channel, minUs, maxUs) — this project passes 600 and 2400
 *     explicitly instead of relying on the 1000/2000 default, matching
 *     a wide-range servo's datasheet.
 *   - writeMicroseconds(us) — sets the pulse width directly, clamped to
 *     [minUs, maxUs] exactly like write(angleDeg) is, but without going
 *     through the 0-180 degree mapping at all. Useful for continuous-
 *     rotation servos (where "degrees" has no meaning — the pulse width
 *     controls speed/direction instead of position) or simply when the
 *     calling code already has a microsecond value from elsewhere (a
 *     lookup table, a received command, etc.).
 *   - readMicroseconds() — returns the last commanded pulse width
 *     (whichever of write() or writeMicroseconds() set it), letting the
 *     application report or log the servo's current target without
 *     keeping its own separate copy.
 *
 * This project steps through five fixed pulse widths spanning the full
 * custom range and prints each one — both the raw microsecond value
 * sent and what readMicroseconds() reports back — to make the 1:1
 * relationship between the two explicit.
 */

#include <util/delay.h>
#include <avr/pgmspace.h>
#include <mikroduino/usart.hpp>
#include <Servo.hpp>

using namespace MikroDuino;

// A wide-range "digital" servo: 600 us at one end-stop, 2400 us at the
// other, instead of the 1000-2000 us hobby-servo default.
Servo gimbal(0, 600, 2400);

static void moveTo(uint16_t us) {
    gimbal.writeMicroseconds(us);

    USART0.write_P(PSTR("commanded="));
    USART0.writeInt(us);
    USART0.write_P(PSTR(" us   readMicroseconds()="));
    USART0.writeInt(gimbal.readMicroseconds());
    USART0.writeLine_P(PSTR(" us"));

    _delay_ms(1500);   // give the servo time to physically get there
}

int main() {
    gimbal.begin();

    USART0.begin(9600);
    USART0.writeLine_P(PSTR("Servo custom pulse range (600-2400 us)"));
    USART0.writeLine_P(PSTR("========================================"));

    while (true) {
        moveTo(600);    // one mechanical end-stop
        moveTo(1200);
        moveTo(1500);   // center
        moveTo(1800);
        moveTo(2400);   // other end-stop

        // A pulse width outside [600, 2400] is clamped, not rejected —
        // demonstrate that a deliberately out-of-range request (3000 us,
        // beyond maxUs) still lands safely at the 2400 us end-stop.
        moveTo(3000);
    }
}
