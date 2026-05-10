#ifndef ZIRVIUM_DRIVERS_COMPAT_LINUX_COMPAT_H
#define ZIRVIUM_DRIVERS_COMPAT_LINUX_COMPAT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

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

typedef uint16_t  __le16;
typedef uint32_t  __le32;
typedef uint64_t  __le64;
typedef uint16_t  __be16;
typedef uint32_t  __be32;
typedef uint64_t  __be64;

typedef unsigned long   ulong;
typedef unsigned int    uint;

static inline u16 cpu_to_le16(u16 v) { return v; }
static inline u32 cpu_to_le32(u32 v) { return v; }
static inline u64 cpu_to_le64(u64 v) { return v; }
static inline u16 le16_to_cpu(u16 v) { return v; }
static inline u32 le32_to_cpu(u32 v) { return v; }
static inline u64 le64_to_cpu(u64 v) { return v; }

static inline u16 cpu_to_be16(u16 v) { return (u16)((v >> 8) | (v << 8)); }
static inline u32 cpu_to_be32(u32 v) {
    return ((v & 0xFF) << 24) | ((v & 0xFF00) << 8) |
           ((v >> 8) & 0xFF00) | ((v >> 24) & 0xFF);
}
static inline u64 cpu_to_be64(u64 v) {
    return ((u64)cpu_to_be32((u32)v) << 32) | cpu_to_be32((u32)(v >> 32));
}
static inline u16 be16_to_cpu(u16 v) { return cpu_to_be16(v); }
static inline u32 be32_to_cpu(u32 v) { return cpu_to_be32(v); }
static inline u64 be64_to_cpu(u64 v) { return cpu_to_be64(v); }

#define BIT(n)              (1UL << (n))
#define BIT_ULL(n)          (1ULL << (n))
#define GENMASK(h, l)       (((~0UL) << (l)) & (~0UL >> (64 - 1 - (h))))
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
#define min_t(t, a, b)      ((t)(a) < (t)(b) ? (t)(a) : (t)(b))
#define max_t(t, a, b)      ((t)(a) > (t)(b) ? (t)(a) : (t)(b))
#define clamp(val, lo, hi)  min(max(val, lo), hi)
#define clamp_t(t, v, lo, hi) min_t(t, max_t(t, v, lo), hi)

#define DIV_ROUND_UP(n, d)  (((n) + (d) - 1) / (d))
#define DIV_ROUND_CLOSEST(n, d) (((n) + (d)/2) / (d))
#define ROUND_UP(n, d)      (DIV_ROUND_UP(n, d) * (d))
#define roundup(x, y)       ((((x) + ((y) - 1)) / (y)) * (y))
#define rounddown(x, y)     (((x) / (y)) * (y))
#define abs(x)              ((x) < 0 ? -(x) : (x))
#define swap(a, b)          do { typeof(a) _t = (a); (a) = (b); (b) = _t; } while (0)

#define lower_32_bits(n)    ((u32)(n))
#define upper_32_bits(n)    ((u32)((n) >> 32))

#define EPERM    1
#define ENOENT   2
#define EIO      5
#define ENXIO    6
#define ENOMEM  12
#define EACCES  13
#define EFAULT  14
#define EBUSY   16
#define ENODEV  19
#define EINVAL  22
#define EAGAIN  35
#define ETIMEDOUT 110

#define IS_ERR_VALUE(x) ((unsigned long)(x) >= (unsigned long)-4095)
static inline bool IS_ERR(const void *ptr) { return IS_ERR_VALUE((unsigned long)ptr); }
static inline long PTR_ERR(const void *ptr) { return (long)ptr; }
static inline void *ERR_PTR(long err)       { return (void *)err; }
static inline long IS_ERR_OR_NULL(const void *ptr) { return !ptr || IS_ERR(ptr); }

void *kzalloc(size_t size, unsigned int flags);
void *kmalloc(size_t size, unsigned int flags);
void  kfree(void *ptr);
void *kcalloc(size_t n, size_t size, unsigned int flags);
void *krealloc(void *ptr, size_t new_size, unsigned int flags);
static inline void *kzalloc_node(size_t sz, unsigned int f, int n) { (void)n; return kzalloc(sz, f); }
#define vmalloc(sz)         kzalloc(sz, 0)
#define vfree(p)            kfree(p)
#define devm_kzalloc(d,s,f) kzalloc(s, f)
#define devm_kmalloc(d,s,f) kmalloc(s, f)
#define devm_kfree(d,p)     kfree(p)
#define GFP_KERNEL          0
#define GFP_ATOMIC          1
#define GFP_DMA32           2
#define GFP_KERNEL_ACCOUNT  0

