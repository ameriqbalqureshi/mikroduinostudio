#pragma once
/*
 * MikroDuino Register Access Layer
 * Direct macros for AVR hardware registers.
 * No abstraction — maps 1:1 to AVR instructions.
 */

#include <avr/io.h>
#include <stdint.h>

// --------------------------------------------------------------------------
// Register read / write
// --------------------------------------------------------------------------
#define REG(r)              (r)

// --------------------------------------------------------------------------
// Bit manipulation
// --------------------------------------------------------------------------
#define BITSET(r, b)        ((r) |=  (1U << (b)))
#define BITCLEAR(r, b)      ((r) &= ~(1U << (b)))
#define BITTOGGLE(r, b)     ((r) ^=  (1U << (b)))
#define BITREAD(r, b)       (((r) >> (b)) & 1U)
#define BITWRITE(r, b, v)   ((v) ? BITSET(r, b) : BITCLEAR(r, b))

// --------------------------------------------------------------------------
// Mask operations
// --------------------------------------------------------------------------
#define SETMASK(r, m)       ((r) |=  (m))
#define CLEARMASK(r, m)     ((r) &= ~(m))
#define TOGGLEMASK(r, m)    ((r) ^=  (m))
#define READMASK(r, m)      ((r) &   (m))

// --------------------------------------------------------------------------
// Polling
// --------------------------------------------------------------------------
#define WAIT_BITSET(r, b)   do { } while (!BITREAD(r, b))
#define WAIT_BITCLEAR(r, b) do { } while ( BITREAD(r, b))

// --------------------------------------------------------------------------
// Field insert/extract (for multi-bit fields in a register)
// --------------------------------------------------------------------------
#define FIELD_SET(r, mask, shift, val) \
    ((r) = ((r) & ~(mask)) | (((val) << (shift)) & (mask)))

#define FIELD_GET(r, mask, shift) \
    (((r) & (mask)) >> (shift))

// --------------------------------------------------------------------------
// Compiler hints
// --------------------------------------------------------------------------
#define MD_INLINE   __attribute__((always_inline)) inline
#define MD_NOINLINE __attribute__((noinline))
#define MD_NORETURN __attribute__((noreturn))
#define MD_UNUSED   __attribute__((unused))
#define MD_PACKED   __attribute__((packed))

// --------------------------------------------------------------------------
// ISR guard helpers (save/restore SREG)
// --------------------------------------------------------------------------
#define ATOMIC_BLOCK_START  uint8_t _sreg_save = SREG; __asm__ volatile ("cli" ::: "memory")
#define ATOMIC_BLOCK_END    SREG = _sreg_save; __asm__ volatile ("" ::: "memory")
