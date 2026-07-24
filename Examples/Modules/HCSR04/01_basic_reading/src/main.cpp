/*
 * HCSR04 Basics — Blocking measureCM(), Out-Of-Range Handling —
 * MikroDuino Module SDK
 *
 * The simplest possible use of the HCSR04 module: trigger a ping and
 * print the distance it echoes back, over USART. This is project 1 of 6
 * in the examples/Modules/HCSR04 series, which walks the HCSR04 class
 * from a single blocking measureCM() up to a capstone parking-sensor
 * dashboard combining HCSR04, LCD, Button, and EEPROM.
 *
 * Hardware (ATmega328P @ 16 MHz, HC-SR04 ultrasonic distance sensor):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ Signal  │ Pin   │ Wiring                                    │
 *   ├─────────┼───────┼──────────────────────────────────────────┤
 *   │ TRIG    │ PD6   │ Sensor's trigger input (MCU drives it)     │
 *   │ ECHO    │ PD7   │ Sensor's echo output. HC-SR04 is a 5V       │
 *   │         │       │ part — on a 3.3V MCU this needs a resistor  │
 *   │         │       │ divider or level shifter, this board runs   │
 *   │         │       │ at 5V so ECHO connects straight to PD7      │
 *   │ VCC     │ —     │ 5V                                          │
 *   │ GND     │ —     │ Ground                                      │
 *   │ TXD     │ PD1   │ USB-serial adapter, 9600 8N1, for readings  │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * HCSR04 concepts introduced:
 *   - HCSR04(trigPin, echoPin) — records which pin drives the trigger
 *     and which pin reads the echo. No hardware is touched yet.
 *   - begin() — configures TRIG as an output (held low) and ECHO as an
 *     input. Call this once before the first measurement.
 *   - measureMM() / measureCM() — a BLOCKING call that sends a 10 µs
 *     trigger pulse, then busy-waits for the echo line to go high and
 *     busy-waits AGAIN while it stays high, timing that second wait to
 *     derive distance. The whole round trip typically takes well under
 *     a millisecond for a nearby object, but can take longer for a
 *     distant one, and the call simply blocks the CPU for however long
 *     that takes.
 *   - 0 return value — measureMM()/measureCM() return exactly 0 when no
 *     echo is seen within the internal timeout: either nothing is in
 *     range (HC-SR04's rated maximum is roughly 4 m), the sensor isn't
 *     wired correctly, or the object is closer than the ~2 cm the
 *     transducer can resolve. This project simply skips printing on a
 *     zero reading; there's no way to tell those cases apart from the
 *     return value alone.
 *
 * Timing note this whole series depends on: this driver measures the
 * echo pulse width by polling in fixed 10 µs steps via _delay_us(),
 * which avr-libc calibrates against F_CPU at compile time — so the
 * distance conversion comes out correct in real microseconds on any
 * supported MCU/clock combination, not just the one it happened to be
 * built and tested on. A ping that never gets an echo (no object within
 * roughly 5 m, either wait phase) times out and returns 0 rather than a
 * bogus distance. The datasheet also recommends waiting at least
 * ~60 ms between the start of one ping and the next, to let any echo
 * from the previous ping fully die out before the next trigger; this
 * project waits 200 ms to keep the terminal output easy to read without
 * changing the underlying timing requirement.
 */

#include <util/delay.h>
#include <mikroduino/usart.hpp>
#include <HCSR04.hpp>

using namespace MikroDuino;

HCSR04 sonar(PD6, PD7);

int main() {
    USART0.begin(9600);
    USART0.writeLine_P(PSTR("HCSR04 basic reading"));
    USART0.writeLine_P(PSTR("====================="));
    USART0.writeLine_P(PSTR(""));

    sonar.begin();

    while (true) {
        uint32_t cm = sonar.measureCM();

        if (cm > 0) {
            USART0.write_P(PSTR("Distance: "));
            USART0.writeInt(static_cast<int32_t>(cm));
            USART0.writeLine_P(PSTR(" cm"));
        } else {
            USART0.writeLine_P(PSTR("No echo (out of range or timeout) - skipping"));
        }

        // Datasheet recommends >= ~60 ms between pings; 200 ms leaves margin.
        _delay_ms(200);
    }
}
