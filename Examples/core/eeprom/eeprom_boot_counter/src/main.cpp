/*
 * EEPROM Example 1 (Beginner) — Persistent Boot Counter — MikroDuino SDK
 *
 * The simplest possible use of the EEPROM driver: read a value once at
 * startup, increment it, write it back. This is the "blink" of EEPROM
 * usage — start here before moving on to the other EEPROM examples in
 * this series.
 *
 * Real-world application: a power-on/maintenance counter, the kind used to
 * track "how many times has this device been switched on" for service
 * intervals, or simply to prove data survives a reset — every time you
 * power-cycle the board, the count picks up where it left off instead of
 * resetting to zero like ordinary RAM would.
 *
 * Hardware (ATmega328P / Arduino Nano @ 16 MHz):
 *
 *   ┌──────────────┬───────┬────────────────────────────────────────────┐
 *   │ Signal       │ Pin   │ Notes                                      │
 *   ├──────────────┼───────┼────────────────────────────────────────────┤
 *   │ USART0 TX    │ PD1   │ To PC via USB-serial, 9600 8N1             │
 *   │ Status LED   │ PB5   │ Blinks once per boot, after the count is   │
 *   │              │       │ safely written                             │
 *   └──────────────┴───────┴────────────────────────────────────────────┘
 *
 * EEPROM features used in this example:
 *   EEPROM.capacity()       — compile-time-resolved EEPROM size for the MCU
 *   EEPROM.get<T>(addr)     — typed read: reads sizeof(T) bytes into a value
 *   EEPROM.put<T>(addr,val) — typed write: writes sizeof(T) bytes, unconditional
 *
 * That's it — just two calls beyond capacity(). Later examples in this
 * series build on this with update() (endurance-aware writes), structs,
 * ring buffers, and interrupt-driven background writes.
 *
 * NOTE: factory-fresh AVR EEPROM reads all bits as 1, i.e. 0xFFFF for a
 * uint16_t. We treat that specific value as "never written before" so the
 * very first boot initialises the counter to 0 instead of a huge garbage
 * number.
 */

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/usart.hpp>
#include <mikroduino/eeprom.hpp>

using namespace MikroDuino;

#define WP(s)   USART0.write_P(PSTR(s))
#define WLP(s)  USART0.writeLine_P(PSTR(s))

static constexpr uint8_t  STATUS_LED   = PB5;
static constexpr uint16_t ADDR_COUNTER = 0x000;   // 2 bytes: uint16_t boot count

int main() {
    GPIO::output(STATUS_LED);
    GPIO::clear(STATUS_LED);

    USART0.begin(9600);
    WLP("");
    WLP("MikroDuino EEPROM Example 1 — Boot Counter");
    WP("EEPROM capacity: "); USART0.writeInt(static_cast<int32_t>(EEPROM.capacity()));
    WLP(" bytes");

    // get<T>() reads sizeof(T) bytes from EEPROM and returns them as a T.
    // Here T = uint16_t, so this reads 2 bytes from ADDR_COUNTER.
    uint16_t count = EEPROM.get<uint16_t>(ADDR_COUNTER);

    if (count == 0xFFFFu) {
        // 0xFFFF is what a never-written (factory-erased) uint16_t reads as.
        WLP("First boot detected (EEPROM was blank) -- starting at 0.");
        count = 0;
    }

    ++count;

    // put<T>() writes sizeof(T) bytes unconditionally. For a counter that
    // changes on every boot this is fine; Example 2 shows update(), which
    // skips the write entirely when the value hasn't changed.
    EEPROM.put(ADDR_COUNTER, count);

    WP("This device has booted ");
    USART0.writeInt(static_cast<int32_t>(count));
    WLP(" time(s).");
    WLP("Power-cycle the board to watch this number keep climbing.");

    // Blink the status LED once to confirm the write completed, then idle.
    GPIO::set(STATUS_LED);
    while (true) {
        // Nothing left to do — the interesting part already happened above.
        // Real firmware would continue into its normal application loop here.
    }
}
