/*
 * I2C EEPROM + RTC Datalogger — MikroDuino SDK (capstone)
 *
 * Project 6 of 6 in the examples/i2c series. Two I2C devices share one
 * bus: a DS1307 RTC (address 0x68, projects 4-5) timestamps events, and
 * an AT24C32 EEPROM (address 0x50, projects 2-3) stores them in a
 * fixed-size ring buffer that survives power loss. Everything here is
 * still built from the same handful of I2CDriver primitives used
 * throughout the series — this project's novelty is entirely in how
 * those primitives get combined and sequenced, not in any new I2C API.
 *
 * Hardware (ATmega328P @ 16 MHz):
 *
 *   ┌─────────────┬───────┬──────────────────────────────────────────┐
 *   │ Signal      │ Pin   │ Wiring                                    │
 *   ├─────────────┼───────┼──────────────────────────────────────────┤
 *   │ SDA         │ PC4   │ Shared bus: DS1307 module + AT24C32       │
 *   │ SCL         │ PC5   │ Shared bus: DS1307 module + AT24C32       │
 *   │ TXD         │ PD1   │ USB-serial adapter                        │
 *   │ LED         │ PB5   │ Pulses once per logged event              │
 *   │ LOG button  │ PD2   │ Other leg to GND, internal pull-up — logs │
 *   │             │       │ a timestamped event on press               │
 *   │ DUMP button │ PD3   │ Other leg to GND, internal pull-up —      │
 *   │             │       │ replays the whole log over USART           │
 *   └─────────────┴───────┴──────────────────────────────────────────┘
 *
 * Many DS1307 breakout boards already carry an onboard AT24C32 at 0x50,
 * so in practice this whole capstone often runs off a single small RTC
 * module with nothing extra to wire up.
 *
 * EEPROM layout (AT24C32, 4096 bytes, 32-byte pages):
 *
 *   0x0000  Header: { magic:1, nextSlot:1, count:1 }  (3 bytes, page 0)
 *   0x0020  Log ring: 32 slots x 8-byte records = 256 bytes (pages 1-8)
 *           Each record: { second, minute, hour, day, month, year,
 *                           dayOfWeek, eventId }, all plain decimal
 *           (already BCD-decoded by ds1307Get() before storage — the
 *           EEPROM just holds ordinary bytes, no BCD involved on this
 *           side of the transaction).
 *
 * The log slot base address (0x0020) is deliberately page-aligned and
 * the 8-byte record size divides the 32-byte page size evenly, so no
 * single record write ever straddles a page boundary — but this project
 * still routes every write through the same page-safe eepromWriteBuffer()
 * helper introduced in project 3, since relying on the arithmetic working
 * out is exactly the kind of assumption that quietly breaks if someone
 * later changes LOG_CAPACITY or the record layout.
 *
 * Ring buffer bookkeeping (persisted in the header so it survives reset):
 *   nextSlot — index the *next* logged event will be written to.
 *   count    — number of valid entries so far, capped at LOG_CAPACITY.
 *              Once count == LOG_CAPACITY, the ring has wrapped and the
 *              oldest surviving entry is the one at nextSlot (about to be
 *              overwritten by the next log() call) — the same relationship
 *              any ring buffer has between its write cursor and its
 *              oldest element once it's full.
 *
 * I2C concepts reused from the whole series:
 *   - I2C.beginMaster(), I2C.write(), I2C.writeRead() — the entire API
 *     surface used by every project in this series is sufficient for a
 *     complete multi-device application; nothing more is needed even at
 *     this level of complexity.
 *   - Device selection is just a different 7-bit address argument to the
 *     same calls — I2CDriver has no notion of "which device" beyond the
 *     address you pass it each time, which is exactly how the real I2C
 *     bus protocol itself works.
 */

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <string.h>
#include <mikroduino/gpio.hpp>
#include <mikroduino/usart.hpp>
#include <mikroduino/i2c.hpp>

using namespace MikroDuino;

static constexpr uint8_t  LED           = PB5;
static constexpr uint8_t  BUTTON_LOG    = PD2;
static constexpr uint8_t  BUTTON_DUMP   = PD3;

