#pragma once
/*
 * MikroDuino USART Library
 *
 * Template-based USART driver. Exposes full hardware capability:
 * baud rate, parity, stop bits, data bits, double speed, interrupts.
 * No hidden buffering unless explicitly enabled.
 */

#include "platform.hpp"
#include "registers.hpp"
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <stdint.h>
#include <stdlib.h>   // ltoa

namespace MikroDuino {

// --------------------------------------------------------------------------
// Configuration enums
// --------------------------------------------------------------------------
enum class USARTParity : uint8_t { None = 0, Even = 2, Odd = 3 };
enum class USARTStopBits : uint8_t { One = 0, Two = 1 };
enum class USARTDataBits : uint8_t { Five=0, Six=1, Seven=2, Eight=3, Nine=7 };
enum class USARTMode : uint8_t { TX=1, RX=2, TX_RX=3 };

struct USARTConfig {
    uint32_t     baudRate  = 9600;
    USARTDataBits dataBits = USARTDataBits::Eight;
    USARTParity  parity    = USARTParity::None;
    USARTStopBits stopBits = USARTStopBits::One;
    USARTMode    mode      = USARTMode::TX_RX;
    bool         doubleSpeed = false;
};

// --------------------------------------------------------------------------
// Internal register map — specialised per USART number
// --------------------------------------------------------------------------
namespace _usart_impl {

template<uint8_t N> struct Regs;

template<> struct Regs<0> {
    MD_INLINE static volatile uint8_t&  ucsra() { return UCSR0A; }
    MD_INLINE static volatile uint8_t&  ucsrb() { return UCSR0B; }
    MD_INLINE static volatile uint8_t&  ucsrc() { return UCSR0C; }
    MD_INLINE static volatile uint16_t& ubrr()  { return UBRR0;  }
    MD_INLINE static volatile uint8_t&  udr()   { return UDR0;   }
};

#if MD_USART_COUNT >= 2
template<> struct Regs<1> {
    MD_INLINE static volatile uint8_t&  ucsra() { return UCSR1A; }
    MD_INLINE static volatile uint8_t&  ucsrb() { return UCSR1B; }
    MD_INLINE static volatile uint8_t&  ucsrc() { return UCSR1C; }
    MD_INLINE static volatile uint16_t& ubrr()  { return UBRR1;  }
    MD_INLINE static volatile uint8_t&  udr()   { return UDR1;   }
};
#endif

} // namespace _usart_impl

// --------------------------------------------------------------------------
// USART driver template
// --------------------------------------------------------------------------
template<uint8_t N>
class USARTDriver {
    using R = _usart_impl::Regs<N>;

public:
    void begin(uint32_t baudRate) {
        USARTConfig cfg;
        cfg.baudRate = baudRate;
        begin(cfg);
    }

    void begin(const USARTConfig& cfg) {
        // Compute UBRR
        uint16_t ubrr;
        if (cfg.doubleSpeed) {
            ubrr = static_cast<uint16_t>(F_CPU / (8UL * cfg.baudRate) - 1);
            BITSET(R::ucsra(), U2X0);
        } else {
            ubrr = static_cast<uint16_t>(F_CPU / (16UL * cfg.baudRate) - 1);
            BITCLEAR(R::ucsra(), U2X0);
        }
        R::ubrr() = ubrr;

        // Data bits + parity + stop bits → UCSRC
        uint8_t ucsrc = (1 << UCSZ00); // default 8N1
        switch (cfg.dataBits) {
            case USARTDataBits::Five:  ucsrc = 0; break;
            case USARTDataBits::Six:   ucsrc = (1<<UCSZ00); break;
            case USARTDataBits::Seven: ucsrc = (1<<UCSZ01); break;
            case USARTDataBits::Eight: ucsrc = (1<<UCSZ01)|(1<<UCSZ00); break;
            case USARTDataBits::Nine:
                ucsrc = (1<<UCSZ01)|(1<<UCSZ00);
                BITSET(R::ucsrb(), UCSZ02);
                break;
        }
        if (cfg.parity != USARTParity::None)
            ucsrc |= static_cast<uint8_t>(cfg.parity) << UPM00;
        if (cfg.stopBits == USARTStopBits::Two)
            ucsrc |= (1 << USBS0);
        R::ucsrc() = ucsrc;

        // Enable TX/RX
        uint8_t ucsrb = R::ucsrb() & 0xE7u; // clear RXEN0/TXEN0
        if (static_cast<uint8_t>(cfg.mode) & 0x01u) ucsrb |= (1<<TXEN0);
        if (static_cast<uint8_t>(cfg.mode) & 0x02u) ucsrb |= (1<<RXEN0);
        R::ucsrb() = ucsrb;
    }

