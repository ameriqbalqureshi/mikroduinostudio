/*
 * WString.cpp — Arduino-compatible String class implementation.
 */

#include "../include/WString.h"
#include <ctype.h>     // tolower, toupper, isspace
#include <avr/pgmspace.h>

// ── Internal helpers ─────────────────────────────────────────────────────────

bool String::_reserve(unsigned int cap) {
    if (_cap >= cap + 1) return true;   // already large enough
    unsigned int newcap = cap + 1;
    char* nb = static_cast<char*>(realloc(_buf, newcap));
    if (!nb) return false;
    _buf = nb;
    _cap = newcap;
    return true;
}

void String::_init(const char* cstr, unsigned int len) {
    if (!_reserve(len)) return;
    memcpy(_buf, cstr, len);
    _buf[len] = '\0';
    _len = len;
}

void String::_fromLong(long val, unsigned char base) {
    char buf[34];
    ltoa(val, buf, base);
    _init(buf, strlen(buf));
}

void String::_fromULong(unsigned long val, unsigned char base) {
    char buf[34];
    ultoa(val, buf, base);
    _init(buf, strlen(buf));
}

void String::_fromFloat(double val, unsigned char places) {
    char buf[16];
    dtostrf(val, 0, places, buf);
    _init(buf, strlen(buf));
}

// ── Constructors / Destructor ─────────────────────────────────────────────────

String::String() : _buf(nullptr), _len(0), _cap(0) {}

String::String(const char* cstr) : _buf(nullptr), _len(0), _cap(0) {
    if (cstr) _init(cstr, strlen(cstr));
}

String::String(const String& other) : _buf(nullptr), _len(0), _cap(0) {
    if (other._buf) _init(other._buf, other._len);
}

String::String(const __FlashStringHelper* fstr) : _buf(nullptr), _len(0), _cap(0) {
    if (!fstr) return;
    const char* p = reinterpret_cast<const char*>(fstr);
    unsigned int len = strlen_P(p);
    if (!_reserve(len)) return;
    memcpy_P(_buf, p, len + 1);
    _len = len;
}

String::String(char c) : _buf(nullptr), _len(0), _cap(0) {
    char s[2] = {c, '\0'};
    _init(s, 1);
}

String::String(unsigned char val, unsigned char base) : _buf(nullptr), _len(0), _cap(0) {
    _fromULong(val, base);
}

String::String(int val, unsigned char base) : _buf(nullptr), _len(0), _cap(0) {
    _fromLong(val, base);
}

String::String(unsigned int val, unsigned char base) : _buf(nullptr), _len(0), _cap(0) {
    _fromULong(val, base);
}

String::String(long val, unsigned char base) : _buf(nullptr), _len(0), _cap(0) {
    _fromLong(val, base);
}

String::String(unsigned long val, unsigned char base) : _buf(nullptr), _len(0), _cap(0) {
    _fromULong(val, base);
}

String::String(float val, unsigned char decimalPlaces) : _buf(nullptr), _len(0), _cap(0) {
    _fromFloat(static_cast<double>(val), decimalPlaces);
}

String::String(double val, unsigned char decimalPlaces) : _buf(nullptr), _len(0), _cap(0) {
    _fromFloat(val, decimalPlaces);
}

String::~String() {
    free(_buf);
}

// ── Assignment ───────────────────────────────────────────────────────────────

String& String::operator=(const String& rhs) {
    if (this != &rhs) _init(rhs._buf ? rhs._buf : "", rhs._len);
    return *this;
}

String& String::operator=(const char* cstr) {
    if (cstr) _init(cstr, strlen(cstr));
    else { _len = 0; if (_buf) _buf[0] = '\0'; }
    return *this;
}

String& String::operator=(char c) {
    char s[2] = {c, '\0'};
    _init(s, 1);
    return *this;
}

// ── Concat helpers ───────────────────────────────────────────────────────────

bool String::concat(const char* cstr, unsigned int len) {
    if (!cstr || len == 0) return true;
    if (!_reserve(_len + len)) return false;
    memcpy(_buf + _len, cstr, len);
    _len += len;
    _buf[_len] = '\0';
    return true;
}

