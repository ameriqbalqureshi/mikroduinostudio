/*
 * IRArray Analog Calibration + Raw Readout — ADCDriver, calibrateBlack(),
 * calibrateWhite(), rawADC() — MikroDuino Module SDK
 *
 * Project 3 of 6 in the examples/Modules/IRArray series. Switches from
 * projects 1-2's DIGITAL comparator sensors to six raw ANALOG IR
 * reflectance sensors read through the ADC, which need a one-time
 * calibration sweep before their readings mean anything comparable
 * across sensors. A push button gates the two calibration phases so the
 * robot can be physically moved into position before each one starts.
 *
 * Hardware (ATmega328P @ 16 MHz, six analog IR reflectance sensors —
 * e.g. bare TCRT5000 phototransistor outputs with a pull-up resistor,
 * no onboard comparator):
 *
 *   ┌──────────────┬───────┬──────────────────────────────────────────┐
 *   │ Sensor0-5    │ PC0-5 │ ADC0-ADC5, one analog voltage per sensor   │
 *   │ Calibrate btn│ PD2   │ Other leg to GND, internal pull-up          │
 *   │ TXD          │ PD1   │ USB-serial adapter, 9600 8N1                │
 *   └──────────────┴───────┴──────────────────────────────────────────┘
 *
 * IRArray concepts introduced:
 *   - ANALOG mode — IRArray<N>::ANALOG passed as the constructor's mode
 *     argument, together with a pointer to an already-existing
 *     ADCDriver. Unlike DIGITAL mode's fixed 0-or-1000 reading, ANALOG
 *     mode reports a smoothly graded 0-1000 value per sensor and is far
 *     more robust to ambient light and sensor-to-sensor variation — but
 *     ONLY once it has been told what "definitely background" and
 *     "definitely line" actually look like on THIS robot, on THIS
 *     surface, under THESE lighting conditions.
 *   - calibrateBlack() — called repeatedly (once per sample) while the
 *     sensors are physically positioned over the dark line surface.
 *     Internally it just tracks the highest ADC reading seen so far per
 *     sensor, because darker surfaces reflect less IR back into a
 *     TCRT5000-style phototransistor, which reads as a HIGHER voltage.
 *   - calibrateWhite() — the mirror image, called while sweeping over
 *     the light background, tracking the LOWEST ADC reading seen per
 *     sensor.
 *   - Both calibration calls are safe to call many times over a couple
 *     of seconds while physically moving the sensors back and forth —
 *     that's the intended usage, not a single instantaneous sample,
 *     since a single sample might land on a speck of dust or a seam in
 *     the tape rather than a representative reading.
 *   - rawADC(i) — the raw, uncalibrated 0-1023 ADC value for sensor i,
 *     exposed here purely as a diagnostic: it's what calibrateBlack()/
 *     calibrateWhite() are secretly tracking the extremes of, useful
 *     for confirming your wiring and lighting are sane BEFORE trusting
 *     the calibrated 0-1000 value read() derives from them.
 *   - read(out[N]) — same signature as project 1, but now every value
 *     is computed from (rawADC - calibratedWhiteMin) scaled by
 *     (calibratedBlackMax - calibratedWhiteMin), clamped to 0-1000. If
 *     either calibration step was skipped, that sensor's range stays
 *     degenerate and read() simply reports 0 for it — never a bogus
 *     extrapolated value.
 *
 * Button concepts reused (see examples/Modules/Button/01_basic_click_led
 * for the full introduction): Button(pin), begin(), update() called
 * every ~1 ms from a busy _delay_ms(1) loop, clicked() as a one-shot
 * gate — used here to advance from "waiting" to "calibrating black" to
 * "calibrating white" to "streaming", one click at a time.
 */

#include <util/delay.h>
#include <mikroduino/usart.hpp>
#include <mikroduino/adc.hpp>
#include <IRArray.hpp>
#include <Button.hpp>

using namespace MikroDuino;

static constexpr uint8_t SENSOR_COUNT = 6;
static const uint8_t SENSOR_PINS[SENSOR_COUNT] = { PC0, PC1, PC2, PC3, PC4, PC5 };

ADCDriver adc;

// ANALOG mode; activeLow is ignored in this mode (it only affects DIGITAL
// mode's HIGH/LOW interpretation) but the constructor still requires an
// argument in that slot, so it's passed as its default, true.
IRArray<SENSOR_COUNT> irArray(SENSOR_PINS, IRArray<SENSOR_COUNT>::ANALOG, true, &adc);

Button calBtn(PD2);

// Blocks until the button is clicked, calling update() every ~1 ms as
// Button's timing model requires (see Button project 1).
static void waitForClick() {
    while (!calBtn.clicked()) {
        calBtn.update();
        _delay_ms(1);
    }
}

int main() {
    USART0.begin(9600);
    USART0.writeLine_P(PSTR("IRArray analog calibration + raw readout"));
    USART0.writeLine_P(PSTR("========================================="));
    USART0.writeLine_P(PSTR(""));

    adc.begin();
    irArray.begin();   // no-op in ANALOG mode, but always call it for parity with DIGITAL mode
    calBtn.begin();

    USART0.writeLine_P(PSTR("Step 1: position sensors over the BLACK line, then click the button."));
    waitForClick();
    USART0.writeLine_P(PSTR("Calibrating BLACK - sweep the sensors back and forth over the line now..."));
    for (uint16_t i = 0; i < 100; ++i) {   // ~2 s of sampling at 20 ms/sample
        irArray.calibrateBlack();
        _delay_ms(20);
    }
    USART0.writeLine_P(PSTR("Black calibration done."));
    USART0.writeLine_P(PSTR(""));

    USART0.writeLine_P(PSTR("Step 2: position sensors over the WHITE background, then click again."));
    waitForClick();
    USART0.writeLine_P(PSTR("Calibrating WHITE - sweep the sensors back and forth over the background now..."));
    for (uint16_t i = 0; i < 100; ++i) {
        irArray.calibrateWhite();
        _delay_ms(20);
    }
    USART0.writeLine_P(PSTR("White calibration done."));
    USART0.writeLine_P(PSTR(""));
    USART0.writeLine_P(PSTR("Streaming raw ADC + normalised readings every 200 ms:"));
    USART0.writeLine_P(PSTR(""));

    while (true) {
        uint16_t normalized[SENSOR_COUNT];
        irArray.read(normalized);

        for (uint8_t i = 0; i < SENSOR_COUNT; ++i) {
            USART0.write_P(PSTR("S"));
            USART0.writeInt(static_cast<int32_t>(i));
            USART0.write_P(PSTR("(raw="));
            USART0.writeInt(static_cast<int32_t>(irArray.rawADC(i)));
            USART0.write_P(PSTR(",norm="));
            USART0.writeInt(static_cast<int32_t>(normalized[i]));
            USART0.write_P(PSTR(") "));
        }

        int16_t pos = irArray.linePosition();
        USART0.write_P(PSTR(" pos="));
        if (pos < 0) USART0.write_P(PSTR("LOST"));
        else         USART0.writeInt(pos);
        USART0.writeLine_P(PSTR(""));

        _delay_ms(200);
    }
}
