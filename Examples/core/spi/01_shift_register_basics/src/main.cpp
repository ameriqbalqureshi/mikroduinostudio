/*
 * SPI Basics — 74HC595 Shift Register — MikroDuino SDK
 *
 * The simplest possible SPI program: shift one byte out to a 74HC595 and
 * latch it onto 8 LEDs. This is project 1 of 6 in the examples/spi series,
 * which walks the SPIDriver API from a single shift-register byte up to a
 * capstone that drives two different SPI devices (a shift register and a
 * MAX7219 LED matrix) on one shared bus.
 *
 * Hardware (ATmega328P @ 16 MHz, e.g. Arduino Nano/Uno):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ Signal  │ Pin   │ Wiring                                    │
 *   ├─────────┼───────┼──────────────────────────────────────────┤
 *   │ MOSI    │ PB3   │ 74HC595 pin 14 (SER) — serial data in     │
 *   │ SCK     │ PB5   │ 74HC595 pin 11 (SRCLK) — shift clock      │
 *   │ CS      │ PB2   │ 74HC595 pin 12 (RCLK) — latch clock       │
 *   │ —       │ —     │ 74HC595 pin 13 (/OE) -> GND (outputs on)  │
 *   │ —       │ —     │ 74HC595 pin 10 (/SRCLR) -> VCC (no clear) │
 *   │ —       │ —     │ 74HC595 pins QA-QH (15,1-7) -> 8 LEDs +   │
 *   │         │       │ 220 Ω resistors -> GND                    │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * The ATmega328P's hardware SPI pins are fixed: MOSI=PB3, MISO=PB4,
 * SCK=PB5, SS=PB2 — unlike GPIO, SPI is not routable to other pins.
 * The 74HC595 has no data output of its own, so MISO (PB4) is left
 * unconnected in every project in this series that uses it.
 *
 * A key SDK design point, true throughout this whole series:
 *   SPIDriver never touches chip-select for you. Unlike I2C (where the
 *   device address is baked into every transaction) or UART (which has
 *   no addressing at all), SPI identifies "who's listening" purely
 *   through which device's CS line is driven low — and that's a decision
 *   only the calling code can make correctly, especially once more than
 *   one device shares a bus (project 6). Every project in this series
 *   therefore drives CS with plain GPIO calls around each SPI.transfer().
 *
 * SPI concepts introduced:
 *   - SPI.beginMaster() — with no arguments, configures the hardware TWI
 *     pins (MOSI/SCK/SS as outputs, MISO as input) and enables the SPI
 *     peripheral using the defaults: SPIMode::Mode0, SPIBitOrder::MSBFirst,
 *     SPIClockDiv::DIV16 (F_CPU/16 = 1 MHz on a 16 MHz board). These
 *     defaults suit the overwhelming majority of SPI peripherals,
 *     including both devices used later in this series.
 *   - SPI.transfer(uint8_t data) — the fundamental SPI operation: shifts
 *     `data` out on MOSI while simultaneously shifting a byte in from
 *     MISO (full-duplex, even though this program ignores the received
 *     byte). Blocks until the hardware finishes the 8-clock exchange,
 *     then returns what came back on MISO.
 *   - GPIO::clear(CS) / GPIO::set(CS) — the manual CS discipline: pull CS
 *     low, transfer the byte(s) that device cares about, then pull CS
 *     high again. For the 74HC595 specifically, the CS pin doubles as
 *     RCLK (the latch clock) — its rising edge is what copies the
 *     internal shift register out to the QA-QH output pins, so nothing
 *     appears on the LEDs until CS goes high again.
 */

#include <avr/io.h>
#include <util/delay.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/spi.hpp>

using namespace MikroDuino;

static constexpr uint8_t CS = PB2;   // 74HC595 RCLK (latch)

static MD_INLINE void cs_low()  { GPIO::clear(CS); }
static MD_INLINE void cs_high() { GPIO::set(CS);   }

// Send one byte to the 74HC595 and pulse the latch so it appears on
// QA-QH. Every later project's device-specific "send a frame" helper
// follows this same cs_low() -> transfer(s) -> cs_high() shape.
static void shiftOut(uint8_t pattern) {
    cs_low();
    SPI.transfer(pattern);
    cs_high();   // rising edge on RCLK copies shift register -> output latch
}

int main() {
    // beginMaster() also configures PB2 (SS) as an output as a side
    // effect (required by the AVR's SPI hardware even though this
    // project drives it manually as CS) — but it doesn't know we intend
    // to use it as a latch, so explicitly set it high (idle) ourselves.
    SPI.beginMaster();
    cs_high();

    while (true) {
        // Walking-one pattern: one LED lit at a time, QA through QH.
        for (uint8_t i = 0; i < 8; ++i) {
            shiftOut(static_cast<uint8_t>(1 << i));
            _delay_ms(120);
        }

        // Walking-zero pattern: one LED off, the rest on — the bitwise
        // complement of the pattern above, still just a single byte per
        // SPI.transfer() call.
        for (uint8_t i = 0; i < 8; ++i) {
            shiftOut(static_cast<uint8_t>(~(1 << i)));
            _delay_ms(120);
        }

        // A couple of fixed patterns so the very first run is visually
        // obvious even before the animations above make sense.
        shiftOut(0xFF);   // all LEDs on
        _delay_ms(300);
        shiftOut(0x00);   // all LEDs off
        _delay_ms(300);
    }
}