bool String::concat(const char* cstr)   { return cstr && concat(cstr, strlen(cstr)); }
bool String::concat(const String& str)  { return concat(str._buf, str._len); }
bool String::concat(char c)             { return concat(&c, 1); }
bool String::concat(unsigned char c)    { return concat(static_cast<unsigned long>(c)); }
bool String::concat(int num)            { char b[20]; ltoa(num,b,10);  return concat(b); }
bool String::concat(unsigned int num)   { char b[20]; ultoa(num,b,10); return concat(b); }
bool String::concat(long num)           { char b[20]; ltoa(num,b,10);  return concat(b); }
bool String::concat(unsigned long num)  { char b[20]; ultoa(num,b,10); return concat(b); }
bool String::concat(float num)  { char b[16]; dtostrf(num,0,2,b); return concat(b); }
bool String::concat(double num) { char b[16]; dtostrf(num,0,2,b); return concat(b); }

// ── += operators ─────────────────────────────────────────────────────────────

String& String::operator+=(const String& rhs)      { concat(rhs);  return *this; }
String& String::operator+=(const char* cstr)       { concat(cstr); return *this; }
String& String::operator+=(char c)                 { concat(c);    return *this; }
String& String::operator+=(unsigned char b)        { concat(b);    return *this; }
String& String::operator+=(int num)                { concat(num);  return *this; }
String& String::operator+=(unsigned int num)       { concat(num);  return *this; }
String& String::operator+=(long num)               { concat(num);  return *this; }
String& String::operator+=(unsigned long num)      { concat(num);  return *this; }
String& String::operator+=(float num)              { concat(num);  return *this; }
String& String::operator+=(double num)             { concat(num);  return *this; }

// ── + operators ──────────────────────────────────────────────────────────────

String operator+(const String& lhs, const String& rhs) { String s(lhs); s += rhs; return s; }
String operator+(const String& lhs, const char* rhs)   { String s(lhs); s += rhs; return s; }
String operator+(const char* lhs, const String& rhs)   { String s(lhs); s += rhs; return s; }
String operator+(const String& lhs, char rhs)          { String s(lhs); s += rhs; return s; }

// ── Comparison ───────────────────────────────────────────────────────────────

int String::compareTo(const String& s) const {
    return strcmp(_buf ? _buf : "", s._buf ? s._buf : "");
}

bool String::equals(const String& s) const {
    return _len == s._len && strcmp(_buf ? _buf : "", s._buf ? s._buf : "") == 0;
}

bool String::equals(const char* cstr) const {
    return strcmp(_buf ? _buf : "", cstr ? cstr : "") == 0;
}

bool String::equalsIgnoreCase(const String& s) const {
    if (_len != s._len) return false;
    for (unsigned int i = 0; i < _len; ++i)
        if (tolower(_buf[i]) != tolower(s._buf[i])) return false;
    return true;
}

bool String::operator<(const String& rhs)  const { return compareTo(rhs) < 0; }
bool String::operator>(const String& rhs)  const { return compareTo(rhs) > 0; }
bool String::operator<=(const String& rhs) const { return compareTo(rhs) <= 0; }
bool String::operator>=(const String& rhs) const { return compareTo(rhs) >= 0; }

bool String::startsWith(const String& prefix) const {
    if (_len < prefix._len) return false;
    return strncmp(_buf, prefix._buf, prefix._len) == 0;
}

bool String::startsWith(const String& prefix, unsigned int offset) const {
    if (_len < offset + prefix._len) return false;
    return strncmp(_buf + offset, prefix._buf, prefix._len) == 0;
}

bool String::endsWith(const String& suffix) const {
    if (_len < suffix._len) return false;
    return strcmp(_buf + _len - suffix._len, suffix._buf) == 0;
}

// ── Element access ───────────────────────────────────────────────────────────

char String::operator[](unsigned int index) const {
    if (!_buf || index >= _len) return '\0';
    return _buf[index];
}

char& String::operator[](unsigned int index) {
    static char dummy = '\0';
    if (!_buf || index >= _len) return dummy;
    return _buf[index];
}