static inline u8  readb(const volatile void *a) { return *(const volatile u8  *)a; }
static inline u16 readw(const volatile void *a) { return *(const volatile u16 *)a; }
static inline u32 readl(const volatile void *a) { return *(const volatile u32 *)a; }
static inline u64 readq(const volatile void *a) { return *(const volatile u64 *)a; }
static inline void writeb(u8  v, volatile void *a) { *(volatile u8  *)a = v; }
static inline void writew(u16 v, volatile void *a) { *(volatile u16 *)a = v; }
static inline void writel(u32 v, volatile void *a) { *(volatile u32 *)a = v; }
static inline void writeq(u64 v, volatile void *a) { *(volatile u64 *)a = v; }

#define mb()    __asm__ volatile("mfence" ::: "memory")
#define rmb()   __asm__ volatile("lfence" ::: "memory")
#define wmb()   __asm__ volatile("sfence" ::: "memory")
#define barrier() __asm__ volatile("" ::: "memory")
#define dma_rmb()   rmb()
#define dma_wmb()   wmb()
#define smp_mb()    mb()
#define smp_rmb()   rmb()
#define smp_wmb()   wmb()

#define READ_ONCE(x)        (*(volatile typeof(x) *)&(x))
#define WRITE_ONCE(x, v)    ((*(volatile typeof(x) *)&(x)) = (v))

static inline void udelay(unsigned long us)
{
    volatile unsigned long n = us * 1000;
    while (n--) __asm__ volatile("pause");
}
static inline void mdelay(unsigned long ms) { udelay(ms * 1000); }
static inline void msleep(unsigned int ms)  { mdelay(ms); }
static inline void usleep_range(unsigned long min_us, unsigned long max_us)
    { udelay((min_us + max_us) / 2); }

void kputs(const char *s);

#define pr_err(fmt, ...)   kputs("[ERR] " fmt)
#define pr_warn(fmt, ...)  kputs("[WRN] " fmt)
#define pr_info(fmt, ...)  kputs("[INF] " fmt)
#define pr_devel(fmt, ...) kputs("[DBG] " fmt)
#define pr_dbg(fmt, ...)   do { } while (0)

#define dev_err(dev, fmt, ...)   pr_err(fmt, ##__VA_ARGS__)
#define dev_warn(dev, fmt, ...)  pr_warn(fmt, ##__VA_ARGS__)
#define dev_info(dev, fmt, ...)  pr_info(fmt, ##__VA_ARGS__)
#define dev_dbg(dev, fmt, ...)   pr_dbg(fmt, ##__VA_ARGS__)
#define dev_notice(dev, fmt, ...) pr_info(fmt, ##__VA_ARGS__)

#define dev_err_once(dev, fmt, ...)  dev_err(dev, fmt, ##__VA_ARGS__)
#define dev_warn_once(dev, fmt, ...) dev_warn(dev, fmt, ##__VA_ARGS__)

#define PCI_ANY_ID   (~0U)
struct pci_dev;

typedef struct { volatile int locked; } spinlock_t;
#define DEFINE_SPINLOCK(name)       spinlock_t name = { 0 }
#define spin_lock_init(l)           do { (l)->locked = 0; } while (0)
#define spin_lock(l)                do { (void)(l); } while (0)
#define spin_unlock(l)              do { (void)(l); } while (0)
#define spin_lock_irqsave(l, f)     do { (void)(l); (f) = 0; } while (0)
#define spin_unlock_irqrestore(l,f) do { (void)(l); (void)(f); } while (0)
#define spin_lock_bh(l)             spin_lock(l)
#define spin_unlock_bh(l)           spin_unlock(l)
#define spin_is_locked(l)           0

