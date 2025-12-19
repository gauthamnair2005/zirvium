/*
 * Zirvium CPU Feature Detection
 * Supports: MMX, SSE, AVX, FMA, AES-NI, HT, POPCNT, and more
 */

#include <kernel/types.h>
#include <kernel/kernel.h>

/* CPU Feature Flags */
#define CPU_FEATURE_FPU         (1ULL << 0)   /* x87 FPU */
#define CPU_FEATURE_VME         (1ULL << 1)   /* Virtual 8086 Mode Extensions */
#define CPU_FEATURE_DE          (1ULL << 2)   /* Debugging Extensions */
#define CPU_FEATURE_PSE         (1ULL << 3)   /* Page Size Extension */
#define CPU_FEATURE_TSC         (1ULL << 4)   /* Time Stamp Counter */
#define CPU_FEATURE_MSR         (1ULL << 5)   /* Model Specific Registers */
#define CPU_FEATURE_PAE         (1ULL << 6)   /* Physical Address Extension */
#define CPU_FEATURE_MCE         (1ULL << 7)   /* Machine Check Exception */
#define CPU_FEATURE_CX8         (1ULL << 8)   /* CMPXCHG8 instruction */
#define CPU_FEATURE_APIC        (1ULL << 9)   /* APIC on-chip */
#define CPU_FEATURE_SEP         (1ULL << 10)  /* SYSENTER/SYSEXIT */
#define CPU_FEATURE_MTRR        (1ULL << 11)  /* Memory Type Range Registers */
#define CPU_FEATURE_PGE         (1ULL << 12)  /* Page Global Enable */
#define CPU_FEATURE_MCA         (1ULL << 13)  /* Machine Check Architecture */
#define CPU_FEATURE_CMOV        (1ULL << 14)  /* Conditional Move */
#define CPU_FEATURE_PAT         (1ULL << 15)  /* Page Attribute Table */
#define CPU_FEATURE_PSE36       (1ULL << 16)  /* 36-bit PSE */
#define CPU_FEATURE_CLFLUSH     (1ULL << 17)  /* CLFLUSH instruction */
#define CPU_FEATURE_MMX         (1ULL << 18)  /* MMX Technology */
#define CPU_FEATURE_FXSR        (1ULL << 19)  /* FXSAVE/FXRSTOR */
#define CPU_FEATURE_SSE         (1ULL << 20)  /* SSE instructions */
#define CPU_FEATURE_SSE2        (1ULL << 21)  /* SSE2 instructions */
#define CPU_FEATURE_SS          (1ULL << 22)  /* Self Snoop */
#define CPU_FEATURE_HT          (1ULL << 23)  /* Hyper-Threading */
#define CPU_FEATURE_TM          (1ULL << 24)  /* Thermal Monitor */
#define CPU_FEATURE_PBE         (1ULL << 25)  /* Pending Break Enable */

/* Extended Features (ECX from CPUID EAX=1) */
#define CPU_FEATURE_SSE3        (1ULL << 26)  /* SSE3 instructions */
#define CPU_FEATURE_PCLMULQDQ   (1ULL << 27)  /* PCLMULQDQ instruction */
#define CPU_FEATURE_MONITOR     (1ULL << 28)  /* MONITOR/MWAIT */
#define CPU_FEATURE_SSSE3       (1ULL << 29)  /* Supplemental SSE3 */
#define CPU_FEATURE_FMA         (1ULL << 30)  /* Fused Multiply-Add */
#define CPU_FEATURE_CMPXCHG16B  (1ULL << 31)  /* CMPXCHG16B instruction */
#define CPU_FEATURE_SSE41       (1ULL << 32)  /* SSE4.1 instructions */
#define CPU_FEATURE_SSE42       (1ULL << 33)  /* SSE4.2 instructions */
#define CPU_FEATURE_X2APIC      (1ULL << 34)  /* x2APIC support */
#define CPU_FEATURE_MOVBE       (1ULL << 35)  /* MOVBE instruction */
#define CPU_FEATURE_POPCNT      (1ULL << 36)  /* POPCNT instruction */
#define CPU_FEATURE_AES         (1ULL << 37)  /* AES-NI instructions */
#define CPU_FEATURE_XSAVE       (1ULL << 38)  /* XSAVE/XRSTOR */
#define CPU_FEATURE_OSXSAVE     (1ULL << 39)  /* OS has enabled XSAVE */
#define CPU_FEATURE_AVX         (1ULL << 40)  /* AVX instructions */
#define CPU_FEATURE_F16C        (1ULL << 41)  /* Half-precision convert */
#define CPU_FEATURE_RDRAND      (1ULL << 42)  /* RDRAND instruction */

