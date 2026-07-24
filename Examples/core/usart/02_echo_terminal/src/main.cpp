/*
 * Echo Terminal — MikroDuino SDK (blocking RX, byte I/O)
 *
 * Project 2 of 7 in the examples/usart series. Every character you type
 * into a serial terminal is echoed straight back, letters are also
 * mirrored in UPPER/lower case, and the on-board LED blinks once per
 * received byte. This is the first project in the series that reads
 * from the USART, not just writes to it.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ Signal  │ Pin   │ Wiring                                    │
 *   ├─────────┼───────┼──────────────────────────────────────────┤
 *   │ TXD     │ PD1   │ To RX of USB-serial adapter                │
 *   │ RXD     │ PD0   │ From TX of USB-serial adapter               │
 *   │ LED     │ PB5   │ Built-in LED — one blink per received byte │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * Open a terminal at 9600 8N1 and type. Each character you send comes
 * back immediately, so most terminal programs will show your own
 * keystrokes appearing twice unless local echo is turned off.
 *
 * USART concepts introduced:
 *   - available() — returns true if a byte has arrived (polls the RXC0
 *     flag in UCSR0A). Non-blocking: safe to call every pass through
 *     the main loop even when nothing has been sent.
 *   - rxError() — checks the frame-error / data-overrun / parity-error
 *     flags for the byte that is about to be read. IMPORTANT: this must
 *     be checked *before* the byte is consumed — the error flags belong
 *     to the byte currently sitting in the receive register, and reading
 *     that register clears them.
 *   - read() — blocking receive. If no byte has arrived yet, it simply
 *     waits (spins) until one does. Fine for short waits at a known
 *     point in the program (see wait_for_keypress() below); a bad choice
 *     for a main loop that also has other work to do (project 5 in this
 *     series solves that with an RX interrupt instead).
 *   - readNonBlock(ready) — companion to available(): pass it a bool by
 *     reference, and it reads UDR0 only if a byte was actually waiting,
 *     setting `ready` to say whether it did. Combined with available(),
 *     this is the standard "poll, then consume" pattern used below.
 *   - write(uint8_t) — the raw single-byte send used to echo each
 *     character back individually, as opposed to write(const char*)
 *     which sends a whole null-terminated string.
 *   - txReady() — true once the transmit data register is empty and
 *     ready to accept a new byte without blocking.
 *   - flush() — blocks until the last bit has actually left the wire
 *     (TXC0 flag), as opposed to txReady() which only checks that the
 *     *next* byte can be loaded. Used here before echo_terminal enters
 *     its main loop, to make sure the startup banner is fully sent.
 */

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/usart.hpp>

using namespace MikroDuino;

static constexpr uint8_t LED = PB5;

// Blocks until a byte arrives, then returns it. Demonstrates the plain
// blocking read() — useful at a well-defined point like startup, where
// the program has nothing better to do until the user responds anyway.
static uint8_t wait_for_keypress() {
    return USART0.read();
}

// Flips the case of a letter; passes through anything else unchanged.
// Gives the echoed text a visible transformation, so it is obvious the
// firmware processed each byte rather than just looping it back blind.
static uint8_t swap_case(uint8_t c) {
    if (c >= 'a' && c <= 'z') return static_cast<uint8_t>(c - 'a' + 'A');
    if (c >= 'A' && c <= 'Z') return static_cast<uint8_t>(c - 'A' + 'a');
    return c;
}

int main() {
    GPIO::output(LED);
    USART0.begin(9600);

    USART0.writeLine_P(PSTR("Echo Terminal ready."));
    USART0.writeLine_P(PSTR("Press any key to start echoing..."));
    USART0.flush();               // make sure the banner is fully sent

    wait_for_keypress();          // blocking read() — absorb the first keypress
    USART0.writeLine_P(PSTR("Echoing now. Letters are case-swapped."));

    while (true) {
        // Poll-then-consume pattern: available() checks RXC0 without
        // touching UDR0, so it is safe to call on every loop pass even
        // when the terminal has sent nothing.
        if (USART0.available()) {
            // Must check rxError() BEFORE reading UDR0 - the flags
            // describe the byte about to be read, and reading UDR0
            // clears them.
            bool error = USART0.rxError();

            bool ready;
            uint8_t c = USART0.readNonBlock(ready);

            if (ready) {
                if (error) {
                    // Framing/overrun/parity error on this byte — flag it
                    // but still echo what we got.
                    USART0.write('[');
                    USART0.write('!');
                    USART0.write(']');
                }

                // txReady() lets us skip the write if the transmitter is
                // still busy with a previous byte, rather than blocking.
                // At 9600 baud and one byte per keypress this basically
                // never happens, but it is the non-blocking pattern you
                // would rely on at a slower baud rate or heavier load.
                if (USART0.txReady()) {
                    USART0.write(swap_case(c));
                }

                GPIO::set(LED);
                _delay_ms(15);
                GPIO::clear(LED);
            }
        }
    }
}