typedef struct { volatile int locked; } mutex_t;
#define DEFINE_MUTEX(name)     mutex_t name = { 0 }
#define mutex_init(m)          do { (m)->locked = 0; } while (0)
#define mutex_lock(m)          do { (void)(m); } while (0)
#define mutex_unlock(m)        do { (void)(m); } while (0)
#define mutex_trylock(m)       1
#define mutex_is_locked(m)     0

typedef struct { volatile int count; } atomic_t;
#define ATOMIC_INIT(i)      { (i) }
#define atomic_read(v)      ((v)->count)
#define atomic_set(v, i)    do { (v)->count = (i); } while (0)
#define atomic_add(i, v)    do { (v)->count += (i); } while (0)
#define atomic_sub(i, v)    do { (v)->count -= (i); } while (0)
#define atomic_inc(v)       atomic_add(1, v)
#define atomic_dec(v)       atomic_sub(1, v)
#define atomic_dec_and_test(v)  (atomic_dec(v), (v)->count == 0)
#define atomic_inc_and_test(v)  (atomic_inc(v), (v)->count == 0)
#define atomic_add_return(i, v) ((v)->count += (i))
#define atomic_sub_return(i, v) ((v)->count -= (i))

typedef struct { volatile long count; } atomic_long_t;
#define ATOMIC_LONG_INIT(i) { (i) }
#define atomic_long_read(v)     ((v)->count)
#define atomic_long_set(v, i)   do { (v)->count = (i); } while (0)
#define atomic_long_inc(v)      do { (v)->count++; } while (0)
#define atomic_long_dec(v)      do { (v)->count--; } while (0)

typedef struct { volatile int done; } completion_t;
#define init_completion(c)         do { (c)->done = 0; } while (0)
#define reinit_completion(c)       init_completion(c)
#define complete(c)                do { (c)->done = 1; } while (0)
#define complete_all(c)            do { (c)->done = 1; } while (0)
#define wait_for_completion(c)     do { while (!(c)->done) __asm__("pause"); } while (0)
#define wait_for_completion_timeout(c, t) \
    ({ unsigned long _t = (t); while (!(c)->done && _t--) udelay(1); (c)->done; })
#define try_wait_for_completion(c) (!!(c)->done)

struct work_struct { void (*func)(struct work_struct *); };
#define INIT_WORK(w, fn)   do { (w)->func = (fn); } while (0)
#define schedule_work(w)   do { if ((w)->func) (w)->func(w); } while (0)
#define flush_work(w)      do { (void)(w); } while (0)
#define cancel_work_sync(w) do { (void)(w); } while (0)
#define schedule_delayed_work(w, d) schedule_work(w)
#define flush_delayed_work(w)       flush_work(w)
#define INIT_DELAYED_WORK(w, fn)    INIT_WORK(&(w)->work, fn)
struct delayed_work { struct work_struct work; };

struct list_head {
    struct list_head *next, *prev;
};
#define LIST_HEAD_INIT(name) { &(name), &(name) }
#define LIST_HEAD(name) struct list_head name = LIST_HEAD_INIT(name)
static inline void INIT_LIST_HEAD(struct list_head *l) { l->next = l->prev = l; }
static inline void __list_add(struct list_head *n, struct list_head *p, struct list_head *nx) {
    nx->prev = n; n->next = nx; n->prev = p; p->next = n;
}
static inline void list_add(struct list_head *n, struct list_head *h) { __list_add(n, h, h->next); }
static inline void list_add_tail(struct list_head *n, struct list_head *h) { __list_add(n, h->prev, h); }
static inline void __list_del(struct list_head *p, struct list_head *nx) { p->next = nx; nx->prev = p; }
static inline void list_del(struct list_head *e) { __list_del(e->prev, e->next); e->next = e->prev = NULL; }
static inline void list_del_init(struct list_head *e) { list_del(e); INIT_LIST_HEAD(e); }
static inline int list_empty(const struct list_head *h) { return h->next == h; }
static inline void list_move(struct list_head *l, struct list_head *h) { list_del(l); list_add(l, h); }
static inline void list_move_tail(struct list_head *l, struct list_head *h) { list_del(l); list_add_tail(l, h); }
static inline int list_is_singular(const struct list_head *h) { return !list_empty(h) && h->next == h->prev; }
static inline void list_splice(struct list_head *f, struct list_head *h) {
    if (!list_empty(f)) { f->next->prev = h; f->prev->next = h->next; h->next->prev = f->prev; h->next = f->next; INIT_LIST_HEAD(f); }
}
#define list_entry(ptr, type, member) container_of(ptr, type, member)
#define list_first_entry(ptr, type, member) list_entry((ptr)->next, type, member)
#define list_last_entry(ptr, type, member) list_entry((ptr)->prev, type, member)
#define list_next_entry(pos, member) list_entry((pos)->member.next, typeof(*(pos)), member)
#define list_prev_entry(pos, member) list_entry((pos)->member.prev, typeof(*(pos)), member)
#define list_for_each(pos, head) for (pos = (head)->next; pos != (head); pos = pos->next)
#define list_for_each_safe(pos, n, head) for (pos = (head)->next, n = pos->next; pos != (head); pos = n, n = pos->next)
#define list_for_each_entry(pos, head, member) for (pos = list_first_entry(head, typeof(*pos), member); &pos->member != (head); pos = list_next_entry(pos, member))
#define list_for_each_entry_safe(pos, n, head, member) for (pos = list_first_entry(head, typeof(*pos), member), n = list_next_entry(pos, member); &pos->member != (head); pos = n, n = list_next_entry(n, member))
#define list_for_each_entry_reverse(pos, head, member) for (pos = list_last_entry(head, typeof(*pos), member); &pos->member != (head); pos = list_prev_entry(pos, member))