void String::setCharAt(unsigned int index, char c) {
    if (_buf && index < _len) _buf[index] = c;
}

// ── Searching ────────────────────────────────────────────────────────────────

int String::indexOf(char ch) const { return indexOf(ch, 0); }

int String::indexOf(char ch, unsigned int fromIndex) const {
    if (!_buf || fromIndex >= _len) return -1;
    const char* p = strchr(_buf + fromIndex, ch);
    return p ? static_cast<int>(p - _buf) : -1;
}

int String::indexOf(const String& str) const { return indexOf(str, 0); }

int String::indexOf(const String& str, unsigned int fromIndex) const {
    if (!_buf || !str._buf || fromIndex >= _len) return -1;
    const char* p = strstr(_buf + fromIndex, str._buf);
    return p ? static_cast<int>(p - _buf) : -1;
}

int String::lastIndexOf(char ch) const {
    if (!_buf) return -1;
    const char* p = strrchr(_buf, ch);
    return p ? static_cast<int>(p - _buf) : -1;
}

int String::lastIndexOf(const String& str) const {
    if (!_buf || !str._buf || str._len > _len) return -1;
    int found = -1;
    const char* p = _buf;
    while ((p = strstr(p, str._buf)) != nullptr) {
        found = static_cast<int>(p - _buf);
        ++p;
    }
    return found;
}

// ── Extraction ───────────────────────────────────────────────────────────────

String String::substring(unsigned int beginIndex) const {
    return substring(beginIndex, _len);
}

String String::substring(unsigned int beginIndex, unsigned int endIndex) const {
    if (!_buf || beginIndex >= _len) return String();
    if (endIndex > _len) endIndex = _len;
    if (beginIndex >= endIndex) return String();
    String s;
    s._init(_buf + beginIndex, endIndex - beginIndex);
    return s;
}

// ── Modification ─────────────────────────────────────────────────────────────

void String::toLowerCase() {
    for (unsigned int i = 0; i < _len; ++i)
        _buf[i] = static_cast<char>(tolower(_buf[i]));
}

void String::toUpperCase() {
    for (unsigned int i = 0; i < _len; ++i)
        _buf[i] = static_cast<char>(toupper(_buf[i]));
}

void String::trim() {
    if (!_buf || _len == 0) return;
    unsigned int s = 0;
    while (s < _len && isspace(_buf[s])) ++s;
    unsigned int e = _len;
    while (e > s && isspace(_buf[e - 1])) --e;
    _len = e - s;
    if (s > 0) memmove(_buf, _buf + s, _len);
    _buf[_len] = '\0';
}

void String::replace(char find, char replaceWith) {
    for (unsigned int i = 0; i < _len; ++i)
        if (_buf[i] == find) _buf[i] = replaceWith;
}

void String::replace(const String& find, const String& replaceWith) {
    if (!_buf || !find._buf || find._len == 0) return;
    String result;
    unsigned int pos = 0;
    int idx;
    while ((idx = indexOf(find, pos)) >= 0) {
        result.concat(_buf + pos, static_cast<unsigned int>(idx) - pos);
        result.concat(replaceWith);
        pos = static_cast<unsigned int>(idx) + find._len;
    }
    result.concat(_buf + pos, _len - pos);
    *this = result;
}

void String::remove(unsigned int index) {
    remove(index, _len - index);
}

void String::remove(unsigned int index, unsigned int count) {
    if (!_buf || index >= _len) return;
    if (index + count > _len) count = _len - index;
    memmove(_buf + index, _buf + index + count, _len - index - count);
    _len -= count;
    _buf[_len] = '\0';
}

// ── Conversion ───────────────────────────────────────────────────────────────

long   String::toInt()    const { return _buf ? atol(_buf)  : 0L; }
float  String::toFloat()  const { return _buf ? atof(_buf)  : 0.0f; }
double String::toDouble() const { return _buf ? atof(_buf)  : 0.0; }

// ── reserve ──────────────────────────────────────────────────────────────────

void String::reserve(unsigned int size) {
    _reserve(size);
}
