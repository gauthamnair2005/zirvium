/* drivers/compat/linux_compat.h
 * Zirvium Kernel — Linux driver porting compatibility layer
 *
 * This header maps the Linux kernel's most-used API surface to Zirvium
 * equivalents so that Linux driver code can be ported with minimal changes.
 *
 * Design philosophy
 * -----------------
 * - Only the subset of Linux APIs used by the ported drivers is implemented.
 * - Semantics are preserved; implementation is Zirvium-native.
 * - Where a Linux concept has no direct Zirvium analogue (e.g. sysfs, kobject)
 *   the macro expands to a safe no-op so driver code still compiles.
 *
 * Ported drivers that include this header must NOT also include real Linux
 * kernel headers.
 */
#ifndef ZIRVIUM_DRIVERS_COMPAT_LINUX_COMPAT_H
#define ZIRVIUM_DRIVERS_COMPAT_LINUX_COMPAT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

/* ── Fundamental integer types (match Linux exactly) ─────────────────────── */
typedef uint8_t   u8;
typedef uint16_t  u16;
typedef uint32_t  u32;
typedef uint64_t  u64;
typedef int8_t    s8;
typedef int16_t   s16;
typedef int32_t   s32;
typedef int64_t   s64;

typedef uint8_t   __u8;
typedef uint16_t  __u16;
typedef uint32_t  __u32;
typedef uint64_t  __u64;

/* Little-endian annotated types (no actual byte-swap on x86-64) */
typedef uint16_t  __le16;
typedef uint32_t  __le32;
typedef uint64_t  __le64;
typedef uint16_t  __be16;
typedef uint32_t  __be32;
typedef uint64_t  __be64;

typedef unsigned long   ulong;
typedef unsigned int    uint;
typedef unsigned long   uintptr_t;  /* already in stdint but just in case */

/* ── Byte-order helpers (little-endian host) ─────────────────────────────── */
static inline u16 cpu_to_le16(u16 v) { return v; }
static inline u32 cpu_to_le32(u32 v) { return v; }
static inline u64 cpu_to_le64(u64 v) { return v; }
static inline u16 le16_to_cpu(u16 v) { return v; }
static inline u32 le32_to_cpu(u32 v) { return v; }
static inline u64 le64_to_cpu(u64 v) { return v; }

static inline u16 cpu_to_be16(u16 v) {
    return (u16)((v >> 8) | (v << 8));
}
static inline u32 cpu_to_be32(u32 v) {
    return ((v & 0xFF) << 24) | ((v & 0xFF00) << 8) |
           ((v >> 8) & 0xFF00) | ((v >> 24) & 0xFF);
}
static inline u16 be16_to_cpu(u16 v) { return cpu_to_be16(v); }
static inline u32 be32_to_cpu(u32 v) { return cpu_to_be32(v); }

/* ── Common macros ────────────────────────────────────────────────────────── */
#define BIT(n)              (1UL << (n))
#define BIT_ULL(n)          (1ULL << (n))
#define GENMASK(h, l)       (((~0UL) << (l)) & (~0UL >> (BITS_PER_LONG - 1 - (h))))
#define GENMASK_ULL(h, l)   (((~0ULL) << (l)) & (~0ULL >> (64 - 1 - (h))))
#define BITS_PER_LONG       64
#define BITS_PER_BYTE       8

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a)       (sizeof(a) / sizeof((a)[0]))
#endif

#define ALIGN(x, a)         (((x) + (a) - 1) & ~((a) - 1))
#define IS_ALIGNED(x, a)    (((x) & ((a) - 1)) == 0)

#define min(a, b)           ((a) < (b) ? (a) : (b))
#define max(a, b)           ((a) > (b) ? (a) : (b))
#define clamp(val, lo, hi)  min(max(val, lo), hi)

#define DIV_ROUND_UP(n, d)  (((n) + (d) - 1) / (d))
#define ROUND_UP(n, d)      (DIV_ROUND_UP(n, d) * (d))

/* ── Error codes (Linux errno values) ────────────────────────────────────── */
#define EPERM    1
#define ENOENT   2
#define EIO      5
#define ENOMEM  12
#define ENODEV  19
#define EINVAL  22
#define ETIMEDOUT 110

