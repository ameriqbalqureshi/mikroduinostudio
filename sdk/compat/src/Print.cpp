#include "Print.h"
#include <string.h>
#include <stdlib.h>

size_t Print::write(const uint8_t *buf, size_t n) {
    size_t written = 0;
    while (n--) written += write(*buf++);
    return written;
}

// ── print ─────────────────────────────────────────────────────────────────────

size_t Print::print(const char *s) {
    return write(reinterpret_cast<const uint8_t *>(s), strlen(s));
}

size_t Print::print(char c) {
    return write(static_cast<uint8_t>(c));
}

size_t Print::print(int n, int base) {
    return print(static_cast<long>(n), base);
}

size_t Print::print(unsigned int n, int base) {
    return print(static_cast<unsigned long>(n), base);
}

size_t Print::print(long n, int base) {
    if (base == DEC && n < 0) {
        size_t s = write('-');
        return s + _printNumber(static_cast<unsigned long>(-n), base);
    }
    return _printNumber(static_cast<unsigned long>(n), base);
}

size_t Print::print(unsigned long n, int base) {
    return _printNumber(n, base);
}

size_t Print::print(double d, int digits) {
    return _printFloat(d, static_cast<uint8_t>(digits));
}

size_t Print::print(const String &s) {
    return write(reinterpret_cast<const uint8_t *>(s.c_str()), s.length());
}

// ── println ───────────────────────────────────────────────────────────────────

size_t Print::println() {
    return write('\r') + write('\n');
}

size_t Print::println(const char *s)           { return print(s)       + println(); }
size_t Print::println(char c)                  { return print(c)       + println(); }
size_t Print::println(int n, int base)         { return print(n, base) + println(); }
size_t Print::println(unsigned int n, int base){ return print(n, base) + println(); }
size_t Print::println(long n, int base)        { return print(n, base) + println(); }
size_t Print::println(unsigned long n, int base){ return print(n, base)+ println(); }
size_t Print::println(double d, int digits)    { return print(d, digits)+println(); }
size_t Print::println(const String &s)         { return print(s)       + println(); }

// ── private helpers ───────────────────────────────────────────────────────────

size_t Print::_printNumber(unsigned long n, uint8_t base) {
    char buf[8 * sizeof(long) + 1];
    char *p = buf + sizeof(buf) - 1;
    *p = '\0';
    if (n == 0) {
        *--p = '0';
    } else {
        while (n > 0) {
            char d = static_cast<char>(n % base);
            *--p = (d < 10) ? ('0' + d) : ('A' + d - 10);
            n /= base;
        }
    }
    return write(reinterpret_cast<const uint8_t *>(p), strlen(p));
}

size_t Print::_printFloat(double number, uint8_t digits) {
    size_t n = 0;
    if (number < 0.0) { n += write('-'); number = -number; }

    // Round to requested decimal places
    double rounding = 0.5;
    for (uint8_t i = 0; i < digits; i++) rounding /= 10.0;
    number += rounding;

    unsigned long int_part = static_cast<unsigned long>(number);
    double remainder = number - static_cast<double>(int_part);
    n += print(int_part);

    if (digits > 0) {
        n += write('.');
        while (digits-- > 0) {
            remainder *= 10.0;
            unsigned int d = static_cast<unsigned int>(remainder);
            n += write('0' + d);
            remainder -= d;
        }
    }
    return n;
}