static constexpr uint8_t  EEPROM_ADDR   = 0x50;
static constexpr uint16_t PAGE_SIZE     = 32;
static constexpr uint8_t  DS1307_ADDR   = 0x68;

static void delay_ms(uint16_t ms) { while (ms--) _delay_ms(1); }

// ── RTC helpers (see projects 4-5 for the full walkthrough) ────────────────

static uint8_t bcdToDec(uint8_t bcd) { return static_cast<uint8_t>((bcd >> 4) * 10 + (bcd & 0x0F)); }
static uint8_t decToBcd(uint8_t dec) { return static_cast<uint8_t>(((dec / 10) << 4) | (dec % 10)); }

struct DateTime {
    uint8_t second, minute, hour, dayOfWeek, day, month, year;
};

static bool ds1307Set(const DateTime& dt) {
    uint8_t packet[8] = {
        0x00,
        decToBcd(dt.second), decToBcd(dt.minute), decToBcd(dt.hour),
        decToBcd(dt.dayOfWeek), decToBcd(dt.day), decToBcd(dt.month), decToBcd(dt.year)
    };
    return I2C.write(DS1307_ADDR, packet, sizeof(packet)) == I2CResult::Ok;
}

static bool ds1307Get(DateTime& dt) {
    uint8_t reg = 0x00, raw[7];
    if (I2C.writeRead(DS1307_ADDR, &reg, 1, raw, 7) != I2CResult::Ok) return false;
    dt.second    = bcdToDec(raw[0] & 0x7F);
    dt.minute    = bcdToDec(raw[1]);
    dt.hour      = bcdToDec(raw[2] & 0x3F);
    dt.dayOfWeek = bcdToDec(raw[3]);
    dt.day       = bcdToDec(raw[4]);
    dt.month     = bcdToDec(raw[5]);
    dt.year      = bcdToDec(raw[6]);
    return true;
}

static bool ds1307ClockHalted() {
    uint8_t reg = 0x00, seconds = 0;
    if (I2C.writeRead(DS1307_ADDR, &reg, 1, &seconds, 1) != I2CResult::Ok) return true;
    return (seconds & 0x80) != 0;
}

// ── EEPROM helpers (see projects 2-3 for the full walkthrough) ─────────────

static bool eepromWaitReady() {
    for (uint16_t attempts = 0; attempts < 200; ++attempts) {
        if (I2C.write(EEPROM_ADDR, nullptr, 0) == I2CResult::Ok) return true;
        _delay_us(50);
    }
    return false;
}

static bool eepromWritePage(uint16_t memAddr, const uint8_t* data, uint8_t len) {
    uint8_t packet[2 + PAGE_SIZE];
    packet[0] = static_cast<uint8_t>(memAddr >> 8);
    packet[1] = static_cast<uint8_t>(memAddr & 0xFF);
    memcpy(&packet[2], data, len);
    if (I2C.write(EEPROM_ADDR, packet, static_cast<uint8_t>(2 + len)) != I2CResult::Ok) return false;
    return eepromWaitReady();
}

static bool eepromWriteBuffer(uint16_t memAddr, const uint8_t* data, uint16_t len) {
    while (len > 0) {
        uint16_t offsetInPage = memAddr % PAGE_SIZE;
        uint16_t roomInPage   = PAGE_SIZE - offsetInPage;
        uint8_t  chunk        = static_cast<uint8_t>(len < roomInPage ? len : roomInPage);
        if (!eepromWritePage(memAddr, data, chunk)) return false;
        memAddr += chunk;
        data    += chunk;
        len     -= chunk;
    }
    return true;
}

static bool eepromReadBuffer(uint16_t memAddr, uint8_t* out, uint16_t len) {
    uint8_t addrBytes[2] = {
        static_cast<uint8_t>(memAddr >> 8),
        static_cast<uint8_t>(memAddr & 0xFF)
    };
    return I2C.writeRead(EEPROM_ADDR, addrBytes, 2, out, len) == I2CResult::Ok;
}

// ── Log ring buffer ─────────────────────────────────────────────────────────

