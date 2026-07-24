/*
 * SPI Clock Speed & Mode Sweep — 74HC595 — MikroDuino SDK
 *
 * Project 2 of 6 in the examples/spi series. Same 74HC595 wiring as
 * project 1, but this time every SPIClockDiv, SPIMode and SPIBitOrder
 * value gets exercised explicitly, with USART narration explaining what
 * each one does to the electrical signal (even though the 74HC595 itself
 * can't show clock speed or CPOL/CPHA on its LEDs — a logic analyser on
 * SCK is the way to actually observe the difference).
 *
 * Hardware: identical to project 1 — 74HC595 with SER=PB3(MOSI),
 * SRCLK=PB5(SCK), RCLK=PB2(CS), QA-QH -> 8 LEDs. Add a USB-serial adapter
 * on PD1(TXD) at 9600 8N1 to see the commentary.
 *
 * SPI concepts introduced:
 *   - SPI.beginMaster(clockDiv, mode, order) — the full 3-argument form.
 *     Unlike USART's begin(), which is normally called once, SPI settings
 *     are commonly changed mid-program: different SPI peripherals on the
 *     same bus often need different speeds or modes, so beginMaster() is
 *     designed to be called again at any time to reconfigure the
 *     peripheral without re-initialising GPIO pin directions.
 *   - SPIClockDiv — divides F_CPU to produce the SCK frequency. All 7
 *     values are swept below. Slower isn't "more correct" — it's a
 *     trade-off between transfer speed and signal integrity over longer
 *     wires; the 74HC595 datasheet's Fmax (100 MHz reference) means all 7
 *     dividers are safe on this specific device.
 *   - SPIMode — CPOL (clock idle polarity) and CPHA (which clock edge
 *     data is sampled on). Every SPI device's datasheet specifies which
 *     mode(s) it requires; getting this wrong is one of the most common
 *     "SPI device doesn't respond" bugs. The 74HC595 latches SER on the
 *     rising SRCLK edge, so it works correctly with Mode0 and Mode3
 *     (both sample on the rising edge) but not Mode1/Mode2.
 *   - SPIBitOrder — MSBFirst (the default, and what almost every SPI
 *     device expects) vs LSBFirst, which reverses which end of the byte
 *     goes out first.
 */

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/usart.hpp>
#include <mikroduino/spi.hpp>

using namespace MikroDuino;

static constexpr uint8_t CS = PB2;

static MD_INLINE void cs_low()  { GPIO::clear(CS); }
static MD_INLINE void cs_high() { GPIO::set(CS);   }

static void shiftOut(uint8_t pattern) {
    cs_low();
    SPI.transfer(pattern);
    cs_high();
}

static void delay_ms(uint16_t ms) { while (ms--) _delay_ms(1); }

int main() {
    SPI.beginMaster();   // start with the defaults from project 1
    cs_high();

    USART0.begin(9600);
    USART0.writeLine_P(PSTR("SPI clock speed & mode sweep (74HC595)"));
    USART0.writeLine_P(PSTR("======================================="));

    while (true) {
        // -------------------------------------------------------------
        // SPIClockDiv — all 7 dividers, F_CPU = 16 MHz assumed.
        // -------------------------------------------------------------
        struct DivEntry { SPIClockDiv div; const char* label; };
        static const DivEntry DIVS[] = {
            { SPIClockDiv::DIV2,   "DIV2   (8 MHz)"   },
            { SPIClockDiv::DIV4,   "DIV4   (4 MHz)"   },
            { SPIClockDiv::DIV8,   "DIV8   (2 MHz)"   },
            { SPIClockDiv::DIV16,  "DIV16  (1 MHz)"   },
            { SPIClockDiv::DIV32,  "DIV32  (500 kHz)" },
            { SPIClockDiv::DIV64,  "DIV64  (250 kHz)" },
            { SPIClockDiv::DIV128, "DIV128 (125 kHz)" },
        };

        USART0.writeLine_P(PSTR(""));
        USART0.writeLine_P(PSTR("-- SPIClockDiv sweep (Mode0, MSBFirst) --"));
        for (uint8_t i = 0; i < 7; ++i) {
            USART0.write_P(PSTR("  "));
            USART0.write(DIVS[i].label);
            USART0.writeLine_P(PSTR(" ..."));

            // Reconfigure the peripheral for this speed, keeping mode
            // and bit order at their defaults.
            SPI.beginMaster(DIVS[i].div, SPIMode::Mode0, SPIBitOrder::MSBFirst);
            shiftOut(0xAA);   // alternating bits: 1010 1010
            delay_ms(150);
            shiftOut(0x55);   // 0101 0101
            delay_ms(150);
        }

        // -------------------------------------------------------------
        // SPIMode — all 4 CPOL/CPHA combinations.
        // -------------------------------------------------------------
        struct ModeEntry { SPIMode mode; const char* label; uint8_t pattern; };
        static const ModeEntry MODES[] = {
            { SPIMode::Mode0, "Mode0  CPOL=0 CPHA=0  (sample on rising edge)  [74HC595-compatible]", 0x0F },
            { SPIMode::Mode1, "Mode1  CPOL=0 CPHA=1  (sample on falling edge) [wrong mode for 74HC595]", 0xF0 },
            { SPIMode::Mode2, "Mode2  CPOL=1 CPHA=0  (sample on falling edge) [wrong mode for 74HC595]", 0x33 },
            { SPIMode::Mode3, "Mode3  CPOL=1 CPHA=1  (sample on rising edge)  [74HC595-compatible]",     0xCC },
        };

        USART0.writeLine_P(PSTR(""));
        USART0.writeLine_P(PSTR("-- SPIMode sweep (DIV16, MSBFirst) --"));
        for (uint8_t i = 0; i < 4; ++i) {
            USART0.write_P(PSTR("  "));
            USART0.write(MODES[i].label);
            USART0.writeLine_P(PSTR(""));

            SPI.beginMaster(SPIClockDiv::DIV16, MODES[i].mode, SPIBitOrder::MSBFirst);
            shiftOut(MODES[i].pattern);
            delay_ms(500);
        }

        // -------------------------------------------------------------
        // SPIBitOrder::LSBFirst — same walking-one pattern as project 1,
        // but shifted out least-significant-bit first. On the LEDs the
        // lit position appears to march QH -> QA instead of QA -> QH,
        // with no change to the pattern values themselves.
        // -------------------------------------------------------------
        USART0.writeLine_P(PSTR(""));
        USART0.writeLine_P(PSTR("-- SPIBitOrder::LSBFirst walking-one --"));
        SPI.beginMaster(SPIClockDiv::DIV16, SPIMode::Mode0, SPIBitOrder::LSBFirst);
        for (uint8_t i = 0; i < 8; ++i) {
            shiftOut(static_cast<uint8_t>(1 << i));
            delay_ms(120);
        }

        // Restore MSBFirst (the default every other project assumes)
        // before the next pass through the loop.
        SPI.beginMaster(SPIClockDiv::DIV16, SPIMode::Mode0, SPIBitOrder::MSBFirst);
        cs_high();

        USART0.writeLine_P(PSTR(""));
        USART0.writeLine_P(PSTR("Sweep complete. Repeating..."));
        delay_ms(1000);
    }
}
