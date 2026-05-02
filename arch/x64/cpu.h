/* arch/x64/cpu.h
 * Zirvium Kernel — x86-64 CPU primitives
 */
#ifndef ZIRVIUM_ARCH_X64_CPU_H
#define ZIRVIUM_ARCH_X64_CPU_H

#include <stdint.h>

/* ── Port I/O ──────────────────────────────────────────────────────────────── */
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port) : "memory");
}
static inline uint8_t inb(uint16_t port) {
    uint8_t val;
    __asm__ volatile("inb %1, %0" : "=a"(val) : "Nd"(port) : "memory");
    return val;
}
static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile("outw %0, %1" : : "a"(val), "Nd"(port) : "memory");
}
static inline uint16_t inw(uint16_t port) {
    uint16_t val;
    __asm__ volatile("inw %1, %0" : "=a"(val) : "Nd"(port) : "memory");
    return val;
}
static inline void outl(uint16_t port, uint32_t val) {
    __asm__ volatile("outl %0, %1" : : "a"(val), "Nd"(port) : "memory");
}
static inline uint32_t inl(uint16_t port) {
    uint32_t val;
    __asm__ volatile("inl %1, %0" : "=a"(val) : "Nd"(port) : "memory");
    return val;
}

/* ── CPU state ─────────────────────────────────────────────────────────────── */
static inline void cpu_halt(void) {
    __asm__ volatile("hlt");
}
static inline void cpu_cli(void) {
    __asm__ volatile("cli" ::: "memory");
}
static inline void cpu_sti(void) {
    __asm__ volatile("sti" ::: "memory");
}
static inline void cpu_pause(void) {
    __asm__ volatile("pause");
}

/* ── Model-Specific Registers ─────────────────────────────────────────────── */
static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t lo, hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}
static inline void wrmsr(uint32_t msr, uint64_t val) {
    __asm__ volatile("wrmsr" : : "c"(msr), "a"((uint32_t)val),
                                  "d"((uint32_t)(val >> 32)));
}

/* ── Control Registers ─────────────────────────────────────────────────────── */
static inline uint64_t read_cr0(void) {
    uint64_t val;
    __asm__ volatile("mov %%cr0, %0" : "=r"(val));
    return val;
}
static inline uint64_t read_cr2(void) {
    uint64_t val;
    __asm__ volatile("mov %%cr2, %0" : "=r"(val));
    return val;
}
static inline uint64_t read_cr3(void) {
    uint64_t val;
    __asm__ volatile("mov %%cr3, %0" : "=r"(val));
    return val;
}
static inline void write_cr3(uint64_t val) {
    __asm__ volatile("mov %0, %%cr3" : : "r"(val) : "memory");
}
static inline uint64_t read_cr4(void) {
    uint64_t val;
    __asm__ volatile("mov %%cr4, %0" : "=r"(val));
    return val;
}

/* ── Timestamp Counter ─────────────────────────────────────────────────────── */
static inline uint64_t rdtsc(void) {
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

#endif /* ZIRVIUM_ARCH_X64_CPU_H */