static constexpr uint16_t HEADER_ADDR  = 0x0000;
static constexpr uint16_t LOG_BASE     = 0x0020;   // page-aligned (0x20 = 32)
static constexpr uint8_t  LOG_CAPACITY = 32;        // slots
static constexpr uint8_t  RECORD_SIZE  = 8;         // bytes per slot
static constexpr uint8_t  HEADER_MAGIC = 0xA5;

struct LogRecord {
    uint8_t second, minute, hour, day, month, year, dayOfWeek, eventId;
};

struct LogHeader {
    uint8_t magic;
    uint8_t nextSlot;
    uint8_t count;
};

static bool headerLoad(LogHeader& hdr) {
    uint8_t raw[3];
    if (!eepromReadBuffer(HEADER_ADDR, raw, 3)) return false;
    hdr.magic = raw[0]; hdr.nextSlot = raw[1]; hdr.count = raw[2];
    return true;
}

static bool headerSave(const LogHeader& hdr) {
    uint8_t raw[3] = { hdr.magic, hdr.nextSlot, hdr.count };
    return eepromWriteBuffer(HEADER_ADDR, raw, 3);
}

// Append one record at the ring's write cursor, then advance and persist
// the cursor. This is the only place slot indices turn into EEPROM
// addresses — everything else works in slot numbers.
static bool logAppend(const LogRecord& rec, LogHeader& hdr) {
    uint16_t addr = LOG_BASE + static_cast<uint16_t>(hdr.nextSlot) * RECORD_SIZE;
    const uint8_t* raw = reinterpret_cast<const uint8_t*>(&rec);
    if (!eepromWriteBuffer(addr, raw, RECORD_SIZE)) return false;

    hdr.nextSlot = static_cast<uint8_t>((hdr.nextSlot + 1) % LOG_CAPACITY);
    if (hdr.count < LOG_CAPACITY) hdr.count++;
    return headerSave(hdr);
}

static bool logReadSlot(uint8_t slot, LogRecord& rec) {
    uint16_t addr = LOG_BASE + static_cast<uint16_t>(slot) * RECORD_SIZE;
    return eepromReadBuffer(addr, reinterpret_cast<uint8_t*>(&rec), RECORD_SIZE);
}

// ── Printing ─────────────────────────────────────────────────────────────

static const char DAY0[] PROGMEM = "Sun";
static const char DAY1[] PROGMEM = "Mon";
static const char DAY2[] PROGMEM = "Tue";
static const char DAY3[] PROGMEM = "Wed";
static const char DAY4[] PROGMEM = "Thu";
static const char DAY5[] PROGMEM = "Fri";
static const char DAY6[] PROGMEM = "Sat";
static const char* const DAY_NAMES[7] PROGMEM = { DAY0, DAY1, DAY2, DAY3, DAY4, DAY5, DAY6 };

static void printField2(uint8_t v) {
    char buf[3];
    buf[0] = static_cast<char>('0' + (v / 10));
    buf[1] = static_cast<char>('0' + (v % 10));
    buf[2] = '\0';
    USART0.write(buf);
}

static void printRecord(const LogRecord& r) {
    const char* dayName = reinterpret_cast<const char*>(pgm_read_word(&DAY_NAMES[(r.dayOfWeek - 1) % 7]));
    USART0.write_P(dayName);
    USART0.write_P(PSTR(" 20")); printField2(r.year);
    USART0.write_P(PSTR("-"));   printField2(r.month);
    USART0.write_P(PSTR("-"));   printField2(r.day);
    USART0.write_P(PSTR("  "));  printField2(r.hour);
    USART0.write_P(PSTR(":"));   printField2(r.minute);
    USART0.write_P(PSTR(":"));   printField2(r.second);
    USART0.write_P(PSTR("   event #"));
    USART0.writeInt(r.eventId);
    USART0.writeLine_P(PSTR(""));
}