#define IS_ERR_VALUE(x) ((unsigned long)(x) >= (unsigned long)-(4096))
static inline bool IS_ERR(const void *ptr) {
    return IS_ERR_VALUE((unsigned long)ptr);
}
static inline long PTR_ERR(const void *ptr) { return (long)ptr; }
static inline void *ERR_PTR(long err)       { return (void *)err; }

/* ── Memory allocation ───────────────────────────────────────────────────── */
/* Forward declaration — implemented in kernel/mm/heap.c (bump allocator) */
void *kzalloc(size_t size, unsigned int flags);
void *kmalloc(size_t size, unsigned int flags);
void  kfree(void *ptr);
void *kcalloc(size_t n, size_t size, unsigned int flags);
void *krealloc(void *ptr, size_t new_size, unsigned int flags);
#define vmalloc(sz)         kzalloc(sz, 0)
#define vfree(p)            kfree(p)
#define devm_kzalloc(d,s,f) kzalloc(s, f)
#define devm_kmalloc(d,s,f) kmalloc(s, f)
#define GFP_KERNEL  0
#define GFP_ATOMIC  1
#define GFP_DMA32   2

/* ── MMIO accessors ──────────────────────────────────────────────────────── */
static inline u8  readb(const volatile void *addr)
    { return *(const volatile u8  *)addr; }
static inline u16 readw(const volatile void *addr)
    { return *(const volatile u16 *)addr; }
static inline u32 readl(const volatile void *addr)
    { return *(const volatile u32 *)addr; }
static inline u64 readq(const volatile void *addr)
    { return *(const volatile u64 *)addr; }
static inline void writeb(u8  val, volatile void *addr)
    { *(volatile u8  *)addr = val; }
static inline void writew(u16 val, volatile void *addr)
    { *(volatile u16 *)addr = val; }
static inline void writel(u32 val, volatile void *addr)
    { *(volatile u32 *)addr = val; }
static inline void writeq(u64 val, volatile void *addr)
    { *(volatile u64 *)addr = val; }

/* Memory barriers */
#define mb()    __asm__ volatile("mfence" ::: "memory")
#define rmb()   __asm__ volatile("lfence" ::: "memory")
#define wmb()   __asm__ volatile("sfence" ::: "memory")
#define barrier() __asm__ volatile("" ::: "memory")

/* ── Delay helpers ───────────────────────────────────────────────────────── */
/* Simple busy-wait loops — replaced by timer infrastructure later */
static inline void udelay(unsigned long us)
{
    /* Rough calibration: ~1000 loops ≈ 1 µs on a 1 GHz machine.
     * Replace with a proper TSC-based delay once the calibration loop runs. */
    volatile unsigned long n = us * 1000;
    while (n--) __asm__ volatile("pause");
}
static inline void mdelay(unsigned long ms) { udelay(ms * 1000); }
static inline void msleep(unsigned int ms)  { mdelay(ms); }
static inline void usleep_range(unsigned long min_us, unsigned long max_us)
    { udelay((min_us + max_us) / 2); }

/* ── Logging ──────────────────────────────────────────────────────────────── */
/* Route driver log messages through the unified kernel console so they
 * appear on VGA, the framebuffer, and the serial port simultaneously. */
void kputs(const char *s);   /* kernel/console.h — forward declaration */

#define pr_err(fmt, ...)   kputs("[ERR] " fmt)
#define pr_warn(fmt, ...)  kputs("[WRN] " fmt)
#define pr_info(fmt, ...)  kputs("[INF] " fmt)
#define pr_debug(fmt, ...) /* debug disabled by default */

/* Device-context logging (dev pointer ignored, messages go to serial) */
#define dev_err(dev, fmt, ...)   pr_err(fmt, ##__VA_ARGS__)
#define dev_warn(dev, fmt, ...)  pr_warn(fmt, ##__VA_ARGS__)
#define dev_info(dev, fmt, ...)  pr_info(fmt, ##__VA_ARGS__)
#define dev_dbg(dev, fmt, ...)   pr_debug(fmt, ##__VA_ARGS__)

/* ── PCI types (minimal, for driver registration) ────────────────────────── */
#define PCI_ANY_ID   (~0U)

/* Forward declaration — full definition in drivers/pci/pci.h */
struct pci_dev;