/* Extended Features (EBX from CPUID EAX=7, ECX=0) */
#define CPU_FEATURE_FSGSBASE    (1ULL << 43)  /* FS/GS base access */
#define CPU_FEATURE_BMI1        (1ULL << 44)  /* Bit Manipulation Instruction Set 1 */
#define CPU_FEATURE_AVX2        (1ULL << 45)  /* AVX2 instructions */
#define CPU_FEATURE_BMI2        (1ULL << 46)  /* Bit Manipulation Instruction Set 2 */
#define CPU_FEATURE_ERMS        (1ULL << 47)  /* Enhanced REP MOVSB/STOSB */
#define CPU_FEATURE_INVPCID     (1ULL << 48)  /* INVPCID instruction */
#define CPU_FEATURE_RTM         (1ULL << 49)  /* Transactional Synchronization Extensions */
#define CPU_FEATURE_MPX         (1ULL << 50)  /* Memory Protection Extensions */
#define CPU_FEATURE_AVX512F     (1ULL << 51)  /* AVX-512 Foundation */
#define CPU_FEATURE_AVX512DQ    (1ULL << 52)  /* AVX-512 DQ */
#define CPU_FEATURE_RDSEED      (1ULL << 53)  /* RDSEED instruction */
#define CPU_FEATURE_ADX         (1ULL << 54)  /* Multi-Precision Add-Carry */
#define CPU_FEATURE_AVX512IFMA  (1ULL << 55)  /* AVX-512 IFMA */
#define CPU_FEATURE_AVX512PF    (1ULL << 56)  /* AVX-512 PF */
#define CPU_FEATURE_AVX512ER    (1ULL << 57)  /* AVX-512 ER */
#define CPU_FEATURE_AVX512CD    (1ULL << 58)  /* AVX-512 CD */
#define CPU_FEATURE_SHA         (1ULL << 59)  /* SHA extensions */
#define CPU_FEATURE_AVX512BW    (1ULL << 60)  /* AVX-512 BW */
#define CPU_FEATURE_AVX512VL    (1ULL << 61)  /* AVX-512 VL */

/* Additional Modern Features */
#define CPU_FEATURE_AVX512VNNI  (1ULL << 62)  /* AVX-512 Vector Neural Network Instructions */
#define CPU_FEATURE_AMX         (1ULL << 63)  /* Advanced Matrix Extensions */

typedef struct {
    uint64_t features_low;
    uint64_t features_high;
    
    /* CPU identification */
    uint32_t vendor_id[4];
    char vendor_string[13];
    uint32_t family;
    uint32_t model;
    uint32_t stepping;
    uint32_t cache_line_size;
    uint32_t logical_processors;
    
    /* Brand string */
    char brand_string[49];
    
    /* Cache information */
    uint32_t l1_cache_size;
    uint32_t l2_cache_size;
    uint32_t l3_cache_size;
    
    /* Frequency */
    uint32_t base_frequency_mhz;
    uint32_t max_turbo_frequency_mhz;
} cpu_info_t;

static cpu_info_t cpu_info;

#if defined(__i386__) || defined(__x86_64__)

static inline void cpuid(uint32_t leaf, uint32_t subleaf, 
                         uint32_t *eax, uint32_t *ebx, 
                         uint32_t *ecx, uint32_t *edx) {
    #ifdef __x86_64__
    asm volatile("cpuid"
        : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
        : "a"(leaf), "c"(subleaf));
    #else
    /* 32-bit: save ebx for PIC */
    asm volatile("mov %%ebx, %%esi\n\t"
                 "cpuid\n\t"
                 "xchg %%ebx, %%esi"
        : "=a"(*eax), "=S"(*ebx), "=c"(*ecx), "=d"(*edx)
        : "a"(leaf), "c"(subleaf));
    #endif
}

static void detect_cpu_vendor(void) {
    uint32_t eax, ebx, ecx, edx;
    
    cpuid(0, 0, &eax, &ebx, &ecx, &edx);
    
    cpu_info.vendor_id[0] = ebx;
    cpu_info.vendor_id[1] = edx;
    cpu_info.vendor_id[2] = ecx;
    cpu_info.vendor_id[3] = 0;
    
    /* Build vendor string */
    for (int i = 0; i < 4; i++) {
        cpu_info.vendor_string[i] = (ebx >> (i * 8)) & 0xFF;
        cpu_info.vendor_string[i + 4] = (edx >> (i * 8)) & 0xFF;
        cpu_info.vendor_string[i + 8] = (ecx >> (i * 8)) & 0xFF;
    }
    cpu_info.vendor_string[12] = '\0';
}

