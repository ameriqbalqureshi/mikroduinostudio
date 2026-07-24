#pragma once
/*
 * Arduino-compatible String class for MikroDuino.
 * Heap-allocated, ref-counted by copy — behaves like Arduino's String.
 * Required by many third-party libraries that use String internally.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <avr/pgmspace.h>

class __FlashStringHelper;
#define F(string_literal) (reinterpret_cast<const __FlashStringHelper *>(PSTR(string_literal)))

class String {
public:
    // ── Constructors ──────────────────────────────────────────────────────────
    String();
    String(const char* cstr);
    String(const String& other);
    String(char c);
    String(unsigned char val,  unsigned char base = 10);
    String(int val,            unsigned char base = 10);
    String(unsigned int val,   unsigned char base = 10);
    String(long val,           unsigned char base = 10);
    String(unsigned long val,  unsigned char base = 10);
    String(float val,          unsigned char decimalPlaces = 2);
    String(double val,         unsigned char decimalPlaces = 2);
    explicit String(const __FlashStringHelper* fstr);
    ~String();

    // ── Assignment ────────────────────────────────────────────────────────────
    String& operator=(const String& rhs);
    String& operator=(const char* cstr);
    String& operator=(char c);

    // ── Concatenation (in-place) ──────────────────────────────────────────────
    String& operator+=(const String& rhs);
    String& operator+=(const char* cstr);
    String& operator+=(char c);
    String& operator+=(unsigned char b);
    String& operator+=(int num);
    String& operator+=(unsigned int num);
    String& operator+=(long num);
    String& operator+=(unsigned long num);
    String& operator+=(float num);
    String& operator+=(double num);

    // ── Concatenation (creating new) ──────────────────────────────────────────
    friend String operator+(const String& lhs, const String& rhs);
    friend String operator+(const String& lhs, const char* rhs);
    friend String operator+(const char* lhs,   const String& rhs);
    friend String operator+(const String& lhs, char rhs);

    bool concat(const String& str);
    bool concat(const char* cstr);
    bool concat(const char* cstr, unsigned int len);
    bool concat(char c);
    bool concat(unsigned char c);
    bool concat(int num);
    bool concat(unsigned int num);
    bool concat(long num);
    bool concat(unsigned long num);
    bool concat(float num);
    bool concat(double num);

    // ── Comparison ────────────────────────────────────────────────────────────
    bool equals(const String& s) const;
    bool equals(const char* cstr) const;
    bool equalsIgnoreCase(const String& s) const;
    int  compareTo(const String& s) const;

    bool operator==(const String& rhs) const { return equals(rhs); }
    bool operator==(const char* rhs)   const { return equals(rhs); }
    bool operator!=(const String& rhs) const { return !equals(rhs); }
    bool operator!=(const char* rhs)   const { return !equals(rhs); }
    bool operator<(const String& rhs)  const;
    bool operator>(const String& rhs)  const;
    bool operator<=(const String& rhs) const;
    bool operator>=(const String& rhs) const;

    bool startsWith(const String& prefix) const;
    bool startsWith(const String& prefix, unsigned int offset) const;
    bool endsWith(const String& suffix) const;

    // ── Element access ────────────────────────────────────────────────────────
    char   operator[](unsigned int index) const;
    char&  operator[](unsigned int index);
    char   charAt(unsigned int index) const { return operator[](index); }
    void   setCharAt(unsigned int index, char c);

    // ── Searching ─────────────────────────────────────────────────────────────
    int indexOf(char ch) const;
    int indexOf(char ch, unsigned int fromIndex) const;
    int indexOf(const String& str) const;
    int indexOf(const String& str, unsigned int fromIndex) const;
    int lastIndexOf(char ch) const;
    int lastIndexOf(const String& str) const;

    // ── Extraction ────────────────────────────────────────────────────────────
    String substring(unsigned int beginIndex) const;
    String substring(unsigned int beginIndex, unsigned int endIndex) const;

    // ── Modification ─────────────────────────────────────────────────────────
    void toLowerCase();
    void toUpperCase();
    void trim();
    void replace(char find, char replaceWith);
    void replace(const String& find, const String& replaceWith);
    void remove(unsigned int index);
    void remove(unsigned int index, unsigned int count);

    // ── Conversion ────────────────────────────────────────────────────────────
    long   toInt()    const;
    float  toFloat()  const;
    double toDouble() const;

    // ── Properties ───────────────────────────────────────────────────────────
    unsigned int length()  const { return _len; }
    bool         isEmpty() const { return _len == 0; }
    void         reserve(unsigned int size);

    const char*  c_str() const { return _buf ? _buf : ""; }
    operator const char*() const { return c_str(); }

private:
    char*        _buf;
    unsigned int _len;
    unsigned int _cap;

    bool _reserve(unsigned int cap);
    void _init(const char* cstr, unsigned int len);
    void _fromLong(long val, unsigned char base);
    void _fromULong(unsigned long val, unsigned char base);
    void _fromFloat(double val, unsigned char places);
};
