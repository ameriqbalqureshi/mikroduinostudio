/*
 * SSD1306 Basics — begin(), clear(), print(), display() — MikroDuino
 * Module SDK
 *
 * The simplest possible use of the SSD1306 module: initialise a 128x64
 * I2C OLED, print a greeting, and show a live-updating uptime counter
 * below it. This is project 1 of 6 in the examples/Modules/SSD1306
 * series, which walks the SSD1306 class from a single "Hello World" up
 * to a capstone on-screen settings menu driven by a push button.
 *
 * Hardware (ATmega328P @ 16 MHz, SSD1306 128x64 OLED, I2C):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ SDA     │ PC4   │ I2C data — shared bus, address 0x3C        │
 *   │         │       │ (0x3D if the module's SA0 pad is set high) │
 *   │ SCL     │ PC5   │ I2C clock                                  │
 *   │ VCC     │ —     │ 3.3-5V (check the specific breakout)        │
 *   │ GND     │ —     │ Ground                                     │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * SSD1306 concepts introduced:
 *   - SSD1306(i2c, addr=0x3C) — the constructor takes an I2CDriver& (the
 *     SDK's global I2C singleton is passed explicitly, unlike DS3231's
 *     defaulted parameter, so a project with more than one I2C bus can
 *     still be explicit about which one the OLED is on) and records the
 *     7-bit address. Nothing is sent over the wire yet.
 *   - begin() — runs the SSD1306's documented power-on init sequence
 *     (charge pump, multiplex ratio, etc.), then clears and pushes an
 *     initial blank frame. I2C.beginMaster() must already have been
 *     called before this, exactly like the DS3231/other I2C module
 *     examples require.
 *   - The framebuffer model: clear(), setCursor(), and print() only
 *     touch a 1024-byte RAM buffer inside the SSD1306 object — nothing
 *     changes on the physical screen until display() pushes that whole
 *     buffer over I2C in one burst. This project calls display() once
 *     per second, not once per print() call, since drawing several
 *     lines of text between two display() calls costs zero extra I2C
 *     traffic beyond the one final push.
 *   - setCursor(x, y) — x is a pixel column (0-127), y is a pixel row
 *     that should land on an 8-pixel boundary (0, 8, 16, ...) since
 *     text is drawn one 8-pixel-tall "page" at a time.
 *   - print(const char*) — draws a 5x7 font string starting at the
 *     cursor and advances it; print(int32_t) (used below for the
 *     uptime counter) formats a signed integer to decimal internally
 *     first.
 */

#include <util/delay.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/i2c.hpp>
#include <SSD1306.hpp>

using namespace MikroDuino;

SSD1306 oled(I2C, 0x3C);

int main() {
    // TWI pins also work with the internal pull-ups as a fallback if a
    // board's external pull-up resistors are missing or too weak.
    GPIO::inputPullup(PC4);
    GPIO::inputPullup(PC5);

    I2C.beginMaster(400000UL);   // 400 kHz — fast enough to push a full
                                  // 1024-byte frame in a few milliseconds
    oled.begin();

    oled.clear();
    oled.setCursor(14, 0);
    oled.print("Hello, Mikro-");
    oled.setCursor(28, 8);
    oled.print("Duino!");
    oled.display();
    _delay_ms(2000);

    oled.clear();
    oled.setCursor(0, 0);
    oled.print("Uptime (s):");
    oled.display();

    int32_t seconds = 0;
    while (true) {
        oled.setCursor(0, 16);
        oled.print(seconds);
        oled.print("   ");   // pad over any leftover digits from a longer previous value
        oled.display();

        _delay_ms(1000);
        ++seconds;
    }
}
