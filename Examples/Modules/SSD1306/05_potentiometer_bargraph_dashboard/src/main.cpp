/*
 * SSD1306 Live Potentiometer Bargraph Dashboard — ADCDriver + SSD1306
 * graphics — MikroDuino Module SDK
 *
 * Project 5 of 6 in the examples/Modules/SSD1306 series. Turns the
 * drawing primitives from project 2 into a genuinely live instrument: a
 * horizontal bar graph tracking a potentiometer in real time, with thin
 * tick marks marking the lowest and highest readings seen since boot —
 * a small "envelope" readout built entirely from fillRect()/drawVLine()
 * redrawn on a fixed schedule.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ SDA     │ PC4   │ I2C data, address 0x3C                     │
 *   │ SCL     │ PC5   │ I2C clock                                  │
 *   │ ADC0    │ PC0   │ Potentiometer wiper — outer legs to        │
 *   │         │       │ 5V and GND                                 │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * SSD1306 concepts reused from projects 1-2:
 *   - clear(), setCursor(), print(int32_t), display() for the numeric
 *     readout row.
 *   - fillRect() for the bar itself, drawRect() for its outline,
 *     drawVLine() for the min/max tick marks — the same primitives
 *     project 2 introduced, now driven by a live value every redraw
 *     instead of a fixed scene.
 *
 * New (non-SSD1306) library used for interactivity:
 *   - ADCDriver (mikroduino/adc.hpp): ADC_Driver.begin() and
 *     ADC_Driver.read(channel) — a blocking single conversion returning
 *     a 0-1023 value proportional to the potentiometer's wiper voltage.
 *
 * Redraw cost: a full 1024-byte display() push at 400 kHz I2C takes on
 * the order of a few milliseconds — comfortably fast enough to redraw
 * the whole frame every 100 ms (10 Hz) without visible lag, so this
 * project doesn't bother with a partial/dirty-rectangle update scheme.
 */

#include <avr/pgmspace.h>
#include <util/delay.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/i2c.hpp>
#include <mikroduino/adc.hpp>
#include <SSD1306.hpp>

using namespace MikroDuino;

static constexpr uint8_t POT_CHANNEL = 0;   // ADC0 / PC0

SSD1306 oled(I2C, 0x3C);

static constexpr uint8_t BAR_X = 2, BAR_Y = 40, BAR_W = 124, BAR_H = 16;

static uint8_t potToBarX(uint16_t potValue) {
    return static_cast<uint8_t>(BAR_X + (static_cast<uint32_t>(potValue) * (BAR_W - 1)) / 1023u);
}

static void delay_ms(uint16_t ms) { while (ms--) _delay_ms(1); }

int main() {
    GPIO::inputPullup(PC4);
    GPIO::inputPullup(PC5);

    I2C.beginMaster(400000UL);
    oled.begin();
    ADC_Driver.begin();

    uint16_t minValue = 1023;
    uint16_t maxValue = 0;

    while (true) {
        uint16_t potValue = ADC_Driver.read(POT_CHANNEL);
        if (potValue < minValue) minValue = potValue;
        if (potValue > maxValue) maxValue = potValue;

        oled.clear();

        // ---- Numeric readout ----
        oled.setCursor(0, 0);
        oled.print("ADC: ");
        oled.print(static_cast<int32_t>(potValue));

        oled.setCursor(0, 16);
        oled.print("min:");
        oled.print(static_cast<int32_t>(minValue));
        oled.print(" max:");
        oled.print(static_cast<int32_t>(maxValue));

        // ---- Bar graph ----
        oled.drawRect(BAR_X - 1, BAR_Y - 1, BAR_W + 2, BAR_H + 2);
        uint8_t fillWidth = static_cast<uint8_t>(
            (static_cast<uint32_t>(potValue) * BAR_W) / 1023u
        );
        if (fillWidth > 0) oled.fillRect(BAR_X, BAR_Y, fillWidth, BAR_H, true);

        // ---- Min/max envelope ticks, drawn OFF so they notch the filled bar ----
        oled.drawVLine(potToBarX(minValue), BAR_Y, BAR_H, false);
        oled.drawVLine(potToBarX(maxValue), BAR_Y, BAR_H, false);

        oled.display();

        // A human turning a knob doesn't need sub-100ms feedback, and a
        // 10 Hz redraw rate keeps the I2C bus and framebuffer math well
        // under any real time pressure.
        delay_ms(100);
    }
}
