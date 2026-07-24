/*
 * IRRemote Interrupt-Driven LED Toggle — IRRemote Static Class — MikroDuino
 * Module SDK
 *
 * Project 3 of 6 in the examples/Modules/IRRemote series. Swaps projects
 * 1-2's software bit-bang IRReceiver for the OTHER class this module
 * provides — IRRemote — which decodes NEC frames entirely in hardware
 * interrupts (Timer2 + INT0), leaving main() completely free to do other
 * work while a frame is being received in the background.
 *
 * Hardware (ATmega328P @ 16 MHz — same IR receiver wiring as projects
 * 1-2, but the pin is no longer an arbitrary choice):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ OUT     │ PD2   │ MUST be PD2 (INT0) unless MD_IR_USE_INT1    │
 *   │         │       │ is #defined before including IRRemote.hpp, │
 *   │         │       │ in which case it must be PD3 (INT1) instead │
 *   │ LED     │ PB5   │ Toggles on PLAY/PAUSE, ignores auto-repeat  │
 *   │ TXD     │ PD1   │ USB-serial adapter, 9600 8N1                │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * Why the pin is no longer free to choose: IRRemote is a STATIC class
 * (project 1-2's IRReceiver was an instance you could construct on any
 * pin) built around one specific piece of hardware wired permanently to
 * INT0 — the ISR fires on every edge, and Timer2 timestamps each edge to
 * measure NEC's mark/space pulse widths in the background, with no
 * busy-waiting anywhere. That's the entire trade: pick a fixed pin +
 * timer, get non-blocking decoding for free.
 *
 * IRRemote concepts introduced:
 *   - IRRemote::begin() — configures Timer2 (Normal mode, prescaler 8 —
 *     0.5 µs/tick at 16 MHz) and the external interrupt (INT0 by
 *     default). Unlike every other MikroDuino peripheral class, IRRemote
 *     is used entirely through STATIC methods — there's exactly one IR
 *     receiver a given MCU can decode via this class, so there's no
 *     instance to construct.
 *   - sei() — must be called after begin(), same as any other
 *     interrupt-driven MikroDuino peripheral (Button/DHT22/HCSR04's
 *     non-blocking projects all do the same).
 *   - available() — true once a complete frame (data or repeat) has
 *     been decoded by the ISR in the background. Checked every pass
 *     through main()'s loop with NO blocking and NO fixed polling
 *     interval requirement, unlike IRReceiver::poll()'s "call at <= 1 ms"
 *     rule from project 1 — the ISR is what's actually timing things now.
 *   - read() — returns the decoded NECCode (same struct as projects 1-2)
 *     and clears available()'s flag. Only call it once available() is
 *     true.
 *
 * Timer/pin note this whole series depends on: IRRemote's header
 * documents that it uses Timer2, which conflicts ONLY with DCMotor
 * speed control wired to OC2A/OC2B — OC0A/OC0B are unaffected. Project 5
 * picks DCMotor's NO_PWM (digital-only) mode specifically to sidestep
 * needing ANY additional timer at all; the capstone (project 6) needs
 * real PWM speed control and reaches for Timer1 (PWM1Driver) directly
 * instead, for the same reason.
 *
 * New application logic (not part of IRRemote itself): only react to
 * PLAY/PAUSE's brand new press (c.repeat == false). If this project
 * toggled the LED on EVERY repeat frame too, holding the button down
 * would flicker the LED roughly nine times a second instead of doing
 * one clean toggle per press-and-release — repeat frames matter for
 * "is this button still held" (see projects 4-6), not for a one-shot
 * toggle action like this one.
 */

#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/usart.hpp>
#include <IRRemote.hpp>

using namespace MikroDuino;

static constexpr uint8_t LED = PB5;
static constexpr uint8_t DEVICE_ADDRESS  = 0x00;
static constexpr uint8_t CMD_PLAY_PAUSE  = 0x43;   // reference 21-key remote

int main() {
    USART0.begin(9600);
    USART0.writeLine_P(PSTR("IRRemote interrupt-driven LED toggle"));
    USART0.writeLine_P(PSTR("====================================="));
    USART0.writeLine_P(PSTR("Press PLAY/PAUSE to toggle the LED."));
    USART0.writeLine_P(PSTR(""));

    GPIO::output(LED);
    GPIO::clear(LED);

    IRRemote::begin();
    sei();

    while (true) {
        if (IRRemote::available()) {
            NECCode c = IRRemote::read();

            if (c.valid && !c.repeat && c.address == DEVICE_ADDRESS) {
                if (c.command == CMD_PLAY_PAUSE) {
                    GPIO::toggle(LED);
                    USART0.writeLine_P(PSTR("LED toggled"));
                } else {
                    USART0.write_P(PSTR("Other button: 0x"));
                    USART0.writeInt(static_cast<int32_t>(c.command), 16);
                    USART0.writeLine_P(PSTR(""));
                }
            }
        }

        // main() is completely free here — a real project could be doing
        // anything else in this loop; the IR frame is decoded entirely by
        // interrupts in the background, unlike projects 1-2's poll()/
        // receive() which both need main()'s attention while decoding.
    }
}