#define MODULE_AUTHOR(s)
#define MODULE_DESCRIPTION(s)
#define MODULE_LICENSE(s)
#define MODULE_FIRMWARE(s)
#define MODULE_DEVICE_TABLE(t, n)
#define module_param(n, t, p)
#define MODULE_PARM_DESC(n, d)
#define module_init(f)
#define module_exit(f)
#define MODULE_VERSION(v)

#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#define WARN_ON(cond)      do { if (unlikely(cond)) pr_warn("WARN_ON at %s:%d\n", __FILE__, __LINE__); } while (0)
#define WARN_ON_ONCE(cond) do { static int _w; if (!_w) WARN_ON(cond); } while (0)
#define WARN(cond, fmt...) do { if (unlikely(cond)) pr_warn(fmt); } while (0)
#define BUG_ON(cond)       do { if (unlikely(cond)) { pr_err("BUG at %s:%d\n", __FILE__, __LINE__); for(;;) __asm__("hlt"); } } while (0)
#define BUG()              BUG_ON(1)
#define BUILD_BUG_ON(cond) ((void)sizeof(char[1 - 2 * !!(cond)]))
#define BUILD_BUG()        BUILD_BUG_ON(1)

#define __must_check        __attribute__((warn_unused_result))
#define __packed            __attribute__((packed))
#define __aligned(x)        __attribute__((aligned(x)))
#define __iomem
#define __user
#define __force
#define __read_mostly
#define __ro_after_init
#define __init
#define __exit
#define __devinit
#define __devinitdata
#define __maybe_unused      __attribute__((unused))
#define __always_unused     __attribute__((unused))
#define __printf(a, b)      __attribute__((format(printf, a, b)))
#define noinline            __attribute__((noinline))
#define inline              inline __attribute__((always_inline))

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
    int old = test_bit(nr, addr); set_bit(nr, addr); return old;
}
static inline int test_and_clear_bit(int nr, volatile unsigned long *addr) {
    int old = test_bit(nr, addr); clear_bit(nr, addr); return old;
}
#define fls(x) (x ? 64 - (unsigned)__builtin_clzll((unsigned long long)(x)) : 0)
#define fls64(x) fls(x)
#define ffs(x) (x ? (unsigned)__builtin_ctzll((unsigned long long)(x)) + 1 : 0)
#define __ffs(x) ffs(x)
#define __fls(x) (fls(x) - 1)
#define hweight32(x) ((unsigned)__builtin_popcount(x))
#define hweight64(x) ((unsigned)__builtin_popcountll(x))

static inline void *memset64(uint64_t *p, uint64_t v, size_t n) {
    for (size_t i = 0; i < n; i++) p[i] = v;
    return p;
}
static inline void *memset32(uint32_t *p, uint32_t v, size_t n) {
    for (size_t i = 0; i < n; i++) p[i] = v;
    return p;
}
#define memcpy_fromio(a, b, c) memcpy((a), (void *)(b), (c))
#define memcpy_toio(a, b, c)   memcpy((void *)(a), (b), (c))
#define memset_io(a, v, c)     memset((void *)(a), (v), (c))

