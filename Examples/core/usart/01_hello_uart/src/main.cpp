/*
 * Hello UART — MikroDuino SDK (USART basics)
 *
 * The simplest possible USART program: say "hello" over serial once a
 * second. This is project 1 of 7 in the examples/usart series, which
 * walks the USART API from a single greeting up to a full binary framed
 * protocol with CRC error-checking.
 *
 * Hardware (ATmega328P @ 16 MHz, e.g. Arduino Nano/Uno):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ Signal  │ Pin   │ Wiring                                    │
 *   ├─────────┼───────┼──────────────────────────────────────────┤
 *   │ TXD     │ PD1   │ To the RX pin of a USB-serial adapter     │
 *   │         │       │ (or straight into the board's own onboard │
 *   │         │       │ USB-serial chip, e.g. Arduino Nano).      │
 *   │ LED     │ PB5   │ Built-in LED — blinks once per message,   │
 *   │         │       │ so you can see the board is alive even    │
 *   │         │       │ before opening a terminal.                │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * Open a serial terminal (PuTTY, the Arduino IDE monitor, `screen`, ...)
 * on the board's COM port at 9600 baud, 8 data bits, no parity, 1 stop
 * bit ("9600 8N1" — the default MikroDuino USART configuration).
 *
 * USART concepts introduced:
 *   - USART0.begin(baudRate) — the one-argument overload. It picks 8N1
 *     (8 data bits, no parity, 1 stop bit) with both transmit and
 *     receive enabled, and computes the UBRR (baud rate) register for
 *     you from F_CPU and the baud rate you asked for.
 *   - USART0.write(const char*) — sends a plain C-string. The string
 *     literal lives in SRAM (the ATmega328P has only 2 KB of it), so
 *     this is fine for short, occasional text but wasteful for a
 *     program full of fixed messages.
 *   - USART0.writeLine(const char*) — same as write(), but appends
 *     "\r\n" (carriage return + line feed) so each call starts a new
 *     line in the terminal.
 *   - USART0.write_P(PSTR("...")) / writeLine_P(PSTR("...")) — the
 *     flash-resident equivalents. PSTR() places the string in program
 *     memory (flash) instead of SRAM, and write_P()/writeLine_P() read
 *     it back byte-by-byte with pgm_read_byte(). Every later example in
 *     this series uses _P() for constant text so SRAM stays free for
 *     actual data — get in the habit early.
 */

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/usart.hpp>

using namespace MikroDuino;

static constexpr uint8_t LED = PB5;

int main() {
    GPIO::output(LED);

    // begin(9600) enables TX+RX at 9600 8N1. Internally this computes:
    //   UBRR = F_CPU / (16 * baudRate) - 1
    // At 16 MHz / 9600 baud that works out to UBRR = 103, giving an
    // actual rate within about 0.2% of 9600 — close enough that no
    // receiver will notice the difference.
    USART0.begin(9600);

    // write_P + PSTR: the string "Hello, MikroDuino!" is stored in flash,
    // not SRAM. write_P() copies it out one byte at a time via
    // pgm_read_byte() as it transmits, so it never occupies RAM at all.
    USART0.writeLine_P(PSTR("Hello, MikroDuino!"));
    USART0.writeLine_P(PSTR("USART0 is alive at 9600 8N1."));

    uint16_t count = 0;

    while (true) {
        // write(const char*) — an ordinary SRAM string, for comparison
        // with the write_P() calls above. Fine for one-off strings;
        // avoid it for large blocks of fixed text.
        USART0.write("Tick #");
        USART0.writeInt(static_cast<int32_t>(count));
        USART0.writeLine_P(PSTR(" - board is running."));

        // Blink the LED once per message: visual proof the loop is
        // executing, useful for debugging before a terminal is open.
        GPIO::set(LED);
        _delay_ms(100);
        GPIO::clear(LED);

        _delay_ms(900);   // ~100 ms LED-on + 900 ms off = 1 message/second
        ++count;
    }
}
