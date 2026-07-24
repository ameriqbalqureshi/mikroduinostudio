/*
 * DS3231 + EEPROM — Timestamped Temperature Ring Log — MikroDuino
 * Module SDK
 *
 * Project 5 of 6 in the examples/Modules/DS3231 series. Combines
 * DS3231's now()/temperatureRaw() with on-chip EEPROM to build a small,
 * fixed-capacity circular log: every LOG_PERIOD_MS, the current
 * timestamp and temperature are appended as one compact record: once
 * the log fills up, each new entry silently overwrites the OLDEST one
 * — a classic ring buffer, this time backed by EEPROM instead of RAM.
 * A button click dumps the whole log over USART, oldest entry first.
 *
 * Related example: examples/i2c/06_eeprom_rtc_datalogger already pairs
 * a DS1307 with an EEPROM, built entirely from raw I2CDriver calls
 * against both chips' generic register/address protocols. This project
 * is the same idea through the DS3231 class instead, and structures
 * the log as a fixed-size ring rather than a simple growing append —
 * a design that never needs an "is the EEPROM full yet" check, at the
 * cost of eventually discarding old data instead of stopping.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌─────────┬───────┬──────────────────────────────────────────┐
 *   │ SDA     │ PC4   │ DS3231 (projects 1-4)                      │
 *   │ SCL     │ PC5   │                                            │
 *   │ Button  │ PD2   │ Other leg to GND — click dumps the log      │
 *   │ TXD     │ PD1   │ USB-serial adapter, 9600 8N1               │
 *   └─────────┴───────┴──────────────────────────────────────────┘
 *
 * LOG_PERIOD_MS is set to 30 seconds here so a demo run can fill the
 * 20-entry ring and watch it wrap within about 10 minutes; a real
 * datalogger would more likely use minutes or hours — nothing about
 * the ring-buffer logic below depends on the specific period chosen.
 *
 * EEPROM layout, both parts using the same 0xA5 magic-byte "first
 * boot" pattern examples/Modules/LCD/06, /Button/06, and
 * /DCMotor/06/DHT22/06 all use:
 *   - LogHeader at address 0: magic byte, how many entries have ever
 *     been written (capped at LOG_CAPACITY), and the ring index the
 *     NEXT entry will land on.
 *   - LOG_CAPACITY LogEntry records immediately after the header, each
 *     one a compact {time, date, raw temperature} snapshot — 9 bytes,
 *     not a float, so no soft-float library is pulled in just to store
 *     a temperature reading.
 *
 * DS3231 concepts reused from projects 1-4:
 *   - DS3231(), begin(), now(), temperatureRaw() — this project stores
 *     the RAW 0.25 °C count rather than the converted temperature()
 *     float, exactly like DHT22/temperature-adjacent projects prefer
 *     integer USART formatting where possible: converting to °C only
 *     happens once, at print time, in dumpLog().
 *
 * Button concepts reused from Button project 1:
 *   - Button(pin), begin(), update(), clicked().
 *
 * Timing model: continues project 4's millis() clock rather than
 * reverting to a blocking `_delay_ms()` loop — with a genuinely
 * concurrent job (the button must stay responsive at ~1 ms cadence
 * while a 30 s logging interval elapses in the background) already
 * established, there's no reason to give it up, the same call
 * examples/Modules/DCMotor/05 and /Button/05 made about reusing a
 * previous project's non-blocking clock rather than reintroducing
 * blocking delays.
 */

#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/usart.hpp>
#include <mikroduino/i2c.hpp>
#include <mikroduino/eeprom.hpp>
#include <mikroduino/timer.hpp>
#include <mikroduino/registers.hpp>
#include <Button.hpp>
#include <DS3231.hpp>

using namespace MikroDuino;

DS3231 rtc;
Button  dumpButton(PD2);

// ── millis() clock (identical technique to examples/timer/02_software_millis_clock) ──

static constexpr uint16_t MICROS_PER_OVERFLOW = 1024;
static constexpr uint16_t MILLIS_INC = MICROS_PER_OVERFLOW / 1000;
static constexpr uint16_t FRACT_INC  = MICROS_PER_OVERFLOW % 1000;
static constexpr uint16_t FRACT_MAX  = 1000;

static volatile uint32_t g_millis = 0;
static volatile uint16_t g_fract  = 0;

ISR(TIMER0_OVF_vect) {
    uint32_t m = g_millis;
    uint16_t f = g_fract;
    m += MILLIS_INC;
    f += FRACT_INC;
    if (f >= FRACT_MAX) { f -= FRACT_MAX; ++m; }
    g_fract  = f;
    g_millis = m;
}

static uint32_t millis() {
    uint32_t snapshot;
    ATOMIC_BLOCK_START;
    snapshot = g_millis;
    ATOMIC_BLOCK_END;
    return snapshot;
}

// ── EEPROM ring log ──────────────────────────────────────────────────────

struct LogEntry {
    uint8_t hour, minute, second;
    uint8_t day, month, year;
    int16_t tempRaw;   // 0.25 C units, straight from temperatureRaw()
};