// Replay every stored record, oldest first. While the ring hasn't wrapped
// yet (count < LOG_CAPACITY), the oldest entry is simply slot 0. Once
// full, the oldest surviving entry is at nextSlot — the slot about to be
// overwritten by the next append — and the dump walks forward from there,
// wrapping around, for exactly `count` entries.
static void logDump(const LogHeader& hdr) {
    USART0.write_P(PSTR("--- Log dump: "));
    USART0.writeInt(hdr.count);
    USART0.write_P(PSTR(" / "));
    USART0.writeInt(LOG_CAPACITY);
    USART0.writeLine_P(PSTR(" entries (oldest first) ---"));

    if (hdr.count == 0) {
        USART0.writeLine_P(PSTR("(empty — press the LOG button to add an entry)"));
        return;
    }

    uint8_t startSlot = (hdr.count < LOG_CAPACITY) ? 0 : hdr.nextSlot;
    for (uint8_t i = 0; i < hdr.count; ++i) {
        uint8_t slot = static_cast<uint8_t>((startSlot + i) % LOG_CAPACITY);
        LogRecord rec;
        if (logReadSlot(slot, rec)) {
            USART0.write_P(PSTR("  "));
            printRecord(rec);
        } else {
            USART0.writeLine_P(PSTR("  <read error>"));
        }
    }
    USART0.writeLine_P(PSTR("--- end of log ---"));
}

// Polled falling-edge debounce for one button, shared by both buttons in
// main(). See project 5 for the same pattern with a single button.
static bool buttonPressed(uint8_t pin, bool& lastState) {
    bool nowLow = (GPIO::read(pin) == false);
    bool edge = nowLow && lastState;
    if (edge) {
        delay_ms(30);
        edge = (GPIO::read(pin) == false);
    }
    lastState = !nowLow;
    return edge;
}

int main() {
    GPIO::output(LED);
    GPIO::clear(LED);
    GPIO::inputPullup(BUTTON_LOG);
    GPIO::inputPullup(BUTTON_DUMP);
    GPIO::inputPullup(PC4);
    GPIO::inputPullup(PC5);

    USART0.begin(9600);
    I2C.beginMaster(100000UL);

    USART0.writeLine_P(PSTR("I2C EEPROM + RTC Datalogger"));
    USART0.writeLine_P(PSTR("============================"));
    USART0.writeLine_P(PSTR("LOG button (PD2)  -> record a timestamped event"));
    USART0.writeLine_P(PSTR("DUMP button (PD3) -> replay the stored log"));
    USART0.writeLine_P(PSTR(""));

    if (ds1307ClockHalted()) {
        DateTime startup = { 0, 0, 12, 2, 20, 7, 26 };   // Mon 2026-07-20 12:00:00
        ds1307Set(startup);
        USART0.writeLine_P(PSTR("RTC oscillator was halted -> initialised to a default time."));
    }

    LogHeader hdr;
    if (!headerLoad(hdr) || hdr.magic != HEADER_MAGIC) {
        // First run ever (or corrupt header): start an empty ring.
        hdr = { HEADER_MAGIC, 0, 0 };
        headerSave(hdr);
        USART0.writeLine_P(PSTR("EEPROM log header initialised (empty ring)."));
    } else {
        USART0.write_P(PSTR("Existing log found: "));
        USART0.writeInt(hdr.count);
        USART0.writeLine_P(PSTR(" entries (persisted across reset)."));
    }
    USART0.writeLine_P(PSTR(""));

    uint8_t nextEventId = hdr.count;   // cosmetic running counter, not part of the ring's own bookkeeping
    bool lastLog = true, lastDump = true;

    while (true) {
        if (buttonPressed(BUTTON_LOG, lastLog)) {
            DateTime now{};
            if (ds1307Get(now)) {
                LogRecord rec = {
                    now.second, now.minute, now.hour,
                    now.day, now.month, now.year, now.dayOfWeek,
                    nextEventId
                };
                bool ok = logAppend(rec, hdr);

                GPIO::set(LED); delay_ms(60); GPIO::clear(LED);

                USART0.write_P(PSTR("Logged: "));
                printRecord(rec);
                USART0.writeLine_P(ok ? PSTR("  -> saved to EEPROM [OK]")
                                       : PSTR("  -> EEPROM WRITE FAILED"));
                if (ok) ++nextEventId;
            } else {
                USART0.writeLine_P(PSTR("LOG pressed but RTC read failed — check wiring."));
            }
        }

        if (buttonPressed(BUTTON_DUMP, lastDump)) {
            logDump(hdr);
        }

        delay_ms(5);   // keeps both buttons responsive without busy-spinning the bus
    }
}
