/*
 * Dual-Device SPI Dashboard — 74HC595 + MAX7219 on One Shared Bus —
 * MikroDuino SDK (capstone)
 *
 * Project 6 of 6 in the examples/spi series. Two SPI devices share one
 * physical bus (MOSI + SCK) with independent chip-select lines: the
 * 74HC595 bar graph from projects 1-3 and the MAX7219 matrix from
 * projects 4-5. A button cycles through three "mood" levels; each mood
 * lights a proportional bar on the shift register AND draws a matching
 * face on the matrix — one button press, two devices updated.
 *
 * This is the payoff of I2CDriver's sibling design principle applied to
 * SPI: SPIDriver never manages CS, so nothing stops two (or more)
 * completely different SPI peripherals from sharing MOSI/SCK/MISO as
 * long as each gets its own CS line and the caller always asserts
 * exactly one CS at a time.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌─────────────┬───────┬──────────────────────────────────────────┐
 *   │ Signal      │ Pin   │ Wiring                                    │
 *   ├─────────────┼───────┼──────────────────────────────────────────┤
 *   │ MOSI        │ PB3   │ Shared: 74HC595 SER + MAX7219 DIN         │
 *   │ SCK         │ PB5   │ Shared: 74HC595 SRCLK + MAX7219 CLK       │
 *   │ SR_CS       │ PB2   │ 74HC595 RCLK (latch) — bar graph only     │
 *   │ MAX_CS      │ PB1   │ MAX7219 LOAD — matrix only                │
 *   │ MOOD button │ PD2   │ Other leg to GND, internal pull-up        │
 *   │ TXD         │ PD1   │ USB-serial adapter                        │
 *   └─────────────┴───────┴──────────────────────────────────────────┘
 *
 * 74HC595: /OE -> GND, /SRCLR -> VCC, QA-QH -> 8 LEDs (bar graph).
 * MAX7219: standard 8x8 matrix module wiring (see project 4).
 *
 * The rule this project exists to demonstrate: reselect AND reconfigure
 * before every device you talk to, never assume the bus is already in
 * the state a given device needs. Concretely, before every 74HC595
 * transfer this file calls SPI.beginMaster() with the shift register's
 * settings and drives SR_CS; before every MAX7219 transfer it calls
 * SPI.beginMaster() again with the matrix's settings and drives MAX_CS.
 * Both devices happen to want Mode0/MSBFirst here, but their clock
 * speeds are deliberately set differently below purely to make the
 * point visible in code — in a real multi-device design (an SD card
 * needing a slow init speed alongside a fast sensor, for example) the
 * settings genuinely would differ, and skipping the reconfiguration
 * step is a common source of "device A stopped working after I added
 * device B" bugs.
 *
 * SPI concepts reused from the whole series:
 *   - SPI.beginMaster(div, mode, order) — reconfigured per device, not
 *     once at startup.
 *   - SPI.transfer(uint8_t) — the 74HC595 bar graph and MAX7219 register
 *     frames are both built from this single primitive.
 *   - Manual CS via GPIO — two independent CS pins, never asserted
 *     together.
 *
 * SPI concept introduced (API-completeness aside, not central to the
 * dashboard itself):
 *   - SPI.beginSlave(mode, order) — reconfigures the same pins for slave
 *     operation (MISO becomes the output, MOSI/SCK/SS become inputs).
 *     Demonstrated once at startup and immediately reverted to master
 *     mode, the same "show the call, then revert" treatment
 *     examples/spi_demo gives it — a full loopback slave demo needs a
 *     second SPI master driving the bus, outside the scope of this
 *     single-board dashboard.
 */

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/usart.hpp>
#include <mikroduino/spi.hpp>

using namespace MikroDuino;

static constexpr uint8_t SR_CS   = PB2;   // 74HC595 RCLK
static constexpr uint8_t MAX_CS  = PB1;   // MAX7219 LOAD
static constexpr uint8_t BUTTON  = PD2;

static void delay_ms(uint16_t ms) { while (ms--) _delay_ms(1); }

// ── 74HC595 bar graph ────────────────────────────────────────────────────

static MD_INLINE void srSelect(bool active) { GPIO::write(SR_CS, !active); }

// Reconfigure SPI for the shift register, then send one byte under its
// own CS. DIV16 (1 MHz) — comfortably within the 74HC595's Fmax.
static void srWrite(uint8_t pattern) {
    SPI.beginMaster(SPIClockDiv::DIV16, SPIMode::Mode0, SPIBitOrder::MSBFirst);
    srSelect(true);
    SPI.transfer(pattern);
    srSelect(false);   // rising edge latches the pattern onto QA-QH
}

// level 0-8 -> that many LEDs lit from QA upward. level=0 -> 0x00 (dark),
// level=8 -> 0xFF (all 8 LEDs).
static uint8_t barPattern(uint8_t level) {
    if (level == 0) return 0x00;
    if (level >= 8) return 0xFF;
    return static_cast<uint8_t>((1u << level) - 1u);
}

// ── MAX7219 matrix ───────────────────────────────────────────────────────

namespace MAX7219Reg {
    constexpr uint8_t Digit0      = 0x01;
    constexpr uint8_t DecodeMode  = 0x09;
    constexpr uint8_t Intensity   = 0x0A;
    constexpr uint8_t ScanLimit   = 0x0B;
    constexpr uint8_t Shutdown    = 0x0C;
    constexpr uint8_t DisplayTest = 0x0D;
}