static void detect_cpu_features(void) {
    uint32_t eax, ebx, ecx, edx;
    
    /* Get basic features */
    cpuid(1, 0, &eax, &ebx, &ecx, &edx);
    
    /* Family, Model, Stepping */
    cpu_info.stepping = eax & 0xF;
    cpu_info.model = (eax >> 4) & 0xF;
    cpu_info.family = (eax >> 8) & 0xF;
    
    /* Extended family and model */
    if (cpu_info.family == 0xF) {
        cpu_info.family += (eax >> 20) & 0xFF;
    }
    if (cpu_info.family == 0x6 || cpu_info.family == 0xF) {
        cpu_info.model += ((eax >> 16) & 0xF) << 4;
    }
    
    /* Cache line size */
    cpu_info.cache_line_size = ((ebx >> 8) & 0xFF) * 8;
    cpu_info.logical_processors = (ebx >> 16) & 0xFF;
    
    /* Parse EDX features */
    if (edx & (1 << 0))  cpu_info.features_low |= CPU_FEATURE_FPU;
    if (edx & (1 << 4))  cpu_info.features_low |= CPU_FEATURE_TSC;
    if (edx & (1 << 5))  cpu_info.features_low |= CPU_FEATURE_MSR;
    if (edx & (1 << 6))  cpu_info.features_low |= CPU_FEATURE_PAE;
    if (edx & (1 << 8))  cpu_info.features_low |= CPU_FEATURE_CX8;
    if (edx & (1 << 9))  cpu_info.features_low |= CPU_FEATURE_APIC;
    if (edx & (1 << 11)) cpu_info.features_low |= CPU_FEATURE_SEP;
    if (edx & (1 << 15)) cpu_info.features_low |= CPU_FEATURE_CMOV;
    if (edx & (1 << 19)) cpu_info.features_low |= CPU_FEATURE_CLFLUSH;
    if (edx & (1 << 23)) cpu_info.features_low |= CPU_FEATURE_MMX;
    if (edx & (1 << 24)) cpu_info.features_low |= CPU_FEATURE_FXSR;
    if (edx & (1 << 25)) cpu_info.features_low |= CPU_FEATURE_SSE;
    if (edx & (1 << 26)) cpu_info.features_low |= CPU_FEATURE_SSE2;
    if (edx & (1 << 28)) cpu_info.features_low |= CPU_FEATURE_HT;
    
    /* Parse ECX features */
    if (ecx & (1 << 0))  cpu_info.features_low |= CPU_FEATURE_SSE3;
    if (ecx & (1 << 1))  cpu_info.features_low |= CPU_FEATURE_PCLMULQDQ;
    if (ecx & (1 << 9))  cpu_info.features_low |= CPU_FEATURE_SSSE3;
    if (ecx & (1 << 12)) cpu_info.features_low |= CPU_FEATURE_FMA;
    if (ecx & (1 << 13)) cpu_info.features_low |= CPU_FEATURE_CMPXCHG16B;
    if (ecx & (1 << 19)) cpu_info.features_low |= CPU_FEATURE_SSE41;
    if (ecx & (1 << 20)) cpu_info.features_low |= CPU_FEATURE_SSE42;
    if (ecx & (1 << 22)) cpu_info.features_low |= CPU_FEATURE_MOVBE;
    if (ecx & (1 << 23)) cpu_info.features_low |= CPU_FEATURE_POPCNT;
    if (ecx & (1 << 25)) cpu_info.features_low |= CPU_FEATURE_AES;
    if (ecx & (1 << 26)) cpu_info.features_low |= CPU_FEATURE_XSAVE;
    if (ecx & (1 << 27)) cpu_info.features_low |= CPU_FEATURE_OSXSAVE;
    if (ecx & (1 << 28)) cpu_info.features_low |= CPU_FEATURE_AVX;
    if (ecx & (1 << 29)) cpu_info.features_low |= CPU_FEATURE_F16C;
    if (ecx & (1 << 30)) cpu_info.features_low |= CPU_FEATURE_RDRAND;
    
    /* Extended features (leaf 7) */
    cpuid(7, 0, &eax, &ebx, &ecx, &edx);
    
    if (ebx & (1 << 0))  cpu_info.features_low |= CPU_FEATURE_FSGSBASE;
    if (ebx & (1 << 3))  cpu_info.features_low |= CPU_FEATURE_BMI1;
    if (ebx & (1 << 5))  cpu_info.features_low |= CPU_FEATURE_AVX2;
    if (ebx & (1 << 8))  cpu_info.features_low |= CPU_FEATURE_BMI2;
    if (ebx & (1 << 11)) cpu_info.features_low |= CPU_FEATURE_RTM;
    if (ebx & (1 << 14)) cpu_info.features_low |= CPU_FEATURE_MPX;
    if (ebx & (1 << 16)) cpu_info.features_low |= CPU_FEATURE_AVX512F;
    if (ebx & (1 << 17)) cpu_info.features_low |= CPU_FEATURE_AVX512DQ;
    if (ebx & (1 << 18)) cpu_info.features_low |= CPU_FEATURE_RDSEED;
    if (ebx & (1 << 19)) cpu_info.features_low |= CPU_FEATURE_ADX;
    if (ebx & (1 << 21)) cpu_info.features_low |= CPU_FEATURE_AVX512IFMA;
    if (ebx & (1 << 26)) cpu_info.features_low |= CPU_FEATURE_AVX512PF;
    if (ebx & (1 << 27)) cpu_info.features_low |= CPU_FEATURE_AVX512ER;
    if (ebx & (1 << 28)) cpu_info.features_low |= CPU_FEATURE_AVX512CD;
    if (ebx & (1 << 29)) cpu_info.features_low |= CPU_FEATURE_SHA;
    if (ebx & (1 << 30)) cpu_info.features_low |= CPU_FEATURE_AVX512BW;
    if (ebx & (1 << 31)) cpu_info.features_low |= CPU_FEATURE_AVX512VL;
    
    if (ecx & (1 << 11)) cpu_info.features_high |= 1; /* AVX512_VNNI */
    if (edx & (1 << 24)) cpu_info.features_high |= 2; /* AMX-TILE */
}

