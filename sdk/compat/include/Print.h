#pragma once
/*
 * Arduino Print base class for MikroDuino compat layer.
 * Provides print()/println() in terms of the pure-virtual write(uint8_t).
 * Inherit from Print and implement write() to get full Arduino print support.
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "WString.h"

#ifndef DEC
#define DEC 10
#endif
#ifndef HEX
#define HEX 16
#endif
#ifndef OCT
#define OCT  8
#endif
#ifndef BIN
#define BIN  2
#endif

class Print {
public:
    virtual ~Print() = default;

    // Subclasses must implement this one method.
    virtual size_t write(uint8_t) = 0;

    virtual size_t write(const uint8_t *buf, size_t n);

    size_t write(const char *str) {
        if (!str) return 0;
        return write(reinterpret_cast<const uint8_t *>(str), strlen(str));
    }

    // ── print (no newline) ────────────────────────────────────────────────────
    size_t print(const char *);
    size_t print(char);
    size_t print(int,           int base = DEC);
    size_t print(unsigned int,  int base = DEC);
    size_t print(long,          int base = DEC);
    size_t print(unsigned long, int base = DEC);
    size_t print(double,        int digits = 2);
    size_t print(const String &);

    // ── println (appends \r\n) ────────────────────────────────────────────────
    size_t println();
    size_t println(const char *);
    size_t println(char);
    size_t println(int,           int base = DEC);
    size_t println(unsigned int,  int base = DEC);
    size_t println(long,          int base = DEC);
    size_t println(unsigned long, int base = DEC);
    size_t println(double,        int digits = 2);
    size_t println(const String &);

private:
    size_t _printNumber(unsigned long n, uint8_t base);
    size_t _printFloat(double number, uint8_t digits);
};