#define strlcpy(d, s, sz)   ({ size_t _l = strlen(s); if (sz) { size_t _c = (_l < sz) ? _l : (sz-1); memcpy(d, s, _c); d[_c] = '\0'; } _l; })
#define strlcat(d, s, sz)   ({ size_t _dl = strlen(d); size_t _sl = strlen(s); size_t _rem = (sz > _dl) ? (sz - _dl - 1) : 0; if (_rem) { size_t _c = (_sl < _rem) ? _sl : _rem; memcpy(d+_dl, s, _c); d[_dl+_c] = '\0'; } _dl + _sl; })
#define strncpy(d, s, n)    ({ size_t _l = strnlen(s, n); memcpy(d, s, _l); if (_l < n) memset(d+_l, 0, n-_l); d; })
#define strncat(d, s, n)    ({ size_t _dl = strlen(d); size_t _c = strnlen(s, n); if (_c) { memcpy(d+_dl, s, _c); d[_dl+_c] = '\0'; } d; })
#define strcasecmp(s1, s2)  ({ int _r; const char *_a=(s1),*_b=(s2); while (*_a && *_b) { int _d = ((*_a>='a'&&*_a<='f')?*_a-0x20:*_a) - ((*_b>='a'&&*_b<='f')?*_b-0x20:*_b); if (_d) { _r=_d; goto _d; } _a++;_b++; } _r = (*_a-*_b); _d: _r; })

static inline int scnprintf(char *buf, size_t sz, const char *fmt, ...) {
    (void)buf; (void)sz; (void)fmt; return 0;
}
static inline int snprintf(char *buf, size_t sz, const char *fmt, ...) {
    (void)buf; (void)sz; (void)fmt; return 0;
}
#define sprintf(buf, fmt...) snprintf(buf, 0xFFFF, fmt)

#define do_div(n, base) ({ uint64_t _r = (n) % (base); (n) /= (base); _r; })
#define div_u64_rem(d, base, rem) ({ *(rem) = (d) % (base); (d) / (base); })
#define div_u64(d, base)          ({ (d) / (base); })
#define div_s64(d, base)          ({ (d) / (base); })

#define time_after(a, b)    ((long)(b) - (long)(a) < 0)
#define time_before(a, b)   time_after(b, a)
#define time_after_eq(a, b) ((long)(a) - (long)(b) >= 0)
#define time_before_eq(a,b) time_after_eq(b, a)

#define jiffies                 ((unsigned long)0)
#define jiffies_to_msecs(j)     (j)
#define msecs_to_jiffies(m)     (m)
#define usecs_to_jiffies(u)     (u)
#define HZ                      1000

typedef struct timer_list {
    unsigned long expires;
    void (*function)(struct timer_list *);
    unsigned long data;
} timer_list_t;
#define TIMER_INITIALIZER(_fn, _expires, _data) { .expires = _expires, .function = _fn, .data = _data }
#define DEFINE_TIMER(name, fn, expires, data) timer_list_t name = TIMER_INITIALIZER(fn, expires, data)
static inline void timer_setup(timer_list_t *t, void (*fn)(struct timer_list *), unsigned int flags) {
    (void)flags; t->function = fn;
}
static inline void init_timer(timer_list_t *t) { t->function = NULL; }
static inline void add_timer(timer_list_t *t) { t->function ? t->function(t) : (void)0; }
static inline int mod_timer(timer_list_t *t, unsigned long expires) {
    (void)expires; t->function ? t->function(t) : (void)0; return 0;
}
static inline int del_timer(timer_list_t *t) { (void)t; return 0; }
static inline int del_timer_sync(timer_list_t *t) { (void)t; return 0; }
static inline int timer_pending(const timer_list_t *t) { (void)t; return 0; }