static MD_INLINE void maxSelect(bool active) { GPIO::write(MAX_CS, !active); }

// Reconfigure SPI for the matrix, then send a 2-byte register frame
// under its own CS. DIV8 (2 MHz) — deliberately different from the
// shift register's DIV16, to make the "reconfigure per device" point
// concrete rather than just asserted in a comment.
static void maxWrite(uint8_t reg, uint8_t data) {
    SPI.beginMaster(SPIClockDiv::DIV8, SPIMode::Mode0, SPIBitOrder::MSBFirst);
    maxSelect(true);
    SPI.transfer(reg);
    SPI.transfer(data);
    maxSelect(false);
}

static void maxInit() {
    maxWrite(MAX7219Reg::Shutdown, 0x00);
    maxWrite(MAX7219Reg::DisplayTest, 0x00);
    maxWrite(MAX7219Reg::DecodeMode, 0x00);
    maxWrite(MAX7219Reg::ScanLimit, 0x07);
    maxWrite(MAX7219Reg::Intensity, 0x08);
    for (uint8_t row = 0; row < 8; ++row) {
        maxWrite(static_cast<uint8_t>(MAX7219Reg::Digit0 + row), 0x00);
    }
    maxWrite(MAX7219Reg::Shutdown, 0x01);
}

static void maxDrawBitmap_P(const uint8_t* bitmapProgmem) {
    for (uint8_t row = 0; row < 8; ++row) {
        uint8_t data = pgm_read_byte(&bitmapProgmem[row]);
        maxWrite(static_cast<uint8_t>(MAX7219Reg::Digit0 + row), data);
    }
}

// ── Mood presets: bar level + matching face bitmap ──────────────────────

static const uint8_t FACE_SAD[8] PROGMEM = {
    0b00111100, 0b01000010, 0b10100101, 0b10000001,
    0b10011001, 0b10100101, 0b01000010, 0b00111100,
};
static const uint8_t FACE_NEUTRAL[8] PROGMEM = {
    0b00111100, 0b01000010, 0b10100101, 0b10000001,
    0b10111101, 0b10000001, 0b01000010, 0b00111100,
};
static const uint8_t FACE_HAPPY[8] PROGMEM = {
    0b00111100, 0b01000010, 0b10100101, 0b10000001,
    0b10100101, 0b10011001, 0b01000010, 0b00111100,
};

struct Mood {
    uint8_t barLevel;
    const uint8_t* face;   // PROGMEM bitmap
    const char* name;       // PROGMEM string (see printMood())
};

static const char NAME_LOW[]  PROGMEM = "LOW";
static const char NAME_MID[]  PROGMEM = "MID";
static const char NAME_HIGH[] PROGMEM = "HIGH";

static const Mood MOODS[3] = {
    { 2, FACE_SAD,     NAME_LOW  },
    { 5, FACE_NEUTRAL, NAME_MID  },
    { 8, FACE_HAPPY,   NAME_HIGH },
};

static void applyMood(uint8_t idx) {
    srWrite(barPattern(MOODS[idx].barLevel));
    maxDrawBitmap_P(MOODS[idx].face);

    USART0.write_P(PSTR("Mood -> "));
    USART0.write_P(MOODS[idx].name);
    USART0.write_P(PSTR("  (bar level "));
    USART0.writeInt(MOODS[idx].barLevel);
    USART0.writeLine_P(PSTR("/8)"));
}

int main() {
    GPIO::inputPullup(BUTTON);

    // Both devices' CS lines must be driven (idle-high) BEFORE the first
    // beginMaster() call configures PB2/PB1 — beginMaster() only touches
    // the hardware SS pin (PB2) and MOSI/SCK/MISO; PB1 is an ordinary
    // GPIO output that this project owns entirely itself.
    GPIO::output(SR_CS);
    GPIO::output(MAX_CS);
    srSelect(false);
    maxSelect(false);

    USART0.begin(9600);
    USART0.writeLine_P(PSTR("Dual-device SPI dashboard: 74HC595 + MAX7219"));
    USART0.writeLine_P(PSTR("=============================================="));
    USART0.writeLine_P(PSTR("Press the button to cycle LOW / MID / HIGH mood."));

    // -----------------------------------------------------------------------
    // API-completeness aside: SPI.beginSlave(). Configures the same four
    // pins for slave operation, then immediately reverts to master mode
    // before anything else in this program runs. No external master is
    // driving the bus, so this section has no observable effect beyond
    // demonstrating the call exists and what it reconfigures.
    // -----------------------------------------------------------------------
    SPI.beginSlave(SPIMode::Mode0, SPIBitOrder::MSBFirst);
    SPI.end();
    USART0.writeLine_P(PSTR("(beginSlave() demonstrated and reverted — master mode below)"));

    maxInit();
    srWrite(0x00);

    uint8_t moodIdx = 1;   // start at MID
    applyMood(moodIdx);

    bool lastButtonState = true;

    while (true) {
        bool pressed = (GPIO::read(BUTTON) == false);
        if (pressed && lastButtonState) {
            delay_ms(30);   // debounce settle
            if (GPIO::read(BUTTON) == false) {
                moodIdx = static_cast<uint8_t>((moodIdx + 1) % 3);
                applyMood(moodIdx);
            }
        }
        lastButtonState = !pressed;

        delay_ms(10);
    }
}