/* ── Spinlock stub (single-core for now, replaced by SMP spinlocks later) ── */
typedef struct { volatile int locked; } spinlock_t;
#define DEFINE_SPINLOCK(name)       spinlock_t name = { 0 }
#define spin_lock_init(l)           do { (l)->locked = 0; } while (0)
#define spin_lock(l)                do { (void)(l); } while (0)
#define spin_unlock(l)              do { (void)(l); } while (0)
#define spin_lock_irqsave(l, f)     do { (void)(l); (f) = 0; } while (0)
#define spin_unlock_irqrestore(l,f) do { (void)(l); (void)(f); } while (0)

/* ── Mutex stub ──────────────────────────────────────────────────────────── */
typedef struct { volatile int locked; } mutex_t;
#define DEFINE_MUTEX(name)     mutex_t name = { 0 }
#define mutex_init(m)          do { (m)->locked = 0; } while (0)
#define mutex_lock(m)          do { (void)(m); } while (0)
#define mutex_unlock(m)        do { (void)(m); } while (0)
#define mutex_trylock(m)       1

/* ── Completion stub ─────────────────────────────────────────────────────── */
typedef struct { volatile int done; } completion_t;
#define init_completion(c)         do { (c)->done = 0; } while (0)
#define complete(c)                do { (c)->done = 1; } while (0)
#define wait_for_completion(c)     do { while (!(c)->done) __asm__("pause"); } while (0)
#define wait_for_completion_timeout(c, t) \
    ({ unsigned long _t = (t); while (!(c)->done && _t--) udelay(1); (c)->done; })

/* ── Work queue stub (synchronous for now) ───────────────────────────────── */
struct work_struct { void (*func)(struct work_struct *); };
#define INIT_WORK(w, fn)   do { (w)->func = (fn); } while (0)
#define schedule_work(w)   do { if ((w)->func) (w)->func(w); } while (0)
#define flush_work(w)      do { (void)(w); } while (0)
#define cancel_work_sync(w) do { (void)(w); } while (0)

/* ── Kernel module macros (no-ops in monolithic kernel) ──────────────────── */
#define MODULE_AUTHOR(s)
#define MODULE_DESCRIPTION(s)
#define MODULE_LICENSE(s)
#define MODULE_FIRMWARE(s)
#define MODULE_DEVICE_TABLE(t, n)
#define module_param(n, t, p)
#define MODULE_PARM_DESC(n, d)

/* ── Misc helpers ─────────────────────────────────────────────────────────── */
#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#define WARN_ON(cond)      do { if (unlikely(cond)) pr_warn("WARN_ON at %s:%d\n", __FILE__, __LINE__); } while (0)
#define WARN_ON_ONCE(cond) WARN_ON(cond)
#define BUG_ON(cond)       do { if (unlikely(cond)) { pr_err("BUG_ON at %s:%d\n", __FILE__, __LINE__); for(;;) __asm__("hlt"); } } while (0)
#define BUG()              BUG_ON(1)

#define __must_check        __attribute__((warn_unused_result))
#define __packed            __attribute__((packed))
#define __aligned(x)        __attribute__((aligned(x)))
#define __iomem             /* no address space qualifiers needed */
#define __user              /* no user/kernel distinction yet */

/* ── Bit operations ──────────────────────────────────────────────────────── */
static inline void set_bit(int nr, volatile unsigned long *addr) {
    *addr |= (1UL << (nr % BITS_PER_LONG));
}
static inline void clear_bit(int nr, volatile unsigned long *addr) {
    *addr &= ~(1UL << (nr % BITS_PER_LONG));
}
static inline int test_bit(int nr, const volatile unsigned long *addr) {
    return (int)((*addr >> (nr % BITS_PER_LONG)) & 1);
}
static inline int test_and_set_bit(int nr, volatile unsigned long *addr) {
    int old = test_bit(nr, addr);
    set_bit(nr, addr);
    return old;
}
static inline int test_and_clear_bit(int nr, volatile unsigned long *addr) {
    int old = test_bit(nr, addr);
    clear_bit(nr, addr);
    return old;
}

#endif /* ZIRVIUM_DRIVERS_COMPAT_LINUX_COMPAT_H */