typedef struct { volatile int pending; } wait_queue_head_t;
#define init_waitqueue_head(w)  do { (w)->pending = 0; } while (0)
#define DECLARE_WAITQUEUE(n, t) int n = 0
#define DECLARE_WAIT_QUEUE_HEAD(n) wait_queue_head_t n = { 0 }
#define add_wait_queue(q, w)    do { (void)(w); } while (0)
#define remove_wait_queue(q, w) do { (void)(w); } while (0)
#define __set_current_state(s)  do { } while (0)
#define set_current_state(s)    do { } while (0)
#define __add_wait_queue(q, w)  do { (void)(w); } while (0)
#define finish_wait(q, w)       do { (void)(w); } while (0)
#define wake_up(q)              do { (q)->pending = 1; } while (0)
#define wake_up_all(q)          wake_up(q)
#define wake_up_interruptible(q) wake_up(q)
#define wait_event(wq, cond)    do { while (!(cond)) __asm__("pause"); } while (0)
#define wait_event_interruptible(wq, cond) ({ wait_event(wq, cond); 0; })
#define wait_event_timeout(wq, cond, timeout) ({ unsigned long _t = timeout; while (!(cond) && _t--) udelay(1); (cond) ? 1 : 0; })
#define wait_event_interruptible_timeout(wq, cond, t) wait_event_timeout(wq, cond, t)

/* ── IRQ API (only if kernel/irq/irq.h not already included) ─────────────── */
#ifndef ZIRVIUM_KERNEL_IRQ_IRQ_H

typedef struct { int irq; void *dev_id; } irq_handler_t;
typedef int (*irq_handler_fn)(int, void *);
#define IRQF_SHARED         0x0001
#define IRQF_TRIGGER_RISING 0x0002
#define IRQF_TRIGGER_FALLING 0x0004
#define IRQF_ONESHOT        0x0008
#define IRQF_NO_SUSPEND     0x0010
#define IRQF_PROBE_SHARED   0x0020

static inline int request_irq(unsigned int irq, irq_handler_fn handler,
                               unsigned long flags, const char *name, void *dev) {
    (void)irq; (void)handler; (void)flags; (void)name; (void)dev;
    pr_info("request_irq(%u) - stub\n", irq);
    return 0;
}
static inline void free_irq(unsigned int irq, void *dev_id) {
    (void)irq; (void)dev_id;
}
#define IRQ_NONE 0
#define IRQ_HANDLED 1
#define IRQ_WAKE_THREAD 2

#endif /* ZIRVIUM_KERNEL_IRQ_IRQ_H */
#define IRQ_RETVAL(x) (x)

typedef struct kref { atomic_t refcount; } kref_t;
#define kref_init(k)         atomic_set(&(k)->refcount, 1)
#define kref_get(k)          atomic_inc(&(k)->refcount)
#define kref_put(k, r)       do { if (atomic_dec_and_test(&(k)->refcount) && (r)) (r)(k); } while (0)
#define kref_sub(k, c, r)    do { int _n = atomic_sub_return(c, &(k)->refcount); if ((_n) == 0 && (r)) (r)(k); } while (0)
#define kref_read(k)         atomic_read(&(k)->refcount)

typedef struct device {
    void *driver_data;
} device_t;
#define dev_get_drvdata(d)   ((d)->driver_data)
#define dev_set_drvdata(d, p) do { (d)->driver_data = (p); } while (0)

typedef struct pci_device_id {
    u32 vendor, device;
    u32 subvendor, subdevice;
    u32 class, class_mask;
    unsigned long driver_data;
} pci_device_id_t;
#define PCI_DEVICE(v,d)             .vendor = (v), .device = (d), .subvendor = PCI_ANY_ID, .subdevice = PCI_ANY_ID
#define PCI_DEVICE_SUB(v,d,sv,sd)   .vendor = (v), .device = (d), .subvendor = (sv), .subdevice = (sd)
#define PCI_VDEVICE(vendor, device) .vendor = PCI_VENDOR_ID_##vendor, .device = (device), .subvendor = PCI_ANY_ID, .subdevice = PCI_ANY_ID
#define PCI_ANY_ID  (~0U)

#ifndef PCI_VENDOR_ID_INTEL
#define PCI_VENDOR_ID_INTEL     0x8086
#define PCI_VENDOR_ID_NVIDIA    0x10DE
#define PCI_VENDOR_ID_AMD       0x1002
#define PCI_VENDOR_ID_REALTEK   0x10EC
#define PCI_VENDOR_ID_BROADCOM  0x14E4
#define PCI_VENDOR_ID_ATHEROS   0x168C
#define PCI_VENDOR_ID_VMWARE    0x15AD
#define PCI_VENDOR_ID_REDHAT    0x1B36
#define PCI_VENDOR_ID_GOOGLE    0x1AE0
#endif

