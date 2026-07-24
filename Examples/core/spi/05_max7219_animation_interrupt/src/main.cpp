/*
 * MAX7219 Interrupt-Driven Animation — MikroDuino SDK
 *
 * Project 5 of 6 in the examples/spi series. Builds on project 4's raw
 * MAX7219 register protocol with two additions:
 *
 *   1. A bouncing-pixel animation whose register frames are sent using
 *      interrupt-driven SPI transfers (SPI.enableInterrupt() + an
 *      ISR(SPI_STC_vect) state machine) instead of the blocking
 *      transfer() calls every earlier project used.
 *   2. SPI.end() + SPI.beginMaster() to stop and restart the peripheral
 *      at a different clock speed partway through the program — useful
 *      whenever a project has phases with genuinely different SPI needs.
 *
 * Hardware: identical to project 4 — MAX7219 on DIN=PB3(MOSI),
 * CLK=PB5(SCK), LOAD=PB1(CS). Add a push button on PD2 (other leg to
 * GND, internal pull-up) to cycle brightness, and USART0 at 9600 8N1 on
 * PD1 for commentary.
 *
 * Why interrupt-driven for a 2-byte frame? At any realistic SPI clock
 * this finishes in microseconds, so there's little real benefit here —
 * the point of this project is to show the MECHANISM clearly on a small,
 * well-understood transfer, the same way project 3's daisy-chain
 * projects showed transferComplete() on a 74HC595 before either
 * technique would matter on a slower, busier system. Once the mechanism
 * is familiar, applying it to something that actually benefits (e.g.
 * streaming many frames to a large display while the CPU keeps doing
 * other work) is a matter of scale, not a different technique.
 *
 * SPI concepts introduced:
 *   - SPI.enableInterrupt() / SPI.disableInterrupt() — sets/clears SPIE
 *     in SPCR. With SPIE set, the hardware raises SPI_STC_vect every time
 *     SPIF becomes set (i.e. every time a byte finishes shifting),
 *     instead of the caller having to poll transferComplete() manually.
 *   - ISR(SPI_STC_vect) — MikroDuino's SPIDriver doesn't wrap this in a
 *     callback registration mechanism ("no hidden state machine" is the
 *     same design philosophy as I2CDriver): the caller writes a plain
 *     avr-libc ISR and manages whatever sequencing it needs. Here that
 *     means a 2-step state machine — send the register address byte,
 *     then on its completion interrupt send the data byte, then on ITS
 *     completion interrupt raise CS to latch the frame.
 *   - SPI.end() — disables the SPI peripheral (clears SPE in SPCR).
 *     Demonstrated here between the two animation phases purely to show
 *     the call; SPI.beginMaster() immediately afterward reconfigures and
 *     re-enables it, this time at a different clock divider.
 *
 * SPI concepts reused from projects 1-4:
 *   - SPI.beginMaster(), manual CS via GPIO, the 2-byte register-frame
 *     shape from project 4's max7219Write().
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/usart.hpp>
#include <mikroduino/spi.hpp>

using namespace MikroDuino;

static constexpr uint8_t CS     = PB1;   // MAX7219 LOAD
static constexpr uint8_t BUTTON = PD2;   // brightness cycle button

static MD_INLINE void cs_low()  { GPIO::clear(CS); }
static MD_INLINE void cs_high() { GPIO::set(CS);   }

namespace MAX7219Reg {
    constexpr uint8_t Digit0      = 0x01;
    constexpr uint8_t DecodeMode  = 0x09;
    constexpr uint8_t Intensity   = 0x0A;
    constexpr uint8_t ScanLimit   = 0x0B;
    constexpr uint8_t Shutdown    = 0x0C;
    constexpr uint8_t DisplayTest = 0x0D;
}

static void delay_ms(uint16_t ms) { while (ms--) _delay_ms(1); }

// ── Blocking register write — used only during setup, before the ISR
//    state machine below takes over for the animation loop. ──────────────
static void max7219Write(uint8_t reg, uint8_t data) {
    cs_low();
    SPI.transfer(reg);
    SPI.transfer(data);
    cs_high();
}

static void max7219Init() {
    max7219Write(MAX7219Reg::Shutdown, 0x00);
    max7219Write(MAX7219Reg::DisplayTest, 0x00);
    max7219Write(MAX7219Reg::DecodeMode, 0x00);
    max7219Write(MAX7219Reg::ScanLimit, 0x07);
    max7219Write(MAX7219Reg::Intensity, 0x08);
    for (uint8_t row = 0; row < 8; ++row) {
        max7219Write(static_cast<uint8_t>(MAX7219Reg::Digit0 + row), 0x00);
    }
    max7219Write(MAX7219Reg::Shutdown, 0x01);
}

// ── Interrupt-driven register write ─────────────────────────────────────
//
// A 2-byte frame split across two SPI_STC_vect interrupts:
//   phase Address -> SPDR loaded with the register byte in max7219WriteAsync()
//   [transfer runs in hardware; CPU is free]
//   ISR fires -> phase becomes Data, SPDR loaded with the data byte
//   [transfer runs in hardware; CPU is free]
//   ISR fires -> CS raised (latches the frame), phase becomes Idle,
//                g_frameDone set so the caller knows it's safe to send
//                another frame.
enum class FramePhase : uint8_t { Idle, Address, Data };

static volatile FramePhase g_phase     = FramePhase::Idle;
static volatile uint8_t    g_pendingData = 0;
static volatile bool       g_frameDone   = true;

ISR(SPI_STC_vect) {
    (void)SPDR;   // reading SPDR clears SPIF; the received byte is unused (MAX7219 has no MISO)

    if (g_phase == FramePhase::Address) {
        g_phase = FramePhase::Data;
        SPDR = g_pendingData;          // kick off the second byte of the frame
    } else if (g_phase == FramePhase::Data) {
        cs_high();                      // rising edge on LOAD latches the frame
        g_phase = FramePhase::Idle;
        g_frameDone = true;
    }
}

// Start an asynchronous register write. Blocks only if a previous async
// frame is still in flight (it never will be for long — two bytes at
// 1 MHz take about 16 microseconds).
static void max7219WriteAsync(uint8_t reg, uint8_t data) {
    while (!g_frameDone) {}   // wait out any frame still in progress

    g_pendingData = data;
    g_frameDone   = false;
    g_phase       = FramePhase::Address;

    cs_low();
    SPDR = reg;   // starts the transfer; the rest happens in ISR(SPI_STC_vect)
}

// ── Bouncing pixel animation ─────────────────────────────────────────────

struct Ball { int8_t x, y, dx, dy; };

static void drawBall(const Ball& b) {
    for (uint8_t row = 0; row < 8; ++row) {
        uint8_t data = (row == static_cast<uint8_t>(b.y))
            ? static_cast<uint8_t>(0x80 >> b.x)
            : 0x00;
        max7219WriteAsync(static_cast<uint8_t>(MAX7219Reg::Digit0 + row), data);
    }
}

static void stepBall(Ball& b) {
    b.x = static_cast<int8_t>(b.x + b.dx);
    b.y = static_cast<int8_t>(b.y + b.dy);
    if (b.x <= 0 || b.x >= 7) b.dx = static_cast<int8_t>(-b.dx);
    if (b.y <= 0 || b.y >= 7) b.dy = static_cast<int8_t>(-b.dy);
    if (b.x < 0) b.x = 0;
    if (b.x > 7) b.x = 7;
    if (b.y < 0) b.y = 0;
    if (b.y > 7) b.y = 7;
}

// ── Scrolling vertical-bar animation (used after the SPI.end()/restart) ──

static void drawBar(uint8_t column) {
    for (uint8_t row = 0; row < 8; ++row) {
        uint8_t data = static_cast<uint8_t>(0x80 >> column);
        max7219WriteAsync(static_cast<uint8_t>(MAX7219Reg::Digit0 + row), data);
    }
}

int main() {
    GPIO::inputPullup(BUTTON);

    SPI.beginMaster();
    cs_high();

    USART0.begin(9600);
    USART0.writeLine_P(PSTR("MAX7219 interrupt-driven animation"));
    USART0.writeLine_P(PSTR("===================================="));

    max7219Init();

    // Arm the interrupt path. From this point on, max7219WriteAsync()
    // relies on SPI_STC_vect firing to advance each frame.
    sei();
    SPI.enableInterrupt();

    static const uint8_t BRIGHTNESS_LEVELS[3] = { 0x02, 0x08, 0x0F };
    uint8_t brightnessIdx = 1;
    bool lastButtonState = true;

    USART0.writeLine_P(PSTR("Phase 1: bouncing pixel (interrupt-driven frames, DIV16)"));
    Ball ball = { 0, 0, 1, 1 };
    for (uint16_t frame = 0; frame < 200; ++frame) {
        // Poll the brightness button between animation frames — the kind
        // of "other work" interrupt-driven SPI is meant to make room for.
        bool pressed = (GPIO::read(BUTTON) == false);
        if (pressed && lastButtonState) {
            delay_ms(30);
            if (GPIO::read(BUTTON) == false) {
                brightnessIdx = static_cast<uint8_t>((brightnessIdx + 1) % 3);
                max7219WriteAsync(MAX7219Reg::Intensity, BRIGHTNESS_LEVELS[brightnessIdx]);
                USART0.write_P(PSTR("Brightness -> "));
                USART0.writeInt(BRIGHTNESS_LEVELS[brightnessIdx]);
                USART0.writeLine_P(PSTR(""));
            }
        }
        lastButtonState = !pressed;

        drawBall(ball);
        stepBall(ball);
        delay_ms(60);
    }

    // -----------------------------------------------------------------------
    // Stop the peripheral and restart it at a different clock divider for
    // the second phase. The MAX7219 doesn't need the extra speed — this is
    // purely to demonstrate SPI.end() + SPI.beginMaster() reconfiguration.
    // -----------------------------------------------------------------------
    USART0.writeLine_P(PSTR("SPI.end() -> SPI.beginMaster(DIV4) -> Phase 2: scrolling bar"));
    SPI.disableInterrupt();
    SPI.end();
    delay_ms(200);

    SPI.beginMaster(SPIClockDiv::DIV4, SPIMode::Mode0, SPIBitOrder::MSBFirst);
    cs_high();
    SPI.enableInterrupt();

    while (true) {
        for (uint8_t col = 0; col < 8; ++col) {
            bool pressed = (GPIO::read(BUTTON) == false);
            if (pressed && lastButtonState) {
                delay_ms(30);
                if (GPIO::read(BUTTON) == false) {
                    brightnessIdx = static_cast<uint8_t>((brightnessIdx + 1) % 3);
                    max7219WriteAsync(MAX7219Reg::Intensity, BRIGHTNESS_LEVELS[brightnessIdx]);
                }
            }
            lastButtonState = !pressed;

            drawBar(col);
            delay_ms(90);
        }
    }
}