struct LogHeader {
    uint8_t magic;
    uint8_t count;       // entries ever written, capped at LOG_CAPACITY
    uint8_t nextIndex;   // ring slot the NEXT entry will be written to
};

static constexpr uint8_t  LOG_MAGIC     = 0xA5;
static constexpr uint8_t  LOG_CAPACITY  = 20;
static constexpr uint16_t HEADER_ADDR   = 0x000;
static constexpr uint16_t LOG_START_ADDR = HEADER_ADDR + sizeof(LogHeader);

static LogHeader g_header;

static void loadOrInitHeader() {
    g_header = EEPROM.get<LogHeader>(HEADER_ADDR);
    if (g_header.magic != LOG_MAGIC) {
        g_header.magic     = LOG_MAGIC;
        g_header.count     = 0;
        g_header.nextIndex = 0;
        EEPROM.put(HEADER_ADDR, g_header);
    }
}

static void appendLogEntry(const DateTime& dt, int16_t tempRaw) {
    LogEntry e;
    e.hour = dt.hour; e.minute = dt.minute; e.second = dt.second;
    e.day  = dt.day;  e.month  = dt.month;  e.year   = dt.year;
    e.tempRaw = tempRaw;

    uint16_t addr = LOG_START_ADDR + static_cast<uint16_t>(g_header.nextIndex) * sizeof(LogEntry);
    EEPROM.put(addr, e);   // every entry's timestamp differs, so update() would never skip a write anyway

    g_header.nextIndex = static_cast<uint8_t>((g_header.nextIndex + 1) % LOG_CAPACITY);
    if (g_header.count < LOG_CAPACITY) ++g_header.count;
    EEPROM.put(HEADER_ADDR, g_header);
}

static void printField2(uint8_t v) {
    USART0.write(static_cast<char>('0' + (v / 10)));
    USART0.write(static_cast<char>('0' + (v % 10)));
}

static void dumpLog() {
    USART0.writeLine_P(PSTR("--- Log dump (oldest first) ---"));
    if (g_header.count == 0) {
        USART0.writeLine_P(PSTR("(empty)"));
        return;
    }

    // Buffer not yet full: oldest entry is slot 0. Buffer full and
    // wrapped: the slot about to be overwritten (nextIndex) holds the
    // current oldest surviving entry.
    uint8_t startIndex = (g_header.count < LOG_CAPACITY) ? 0 : g_header.nextIndex;

    for (uint8_t i = 0; i < g_header.count; ++i) {
        uint8_t idx = static_cast<uint8_t>((startIndex + i) % LOG_CAPACITY);
        uint16_t addr = LOG_START_ADDR + static_cast<uint16_t>(idx) * sizeof(LogEntry);
        LogEntry e = EEPROM.get<LogEntry>(addr);

        USART0.write_P(PSTR("#"));
        USART0.writeInt(i);
        USART0.write_P(PSTR("  20"));
        printField2(e.year);
        USART0.write_P(PSTR("-"));
        printField2(e.month);
        USART0.write_P(PSTR("-"));
        printField2(e.day);
        USART0.write_P(PSTR("  "));
        printField2(e.hour);
        USART0.write_P(PSTR(":"));
        printField2(e.minute);
        USART0.write_P(PSTR(":"));
        printField2(e.second);
        USART0.write_P(PSTR("  "));
        USART0.writeFloat(e.tempRaw * 0.25f, 2);
        USART0.writeLine_P(PSTR(" C"));
    }
}

static constexpr uint32_t LOG_PERIOD_MS = 30000;

int main() {
    GPIO::inputPullup(PC4);
    GPIO::inputPullup(PC5);

    USART0.begin(9600);
    USART0.writeLine_P(PSTR("DS3231 + EEPROM timestamped ring log"));
    USART0.writeLine_P(PSTR("======================================"));
    USART0.writeLine_P(PSTR("Click the button anytime to dump the log."));
    USART0.writeLine_P(PSTR(""));

    I2C.beginMaster(100000UL);
    rtc.begin();
    dumpButton.begin();
    loadOrInitHeader();

    Timer0.mode(TimerMode::Normal);
    Timer0.prescaler(TimerPrescaler::DIV64);
    Timer0.start();
    Timer0.enableOverflow();
    sei();

    uint32_t lastUpdateMs = millis();
    uint32_t nextLogAt    = millis();

    while (true) {
        uint32_t now = millis();

        while (now - lastUpdateMs >= 1) {
            dumpButton.update();
            ++lastUpdateMs;
        }

        if (dumpButton.clicked()) {
            dumpLog();
        }

        if (now >= nextLogAt) {
            nextLogAt = now + LOG_PERIOD_MS;

            DateTime dt = rtc.now();
            int16_t  tempRaw = rtc.temperatureRaw();
            appendLogEntry(dt, tempRaw);

            USART0.write_P(PSTR("Logged entry #"));
            USART0.writeInt(g_header.count);
            USART0.write_P(PSTR(" at "));
            printField2(dt.hour);
            USART0.write_P(PSTR(":"));
            printField2(dt.minute);
            USART0.write_P(PSTR(":"));
            printField2(dt.second);
            USART0.writeLine_P(PSTR(""));
        }
    }
}