#define PCI_CLASS_DISPLAY_VGA   0x0300
#define PCI_CLASS_NETWORK_ETHERNET 0x0200
#define PCI_CLASS_STORAGE_SATA  0x0106
#define PCI_CLASS_STORAGE_NVME  0x0108
#define PCI_CLASS_SERIAL_USB    0x0C03

struct pci_driver_linux {
    const char *name;
    const pci_device_id_t *id_table;
    int  (*probe)(struct pci_dev *pdev, const pci_device_id_t *id);
    void (*remove)(struct pci_dev *pdev);
    int  (*suspend)(struct pci_dev *pdev, void *state);
    int  (*resume)(struct pci_dev *pdev);
    void *driver_data;
};

static inline u32 pci_resource_start(const struct pci_dev *dev, int bar) { (void)dev; (void)bar; return 0; }
static inline u32 pci_resource_end(const struct pci_dev *dev, int bar) { (void)dev; (void)bar; return 0; }
static inline u32 pci_resource_len(const struct pci_dev *dev, int bar) { (void)dev; (void)bar; return 0; }
static inline u32 pci_resource_flags(const struct pci_dev *dev, int bar) { (void)dev; (void)bar; return 0; }

static inline void *pci_iomap(struct pci_dev *dev, int bar, unsigned long maxlen) {
    (void)dev; (void)bar; (void)maxlen; return NULL;
}
static inline void pci_iounmap(struct pci_dev *dev, void *addr) { (void)dev; (void)addr; }
static inline void *pci_ioremap_bar(struct pci_dev *pdev, int bar) { return pci_iomap(pdev, bar, 0); }

static inline int pci_enable_device_mem(struct pci_dev *dev) { (void)dev; return 0; }
static inline int pci_set_master(struct pci_dev *dev) { (void)dev; return 0; }
static inline void pci_disable_device(struct pci_dev *dev) { (void)dev; }
static inline int pci_select_bars(struct pci_dev *dev, unsigned long flags) { (void)dev; (void)flags; return 0; }
static inline int pci_request_regions(struct pci_dev *dev, const char *name) { (void)dev; (void)name; return 0; }
static inline void pci_release_regions(struct pci_dev *dev) { (void)dev; }
static inline int pci_request_region(struct pci_dev *dev, int bar, const char *name) { (void)dev; (void)bar; (void)name; return 0; }
static inline void pci_release_region(struct pci_dev *dev, int bar) { (void)dev; (void)bar; }

static inline void *pci_get_drvdata(const struct pci_dev *dev) { (void)dev; return NULL; }
static inline void pci_set_drvdata(struct pci_dev *dev, void *data) { (void)dev; (void)data; }

static inline u16 pci_read_config_word(const struct pci_dev *dev, int offset) { (void)dev; (void)offset; return 0; }
static inline u32 pci_read_config_dword(const struct pci_dev *dev, int offset) { (void)dev; (void)offset; return 0; }
static inline int pci_write_config_word(struct pci_dev *dev, int offset, u16 val) { (void)dev; (void)offset; (void)val; return 0; }
static inline int pci_write_config_dword(struct pci_dev *dev, int offset, u32 val) { (void)dev; (void)offset; (void)val; return 0; }

#define PCI_DMA_BIDIRECTIONAL   0
#define PCI_DMA_TODEVICE        1
#define PCI_DMA_FROMDEVICE      2
#define PCI_DMA_NONE            3

static inline void *dma_alloc_coherent(struct device *dev, size_t sz, u64 *dma_handle, int gfp) {
    (void)dev; (void)gfp; void *p = kzalloc(sz, 0); if (dma_handle) *dma_handle = (u64)(unsigned long)p; return p;
}
static inline void dma_free_coherent(struct device *dev, size_t sz, void *cpu_addr, u64 dma_handle) {
    (void)dev; (void)sz; (void)dma_handle; kfree(cpu_addr);
}
static inline u64 dma_map_single(struct device *dev, void *cpu_addr, size_t sz, int dir) {
    (void)dev; (void)sz; (void)dir; return (u64)(unsigned long)cpu_addr;
}
static inline void dma_unmap_single(struct device *dev, u64 dma_addr, size_t sz, int dir) {
    (void)dev; (void)dma_addr; (void)sz; (void)dir;
}
static inline int dma_set_mask(struct device *dev, u64 mask) { (void)dev; (void)mask; return 0; }
static inline int dma_set_coherent_mask(struct device *dev, u64 mask) { (void)dev; (void)mask; return 0; }
#define DMA_BIT_MASK(n)         (((u64)1 << (n)) - 1)
#define DMA_MASK_NONE           0ULL

