#ifndef __LIB_IO_H
#define __LIB_IO_H
#include "stdint.h"

// machine mode
// b -- QImode name for reg, i.e. [a-d]l
// w -- HImode name for reg, i.e. [a-d]x

// write port a byte
static inline void outb(uint16_t port, uint8_t data) {
    // a -> al/ax/eax, N -> 0-255, d -> dx
   asm volatile ( "outb %b0, %w1" : : "a" (data), "Nd" (port));
}

// write port many words from addr
static inline void outsw(uint16_t port, const void* addr, uint32_t word_cnt) {
    // outsw -> 2B from ds:esi to port
   asm volatile ("cld; rep outsw" : "+S" (addr), "+c" (word_cnt) : "d" (port));
}

// read port a byte
static inline uint8_t inb(uint16_t port) {
   uint8_t data;
   asm volatile ("inb %w1, %b0" : "=a" (data) : "Nd" (port));
   return data;
}

// read from port to mem addr
static inline void insw(uint16_t port, void* addr, uint32_t word_cnt) {
    // insw -> 2B from port to es:edi
   asm volatile ("cld; rep insw" : "+D" (addr), "+c" (word_cnt) : "d" (port) : "memory");
}

#endif
