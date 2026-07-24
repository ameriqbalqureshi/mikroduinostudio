/*
 * I2C Bus Scanner — MikroDuino SDK (I2C basics)
 *
 * The simplest possible I2C program: bring up the hardware TWI peripheral
 * as a bus master and ask every possible 7-bit address whether a device is
 * listening there. This is project 1 of 6 in the examples/i2c series, which
 * walks the I2CDriver API from a bare bus scan up to a capstone that logs
 * timestamped data to an external EEPROM using an RTC for timekeeping.
 *
 * Hardware (ATmega328P @ 16 MHz, e.g. Arduino Nano/Uno):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ Signal  │ Pin   │ Wiring                                    │
 *   ├─────────┼───────┼──────────────────────────────────────────┤
 *   │ SDA     │ PC4   │ I2C data — shared bus line, all devices   │
 *   │ SCL     │ PC5   │ I2C clock — shared bus line, all devices  │
 *   │ TXD     │ PD1   │ To the RX pin of a USB-serial adapter     │
 *   │ LED     │ PB5   │ Built-in LED — blinks once per scan pass  │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * The ATmega328P's hardware TWI pins are fixed at PC4 (SDA) / PC5 (SCL) —
 * unlike GPIO, I2C is not routable to other pins. Real I2C busses need
 * pull-up resistors (typically 4.7 kΩ) from SDA and SCL to VCC so the line
 * idles high; most breakout boards (EEPROM modules, RTC modules) already
 * include them. If your wiring has no external pull-ups, this example
 * enables the ATmega's weak internal ones as a fallback — fine for a
 * single slow device on a short wire, but external resistors are more
 * reliable for real projects with multiple devices or long wires.
 *
 * Open a serial terminal at 9600 8N1 and watch the scan results.
 *
 * I2C concepts introduced:
 *   - I2C.beginMaster(clockHz) — configures TWBR/TWSR for the requested
 *     SCL clock rate (100 kHz "standard mode" is used here — the speed
 *     every I2C device is guaranteed to support) and enables the TWI
 *     peripheral (TWEN). Must be called once before any other I2C.*() call.
 *   - I2C.scan(callback) — walks every 7-bit address from 1 to 126, sends
 *     just a START + address-byte (write direction) on each one, and
 *     invokes callback(address) for every address that comes back with an
 *     ACK. An ACK at this stage means *some* chip is listening at that
 *     address — it says nothing about what the chip is or whether it's
 *     healthy, just that it's present and responding on the wire.
 *
 * This scanner alone is a genuinely useful diagnostic: run it first on any
 * new I2C wiring before trying a specific device driver. If the address you
 * expect doesn't show up, the problem is almost always wiring (SDA/SCL
 * swapped, missing pull-ups, wrong VCC) rather than software.
 */

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/usart.hpp>
#include <mikroduino/i2c.hpp>

using namespace MikroDuino;

static constexpr uint8_t LED = PB5;

int main() {
    GPIO::output(LED);

    // Fallback pull-ups: harmless to leave enabled even if the bus also has
    // external resistors — it just adds a very high-value resistor in
    // parallel with them. Remove these two lines if you want to rely
    // strictly on external pull-ups (the more standard approach).
    GPIO::inputPullup(PC4);   // SDA
    GPIO::inputPullup(PC5);   // SCL

    USART0.begin(9600);
    USART0.writeLine_P(PSTR("MikroDuino I2C Bus Scanner"));
    USART0.writeLine_P(PSTR("==========================="));

    // 100 kHz is I2C "standard mode" — every device datasheet supports it,
    // even ones capable of the faster 400 kHz "fast mode". Start here;
    // later examples bump this up once a specific device is confirmed to
    // handle it.
    I2C.beginMaster(100000UL);

    uint16_t pass = 0;

    while (true) {
        ++pass;
        USART0.write_P(PSTR("Scan #"));
        USART0.writeInt(static_cast<int32_t>(pass));
        USART0.writeLine_P(PSTR(" ..."));

        uint8_t found = 0;

        // scan() takes any callable — here a small lambda that captures
        // `found` by reference to count hits and prints each address it
        // sees. This is the entire discovery mechanism: no device-specific
        // knowledge required at all.
        I2C.scan([&found](uint8_t addr) {
            ++found;
            USART0.write_P(PSTR("  found device at 0x"));
            char hex[3];
            hex[0] = "0123456789ABCDEF"[(addr >> 4) & 0x0F];
            hex[1] = "0123456789ABCDEF"[addr & 0x0F];
            hex[2] = '\0';
            USART0.write(hex);

            // Point out the two device families used later in this series,
            // so the scan output is immediately meaningful.
            if (addr == 0x50)      USART0.write_P(PSTR("  (typical AT24Cxx EEPROM)"));
            else if (addr == 0x68) USART0.write_P(PSTR("  (typical DS1307/DS3231 RTC)"));
            USART0.writeLine_P(PSTR(""));
        });

        if (found == 0) {
            USART0.writeLine_P(PSTR("  no devices found — check wiring/pull-ups"));
        } else {
            USART0.write_P(PSTR("  "));
            USART0.writeInt(static_cast<int32_t>(found));
            USART0.writeLine_P(PSTR(" device(s) total"));
        }
        USART0.writeLine_P(PSTR(""));

        // One LED blink per completed scan pass — visible proof of life
        // even without a terminal attached.
        GPIO::set(LED);
        _delay_ms(80);
        GPIO::clear(LED);

        _delay_ms(2000);
    }
}