    void end() {
        BITCLEAR(R::ucsrb(), RXEN0);
        BITCLEAR(R::ucsrb(), TXEN0);
        BITCLEAR(R::ucsrb(), RXCIE0);
        BITCLEAR(R::ucsrb(), TXCIE0);
    }

    // --- TX ---
    void write(uint8_t data) {
        WAIT_BITSET(R::ucsra(), UDRE0);
        R::udr() = data;
    }

    void write(const char* str) {
        while (*str) write(static_cast<uint8_t>(*str++));
    }

    void writeLine(const char* str) {
        write(str);
        write('\r');
        write('\n');
    }

    void writeInt(int32_t value, uint8_t base = 10) {
        char buf[12];
        ltoa(value, buf, base);
        write(buf);
    }

    // Write a string stored in flash (PROGMEM) — avoids copying literals to SRAM.
    // Usage:  USART0.write_P(PSTR("Hello"));
    void write_P(PGM_P str) {
        uint8_t c;
        while ((c = pgm_read_byte(str++)) != 0) write(c);
    }

    void writeLine_P(PGM_P str) {
        write_P(str);
        write('\r');
        write('\n');
    }

    void writeHex(uint32_t value) {
        write("0x");
        for (int8_t i = 28; i >= 0; i -= 4) {
            uint8_t nibble = (value >> i) & 0x0Fu;
            write(static_cast<uint8_t>(nibble < 10 ? '0' + nibble : 'A' + nibble - 10));
        }
    }

    // Fixed-decimal float, e.g. 3.14159 with decimals=2 -> "3.14".
    // Pulls in soft-float; #define MD_NO_FLOAT to exclude this method.
#ifndef MD_NO_FLOAT
    void writeFloat(float val, uint8_t decimals = 2) {
        if (val < 0.0f) { write('-'); val = -val; }
        writeInt(static_cast<int32_t>(val));
        if (decimals > 0) {
            write('.');
            float frac = val - static_cast<float>(static_cast<int32_t>(val));
            for (uint8_t i = 0; i < decimals; ++i) {
                frac *= 10.0f;
                uint8_t digit = static_cast<uint8_t>(frac);
                write(static_cast<char>('0' + digit));
                frac -= static_cast<float>(digit);
            }
        }
    }
#endif

    // --- RX ---
    bool available() const {
        return BITREAD(R::ucsra(), RXC0) != 0;
    }

    uint8_t read() {
        WAIT_BITSET(R::ucsra(), RXC0);
        return R::udr();
    }

    uint8_t readNonBlock(bool& dataReady) {
        dataReady = available();
        return dataReady ? R::udr() : 0;
    }

    // Blocking read of a line into buffer, stopping at '\r' or '\n' or when
    // (size - 1) characters have been stored. Always null-terminates.
    // Line-ending characters are consumed but not stored in the buffer.
    // Usage:  char buf[32]; USART0.readLine(buf, sizeof(buf));
    // Returns the number of characters stored (excluding the terminator).
    uint8_t readLine(char* buffer, uint8_t size) {
        uint8_t count = 0;
        if (size == 0) return 0;
        while (count < static_cast<uint8_t>(size - 1)) {
            uint8_t c = read();
            if (c == '\r' || c == '\n') break;
            buffer[count++] = static_cast<char>(c);
        }
        buffer[count] = '\0';
        return count;
    }

    bool rxError() const {
        return BITREAD(R::ucsra(), FE0) || BITREAD(R::ucsra(), DOR0) || BITREAD(R::ucsra(), UPE0);
    }

    bool txReady() const {
        return BITREAD(R::ucsra(), UDRE0) != 0;
    }

    void flush() {
        WAIT_BITSET(R::ucsra(), TXC0);
    }

    // --- Interrupts ---
    void enableRxInterrupt()  { BITSET(R::ucsrb(), RXCIE0); }
    void disableRxInterrupt() { BITCLEAR(R::ucsrb(), RXCIE0); }
    void enableTxInterrupt()  { BITSET(R::ucsrb(), TXCIE0); }
    void disableTxInterrupt() { BITCLEAR(R::ucsrb(), TXCIE0); }
    void enableUDREInterrupt(){ BITSET(R::ucsrb(), UDRIE0); }
    void disableUDREInterrupt(){ BITCLEAR(R::ucsrb(), UDRIE0); }
};

// --------------------------------------------------------------------------
// Instances
// --------------------------------------------------------------------------
static USARTDriver<0> USART0;
#if MD_USART_COUNT >= 2
static USARTDriver<1> USART1;
#endif

} // namespace MikroDuino
