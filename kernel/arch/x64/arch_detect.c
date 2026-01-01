/*
 * Zirvium Architecture Detection
 * Supports: x86 (32/64), ARM (32/64), RISC-V, PPC, Apple Silicon, 
 *           Xtensa, Itanium, SPARC, and legacy architectures
 */

#include <kernel/types.h>
#include <kernel/kernel.h>

typedef enum {
    ARCH_UNKNOWN = 0,
    /* x86 Family */
    ARCH_I386,
    ARCH_I486,
    ARCH_I586,          /* Pentium */
    ARCH_I686,          /* Pentium Pro/II/III */
    ARCH_COPPERMINE,    /* Pentium III */
    ARCH_PRESCOTT,      /* Pentium 4 */
    ARCH_DOTHAN,        /* Pentium M */
    ARCH_YONAH,         /* Core Duo */
    ARCH_CORE_SOLO,
    ARCH_CORE_DUO,
    ARCH_CORE_QUAD,
    ARCH_NEHALEM,       /* Core i7 1st gen */
    ARCH_X86_64,        /* Modern x64 */
    
    /* ARM Family */
    ARCH_ARM32,
    ARCH_ARM64,
    ARCH_APPLE_M1,
    ARCH_APPLE_M2,
    ARCH_APPLE_M3,
    ARCH_APPLE_M4,
    ARCH_SNAPDRAGON_865,
    ARCH_SNAPDRAGON_888,
    ARCH_SNAPDRAGON_8_GEN1,
    ARCH_SNAPDRAGON_8_GEN2,
    ARCH_SNAPDRAGON_8_GEN3,
    
    /* RISC-V */
    ARCH_RISCV32,
    ARCH_RISCV64,
    
    /* PowerPC */
    ARCH_PPC32,
    ARCH_PPC64,
    ARCH_PPC64LE,
    
    /* Other Architectures */
    ARCH_XTENSA,
    ARCH_ITANIUM,       /* IA-64 */
    ARCH_SPARC32,
    ARCH_SPARC64,
    ARCH_ARS,           /* Custom arch */
    
    ARCH_MAX
} arch_type_t;

static const char *arch_names[] = {
    [ARCH_UNKNOWN] = "Unknown",
    [ARCH_I386] = "Intel 80386",
    [ARCH_I486] = "Intel 80486",
    [ARCH_I586] = "Intel Pentium",
    [ARCH_I686] = "Intel Pentium Pro/II/III",
    [ARCH_COPPERMINE] = "Intel Pentium III Coppermine",
    [ARCH_PRESCOTT] = "Intel Pentium 4 Prescott",
    [ARCH_DOTHAN] = "Intel Pentium M Dothan",
    [ARCH_YONAH] = "Intel Core Yonah",
    [ARCH_CORE_SOLO] = "Intel Core Solo",
    [ARCH_CORE_DUO] = "Intel Core Duo",
    [ARCH_CORE_QUAD] = "Intel Core Quad",
    [ARCH_NEHALEM] = "Intel Core i7 Nehalem",
    [ARCH_X86_64] = "x86-64 (AMD64/Intel 64)",
    [ARCH_ARM32] = "ARM 32-bit",
    [ARCH_ARM64] = "ARM 64-bit (AArch64)",
    [ARCH_APPLE_M1] = "Apple M1",
    [ARCH_APPLE_M2] = "Apple M2",
    [ARCH_APPLE_M3] = "Apple M3",
    [ARCH_APPLE_M4] = "Apple M4",
    [ARCH_SNAPDRAGON_865] = "Qualcomm Snapdragon 865",
    [ARCH_SNAPDRAGON_888] = "Qualcomm Snapdragon 888",
    [ARCH_SNAPDRAGON_8_GEN1] = "Qualcomm Snapdragon 8 Gen 1",
    [ARCH_SNAPDRAGON_8_GEN2] = "Qualcomm Snapdragon 8 Gen 2",
    [ARCH_SNAPDRAGON_8_GEN3] = "Qualcomm Snapdragon 8 Gen 3",
    [ARCH_RISCV32] = "RISC-V 32-bit",
    [ARCH_RISCV64] = "RISC-V 64-bit",
    [ARCH_PPC32] = "PowerPC 32-bit",
    [ARCH_PPC64] = "PowerPC 64-bit",
    [ARCH_PPC64LE] = "PowerPC 64-bit Little Endian",
    [ARCH_XTENSA] = "Xtensa",
    [ARCH_ITANIUM] = "Intel Itanium (IA-64)",
    [ARCH_SPARC32] = "SPARC 32-bit",
    [ARCH_SPARC64] = "SPARC 64-bit",
    [ARCH_ARS] = "ARS Architecture",
};

static arch_type_t current_arch = ARCH_UNKNOWN;

arch_type_t arch_detect(void) {
#if defined(__x86_64__) || defined(_M_X64) || defined(_M_AMD64)
    current_arch = ARCH_X86_64;
#elif defined(__i386__) || defined(_M_IX86)
    #if defined(__i686__)
        current_arch = ARCH_I686;
    #elif defined(__i586__)
        current_arch = ARCH_I586;
    #elif defined(__i486__)
        current_arch = ARCH_I486;
    #else
        current_arch = ARCH_I386;
    #endif
#elif defined(__aarch64__) || defined(_M_ARM64)
    current_arch = ARCH_ARM64;
#elif defined(__arm__) || defined(_M_ARM)
    current_arch = ARCH_ARM32;
#elif defined(__riscv)
    #if __riscv_xlen == 64
        current_arch = ARCH_RISCV64;
    #else
        current_arch = ARCH_RISCV32;
    #endif
#elif defined(__powerpc64__) || defined(__ppc64__)
    #ifdef __LITTLE_ENDIAN__
        current_arch = ARCH_PPC64LE;
    #else
        current_arch = ARCH_PPC64;
    #endif
#elif defined(__powerpc__) || defined(__ppc__)
    current_arch = ARCH_PPC32;
#elif defined(__xtensa__)
    current_arch = ARCH_XTENSA;
#elif defined(__ia64__) || defined(_M_IA64)
    current_arch = ARCH_ITANIUM;
#elif defined(__sparc__)
    #ifdef __arch64__
        current_arch = ARCH_SPARC64;
    #else
        current_arch = ARCH_SPARC32;
    #endif
#else
    current_arch = ARCH_UNKNOWN;
#endif
    
    return current_arch;
}

const char *arch_get_name(void) {
    if (current_arch < ARCH_MAX) {
        return arch_names[current_arch];
    }
    return "Unknown Architecture";
}

void arch_print_info(void) {
    arch_detect();
    kprintf("Architecture: %s\n", arch_get_name());
    
    /* Print architecture capabilities */
    kprintf("  Word size: ");
#if defined(__LP64__) || defined(_WIN64)
    kprintf("64-bit\n");
#else
    kprintf("32-bit\n");
#endif
    
    kprintf("  Endianness: ");
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    kprintf("Little Endian\n");
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    kprintf("Big Endian\n");
#else
    kprintf("Unknown\n");
#endif
}