static void detect_brand_string(void) {
    uint32_t eax, ebx, ecx, edx;
    uint32_t *brand = (uint32_t*)cpu_info.brand_string;
    
    cpuid(0x80000000, 0, &eax, &ebx, &ecx, &edx);
    
    if (eax >= 0x80000004) {
        cpuid(0x80000002, 0, &brand[0], &brand[1], &brand[2], &brand[3]);
        cpuid(0x80000003, 0, &brand[4], &brand[5], &brand[6], &brand[7]);
        cpuid(0x80000004, 0, &brand[8], &brand[9], &brand[10], &brand[11]);
        cpu_info.brand_string[48] = '\0';
    } else {
        cpu_info.brand_string[0] = '\0';
    }
}

#else
/* Non-x86 architectures */
static void detect_cpu_vendor(void) {
    cpu_info.vendor_string[0] = '\0';
}
static void detect_cpu_features(void) {}
static void detect_brand_string(void) {
    cpu_info.brand_string[0] = '\0';
}
#endif

void cpu_feature_init(void) {
    /* Initialize structure */
    cpu_info.features_low = 0;
    cpu_info.features_high = 0;
    
    detect_cpu_vendor();
    detect_cpu_features();
    detect_brand_string();
}

void cpu_print_info(void) {
    kprintf("\n=== CPU Feature Detection ===\n");
    
    if (cpu_info.vendor_string[0]) {
        kprintf("Vendor: %s\n", cpu_info.vendor_string);
    }
    
    if (cpu_info.brand_string[0]) {
        kprintf("Brand: %s\n", cpu_info.brand_string);
    }
    
    kprintf("Family: %u, Model: %u, Stepping: %u\n",
            cpu_info.family, cpu_info.model, cpu_info.stepping);
    
    if (cpu_info.logical_processors > 0) {
        kprintf("Logical CPUs: %u\n", cpu_info.logical_processors);
    }
    
    kprintf("\nInstruction Set Extensions:\n");
    
    /* x87 and MMX */
    if (cpu_info.features_low & CPU_FEATURE_FPU) {
        kprintf("  ✓ x87 FPU (Floating Point Unit)\n");
    }
    if (cpu_info.features_low & CPU_FEATURE_MMX) {
        kprintf("  ✓ MMX (MultiMedia eXtensions)\n");
    }
    
    /* SSE family */
    if (cpu_info.features_low & CPU_FEATURE_SSE) {
        kprintf("  ✓ SSE (Streaming SIMD Extensions)\n");
    }
    if (cpu_info.features_low & CPU_FEATURE_SSE2) {
        kprintf("  ✓ SSE2\n");
    }
    if (cpu_info.features_low & CPU_FEATURE_SSE3) {
        kprintf("  ✓ SSE3\n");
    }
    if (cpu_info.features_low & CPU_FEATURE_SSSE3) {
        kprintf("  ✓ SSSE3 (Supplemental SSE3)\n");
    }
    if (cpu_info.features_low & CPU_FEATURE_SSE41) {
        kprintf("  ✓ SSE4.1\n");
    }
    if (cpu_info.features_low & CPU_FEATURE_SSE42) {
        kprintf("  ✓ SSE4.2\n");
    }
    
    /* AVX family */
    if (cpu_info.features_low & CPU_FEATURE_AVX) {
        kprintf("  ✓ AVX (Advanced Vector Extensions)\n");
    }
    if (cpu_info.features_low & CPU_FEATURE_AVX2) {
        kprintf("  ✓ AVX2\n");
    }
    if (cpu_info.features_low & CPU_FEATURE_AVX512F) {
        kprintf("  ✓ AVX-512 Foundation\n");
    }
    if (cpu_info.features_low & CPU_FEATURE_AVX512DQ) {
        kprintf("  ✓ AVX-512 DQ (Doubleword and Quadword)\n");
    }
    if (cpu_info.features_low & CPU_FEATURE_AVX512BW) {
        kprintf("  ✓ AVX-512 BW (Byte and Word)\n");
    }
    if (cpu_info.features_low & CPU_FEATURE_AVX512VL) {
        kprintf("  ✓ AVX-512 VL (Vector Length)\n");
    }
    
    /* Crypto and special */
    if (cpu_info.features_low & CPU_FEATURE_AES) {
        kprintf("  ✓ AES-NI (AES New Instructions)\n");
    }
    if (cpu_info.features_low & CPU_FEATURE_SHA) {
        kprintf("  ✓ SHA Extensions\n");
    }
    if (cpu_info.features_low & CPU_FEATURE_PCLMULQDQ) {
        kprintf("  ✓ PCLMULQDQ (Carry-less Multiplication)\n");
    }
    
    /* Math and bit manipulation */
    if (cpu_info.features_low & CPU_FEATURE_FMA) {
        kprintf("  ✓ FMA (Fused Multiply-Add)\n");
    }
    if (cpu_info.features_low & CPU_FEATURE_POPCNT) {
        kprintf("  ✓ POPCNT (Population Count)\n");
    }
    if (cpu_info.features_low & CPU_FEATURE_BMI1) {
        kprintf("  ✓ BMI1 (Bit Manipulation Instructions Set 1)\n");
    }
    if (cpu_info.features_low & CPU_FEATURE_BMI2) {
        kprintf("  ✓ BMI2 (Bit Manipulation Instructions Set 2)\n");
    }
    if (cpu_info.features_low & CPU_FEATURE_ADX) {
        kprintf("  ✓ ADX (Multi-Precision Add-Carry)\n");
    }
    
    /* Threading and virtualization */
    if (cpu_info.features_low & CPU_FEATURE_HT) {
        kprintf("  ✓ HT (Hyper-Threading Technology)\n");
    }
    
    /* Random number generation */
    if (cpu_info.features_low & CPU_FEATURE_RDRAND) {
        kprintf("  ✓ RDRAND (Hardware RNG)\n");
    }
    if (cpu_info.features_low & CPU_FEATURE_RDSEED) {
        kprintf("  ✓ RDSEED (Hardware Seed RNG)\n");
    }
    
    /* Transactional memory */
    if (cpu_info.features_low & CPU_FEATURE_RTM) {
        kprintf("  ✓ RTM (Restricted Transactional Memory)\n");
    }
    
    /* Advanced features */
    if (cpu_info.features_high & 2) {
        kprintf("  ✓ AMX (Advanced Matrix Extensions)\n");
    }
    
    kprintf("\n");
}

int cpu_has_feature(uint64_t feature) {
    return (cpu_info.features_low & feature) != 0 ||
           (cpu_info.features_high & (feature & 0xFFFFFFFFULL)) != 0;
}

const char* cpu_get_vendor(void) {
    return cpu_info.vendor_string;
}

const char* cpu_get_brand(void) {
    return cpu_info.brand_string;
}
