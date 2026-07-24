/*
 * IRRemote Basics — IRReceiver Bit-Bang poll() — MikroDuino Module SDK
 *
 * The simplest possible use of the IRRemote module: a TSOP38238/VS1838B-
 * style 38 kHz IR receiver module wired to any GPIO pin, decoded entirely
 * in software with no timer and no interrupt. Every button press (and
 * every auto-repeat frame while the button stays held) is printed as a
 * raw address/command pair over USART. This is project 1 of 6 in the
 * examples/Modules/IRRemote series, which walks IRRemote/IRReceiver from
 * a single polled software decode up to a capstone IR-remote-controlled
 * car dashboard combining IRRemote, PWM1, and DCMotor.
 *
 * This project doubles as a practical tool: point ANY NEC remote at the
 * receiver and this sketch prints its address/command bytes in hex —
 * exactly how project 2 discovered the button codes for the reference
 * remote its own lookup table is built around, and how you'd discover
 * the codes for a DIFFERENT remote of your own.
 *
 * Hardware (ATmega328P @ 16 MHz, one 3-pin IR receiver module):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ OUT     │ PD2   │ IRReceiver enables the internal pull-up     │
 *   │         │       │ itself in begin() — no external pull-up or  │
 *   │         │       │ resistor needed for most receiver modules.  │
 *   │ VCC     │ —     │ 5V (3.3V for some parts — check datasheet) │
 *   │ GND     │ —     │ Ground                                     │
 *   │ TXD     │ PD1   │ USB-serial adapter, 9600 8N1, for readings │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * IRRemote concepts introduced:
 *   - NECCode — the shared result type both IRReceiver and IRRemote
 *     return: address (8-bit device address), command (8-bit button
 *     code), valid (true if the NEC checksum inversions both pass), and
 *     repeat (true if this frame was a "button still held" auto-repeat
 *     rather than a fresh press — NEC sends the full 32-bit code only
 *     ONCE per press, then short 2-frame "still held" pulses roughly
 *     every 108 ms for as long as the button stays down).
 *   - IRReceiver(gpioPin) — an INSTANCE class (unlike project 3's static
 *     IRRemote), so more than one could coexist on different pins if a
 *     project ever needed two independent receivers. Works on ANY
 *     MikroDuino GPIO pin — no restriction to INT0/INT1.
 *   - begin() — configures the pin as an input with the internal
 *     pull-up. That's the ENTIRE hardware setup: no timer, no external
 *     interrupt, unlike project 3 onward.
 *   - poll() — non-blocking. Returns an empty/invalid NECCode
 *     immediately if the pin currently reads HIGH (idle, nothing being
 *     received right now). If the pin is LOW when poll() is called, a
 *     frame is already starting, so poll() busy-waits through the
 *     ENTIRE decode (roughly 70 ms for a full 32-bit NEC frame) before
 *     returning. The module header recommends calling poll() at ≤ 1 ms
 *     intervals so a frame's very first falling edge is never missed —
 *     this project's tight `_delay_ms(1)` loop satisfies that directly.
 */

#include <util/delay.h>
#include <mikroduino/usart.hpp>
#include <IRRemote.hpp>

using namespace MikroDuino;

IRReceiver irReceiver(PD2);

int main() {
    USART0.begin(9600);
    USART0.writeLine_P(PSTR("IRRemote basic bit-bang poll"));
    USART0.writeLine_P(PSTR("============================"));
    USART0.writeLine_P(PSTR("Point any NEC remote at the receiver and press a button."));
    USART0.writeLine_P(PSTR(""));

    irReceiver.begin();

    while (true) {
        NECCode c = irReceiver.poll();

        if (c.valid) {
            USART0.write_P(PSTR("addr=0x"));
            USART0.writeInt(static_cast<int32_t>(c.address), 16);
            USART0.write_P(PSTR("  cmd=0x"));
            USART0.writeInt(static_cast<int32_t>(c.command), 16);
            USART0.writeLine_P(c.repeat ? PSTR("  (repeat)") : PSTR("  (new press)"));
        }

        _delay_ms(1);   // poll() must be called at <= 1 ms intervals per the module header
    }
}
