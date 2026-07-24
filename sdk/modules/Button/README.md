# Button

Push button with debounce, long press, click and double-click event detection

**Header:** `include/Button.hpp`
**Header-only** — no `.cpp` required, `#include <Button.hpp>` is enough
**Extends:** Phase-1 `Debounce` (`sched/debounce.hpp`) with long press and double-click gesture detection

## Overview

`Button` wraps a single GPIO pin driving a physical push button and turns its raw,
bouncy signal into clean, one-shot events: `pressed()`, `released()`, `clicked()`,
`doubleClicked()`, and `longPressed()`. It self-reads the pin and self-times every
gesture internally — no external millisecond counter or state machine needed in
user code.

Reach for `Button` any time a project reads a mechanical tactile button or
momentary switch. If you need to debounce a *non-button* digital signal (e.g. a
mechanical limit switch used only for level changes, not gestures), use the
lighter-weight `Debounce` (`sched/debounce.hpp`) instead — `Button` is built on
top of the same idea but adds hold-time and multi-click tracking.

## Wiring

| Signal | Connect to | Notes |
|---|---|---|
| Button leg 1 | Any GPIO pin | Passed to the `Button` constructor |
| Button leg 2 | GND | Default is active-LOW: `begin()` enables the pin's internal pull-up, no external resistor needed |

For an active-HIGH button (pulls the pin to VCC when pressed), wire an external
pull-down resistor and pass `activeLow = false` to the constructor.

## API

**Timing model:** call `update()` once per millisecond (or another known, fixed
interval) — its internal timers (debounce, hold time, double-click window) count
*calls*, not wall-clock time, so a steady call rate is required for the
`...Ms` parameters to mean what their names say.

| Method | Description |
|---|---|
| `Button(pin, activeLow=true, debounceMs=20, longPressMs=600, doubleClickMs=350)` | Constructor |
| `begin()` | Configures the pin direction (pull-up if active-low) and takes an initial reading |
| `update()` | Advances debounce/timing state by one tick — call every ~1 ms |
| `pressed()` | One-shot: true once when the button just debounced to pressed |
| `released()` | One-shot: true once when the button just debounced to released |
| `clicked()` | One-shot: true once on a short press+release (no long press fired) |
| `doubleClicked()` | One-shot: true once when two clicks land within `doubleClickMs` |
| `longPressed()` | One-shot: true once after being held ≥ `longPressMs` |
| `isDown()` | State query (no side effect): true while the button is currently held |
| `heldMs()` | State query (no side effect): milliseconds the button has been held |
| `setLongPressMs(ms)` | Reconfigure the long-press threshold at runtime |
| `setDoubleClickMs(ms)` | Reconfigure the double-click window at runtime |

Each one-shot event method returns `true` exactly once per occurrence, then
automatically clears itself — safe to poll every loop iteration.

## Example

```cpp
#include <util/delay.h>
#include <mikroduino/gpio.hpp>
#include <Button.hpp>

using namespace MikroDuino;

static constexpr uint8_t LED = PB5;

Button button(PD2);   // active-low, defaults: 20 ms debounce, 600 ms long-press, 350 ms double-click

int main() {
    GPIO::output(LED);
    GPIO::clear(LED);

    button.begin();

    while (true) {
        button.update();

        if (button.clicked()) {
            GPIO::toggle(LED);
        }

        _delay_ms(1);   // Button::update() expects to be called ~every 1 ms
    }
}
```

A full six-project walkthrough — from this basic click-to-toggle, through
state queries, non-blocking scheduling, double-click menus, multi-button
auto-repeat, up to an EEPROM-backed combination lock — ships in
`examples/Modules/Button/01_basic_click_led` through `06_combination_lock`
(also under `Samples/Modules/Button/`). Open any of them from the IDE's
**File > Examples** menu.

## See also

- Full reference already written: [`sdk/docs/core-libraries.md`](../../../sdk/docs/core-libraries.md), section 9, "Push Button".