#define pci_alloc_consistent(d, s, h) dma_alloc_coherent(NULL, s, h, 0)
#define pci_free_consistent(d, s, c, h) dma_free_coherent(NULL, s, c, h)
#define pci_map_single(d, a, s, dir) dma_map_single(NULL, a, s, dir)
#define pci_unmap_single(d, a, s, dir) dma_unmap_single(NULL, a, s, dir)
#define pci_dma_sync_single_for_cpu(d, a, s, dir) do { } while (0)
#define pci_dma_sync_single_for_device(d, a, s, dir) do { } while (0)

typedef struct resource {
    u64 start, end;
    const char *name;
    unsigned long flags;
} resource_t;
#define IORESOURCE_MEM          0x00000200
#define IORESOURCE_IO           0x00000100
#define IORESOURCE_IRQ          0x00000400
#define IORESOURCE_PCI_FIXED    0x00010000
#define IORESOURCE_SIZEALIGN    0x00020000
#define IORESOURCE_STARTALIGN   0x00040000

#define PCI_STD_RESOURCES       0
#define PCI_STD_RESOURCE_END    5
#define PCI_ROM_RESOURCE        6

typedef struct pm_message { int event; } pm_message_t;
#define PM_EVENT_SUSPEND        0x0001
#define PM_EVENT_RESUME         0x0003
#define PM_EVENT_HIBERNATE      0x0004
#define PM_EVENT_QUIESCE        0x0008
#define PMSG_SUSPEND            ((pm_message_t){ .event = PM_EVENT_SUSPEND })
#define PMSG_RESUME             ((pm_message_t){ .event = PM_EVENT_RESUME })
#define PMSG_HIBERNATE          ((pm_message_t){ .event = PM_EVENT_HIBERNATE })
#define PMSG_AUTO_SUSPEND       ((pm_message_t){ .event = PM_EVENT_SUSPEND })

#define ENOIOCTLCMD             515
#define ECHILD                  10

#define THIS_MODULE             ((void *)0)
#define try_module_get(m)       1
#define module_put(m)           do { } while (0)

#define CLASS_CREATE(n)         ((void *)1)
#define CLASS_DESTROY(c)        do { } while (0)
#define device_create(c, p, n, d, f, a...) ((void *)1)
#define device_destroy(c, n)    do { } while (0)

#define __raw_readb(a)          readb(a)
#define __raw_readw(a)          readw(a)
#define __raw_readl(a)          readl(a)
#define __raw_readq(a)          readq(a)
#define __raw_writeb(v, a)      writeb(v, a)
#define __raw_writew(v, a)      writew(v, a)
#define __raw_writel(v, a)      writel(v, a)
#define __raw_writeq(v, a)      writeq(v, a)

#define ioread8(a)              readb(a)
#define ioread16(a)             readw(a)
#define ioread32(a)             readl(a)
#define iowrite8(v, a)          writeb(v, a)
#define iowrite16(v, a)         writew(v, a)
#define iowrite32(v, a)         writel(v, a)
#define ioread16be(a)           be16_to_cpu(readw(a))
#define ioread32be(a)           be32_to_cpu(readl(a))
#define iowrite16be(v, a)       writew(cpu_to_be16(v), a)
#define iowrite32be(v, a)       writel(cpu_to_be32(v), a)

#define ioremap(phys, sz)       ((void *)((unsigned long)(phys) + 0xFFFF800000000000ULL))
#define iounmap(addr)           do { } while (0)
#define ioremap_nocache(p, s)   ioremap(p, s)
#define ioremap_wc(p, s)        ioremap(p, s)

typedef struct { u8 b[16]; } guid_t;
typedef struct { u8 b[16]; } uuid_t;

#endif
