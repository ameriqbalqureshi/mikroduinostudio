/*
 * Two Independent Interrupt Sources — enable/disable vs detach/attach —
 * MikroDuino SDK
 *
 * Project 2 of 6 in the examples/interrupts series. Wires up BOTH
 * external interrupt lines the ATmega328P has (INT0 and INT1), each
 * toggling its own LED, and uses each one to demonstrate a different way
 * of pausing an interrupt source:
 *
 *   - INT0 is paused with Interrupt.disable()/enable() — a lightweight
 *     gate. The handler stays registered; only the EIMSK enable bit
 *     toggles.
 *   - INT1 is paused with Interrupt.detach()/attach() — a full teardown.
 *     detach() disables the line AND forgets the handler; resuming needs
 *     a complete attach() call with the handler and sense mode supplied
 *     again, exactly like the very first time.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ Button0 │ PD2   │ INT0 — other leg to GND, internal pull-up │
 *   │ LED0    │ PB5   │ Toggled by Button0                        │
 *   │ Button1 │ PD3   │ INT1 — other leg to GND, internal pull-up │
 *   │ LED1    │ PB4   │ Toggled by Button1                        │
 *   │ TXD     │ PD1   │ USB-serial adapter                        │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * Each button toggles its LED and counts its own presses. After 5
 * presses, that source pauses itself for 3 seconds (presses during the
 * pause are silently ignored — proof the line really is off) before
 * resuming automatically. Watching both LEDs side by side, the paused
 * BEHAVIOUR looks identical either way — the difference between
 * disable()/enable() and detach()/attach() is entirely about what code
 * has to run to resume, not what the hardware does while paused.
 *
 * Interrupt concepts introduced:
 *   - IntSource::INT1 — the second (and, on the ATmega328P, last)
 *     external interrupt line, fixed to PD3. Fully independent of INT0:
 *     separate EIMSK bit, separate EICRA sense bits, separate handler
 *     slot, separate ISR vector (INT1_vect).
 *   - Interrupt.disable(source) / Interrupt.enable(source) — clears/sets
 *     just that source's EIMSK bit. The handler set by an earlier
 *     attach() is untouched, so enable() alone is enough to resume.
 *   - Interrupt.detach(source) — disable(source) PLUS forgetting the
 *     stored handler pointer. Calling attach() again is mandatory to
 *     resume, even with the exact same handler as before.
 *
 * A note on the press counters: g_int0Count/g_int1Count are plain
 * uint8_t volatiles, each read directly in main() with no atomic
 * protection — unlike the 32-bit millis() counters in the timer series
 * (which genuinely need ATOMIC_BLOCK_START/END to avoid a torn multi-
 * byte read), a single byte is always read or written in ONE indivisible
 * instruction on an 8-bit AVR, so there's nothing to protect here. Using
 * ATOMIC_BLOCK on a single byte would be harmless but pointless — good
 * to know which is which.
 */

#include <avr/pgmspace.h>
#include <util/delay.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/interrupt.hpp>
#include <mikroduino/usart.hpp>

using namespace MikroDuino;

static constexpr uint8_t LED0 = PB5;
static constexpr uint8_t LED1 = PB4;

static volatile uint8_t g_int0Count = 0;
static volatile uint8_t g_int1Count = 0;

void onButton0Press() {
    GPIO::toggle(LED0);
    ++g_int0Count;
}

void onButton1Press() {
    GPIO::toggle(LED1);
    ++g_int1Count;
}

static void delay_ms(uint16_t ms) { while (ms--) _delay_ms(1); }

int main() {
    GPIO::output(LED0);
    GPIO::output(LED1);
    GPIO::clear(LED0);
    GPIO::clear(LED1);
    GPIO::inputPullup(PD2);
    GPIO::inputPullup(PD3);

    USART0.begin(9600);
    USART0.writeLine_P(PSTR("Two interrupt sources: disable/enable vs detach/attach"));
    USART0.writeLine_P(PSTR("========================================================="));
    USART0.writeLine_P(PSTR("Button0 (INT0/PD2): pauses via disable()/enable() every 5 presses"));
    USART0.writeLine_P(PSTR("Button1 (INT1/PD3): pauses via detach()/attach() every 5 presses"));
    USART0.writeLine_P(PSTR(""));

    Interrupt.attach(IntSource::INT0, onButton0Press, IntSense::Falling);
    Interrupt.attach(IntSource::INT1, onButton1Press, IntSense::Falling);
    Interrupt.enableGlobal();

    while (true) {
        if (g_int0Count >= 5) {
            g_int0Count = 0;

            USART0.writeLine_P(PSTR(">> INT0: 5 presses reached. Interrupt.disable(INT0) for 3s."));
            USART0.writeLine_P(PSTR("   (handler stays registered — only the EIMSK bit clears)"));
            Interrupt.disable(IntSource::INT0);

            delay_ms(3000);

            Interrupt.enable(IntSource::INT0);   // no handler/sense needed — still remembered
            USART0.writeLine_P(PSTR(">> INT0: Interrupt.enable(INT0) — resumed with the SAME handler."));
            USART0.writeLine_P(PSTR(""));
        }

        if (g_int1Count >= 5) {
            g_int1Count = 0;

            USART0.writeLine_P(PSTR(">> INT1: 5 presses reached. Interrupt.detach(INT1) for 3s."));
            USART0.writeLine_P(PSTR("   (handler pointer is CLEARED, not just paused)"));
            Interrupt.detach(IntSource::INT1);

            delay_ms(3000);

            // detach() forgot the handler — attach() must supply it again,
            // exactly like the very first call in main() above.
            Interrupt.attach(IntSource::INT1, onButton1Press, IntSense::Falling);
            USART0.writeLine_P(PSTR(">> INT1: Interrupt.attach(INT1, onButton1Press, Falling) — full re-registration."));
            USART0.writeLine_P(PSTR(""));
        }
    }
}
