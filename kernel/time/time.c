#include "time.h"
#include "arch/x64/cpu.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define CMOS_ADDR   0x70
#define CMOS_DATA   0x71

#define RTC_SECONDS   0x00
#define RTC_MINUTES   0x02
#define RTC_HOURS     0x04
#define RTC_DAY_WEEK  0x06
#define RTC_DAY_MONTH 0x07
#define RTC_MONTH     0x08
#define RTC_YEAR      0x09
#define RTC_STATUS_A  0x0A
#define RTC_STATUS_B  0x0B
#define RTC_STATUS_C  0x0C
#define RTC_STATUS_D  0x0D
#define RTC_CENTURY   0x32

#define RTC_A_UIP     (1 << 7)
#define RTC_B_SET     (1 << 7)
#define RTC_B_DM      (1 << 2)
#define RTC_B_24H     (1 << 1)
#define RTC_D_VRT     (1 << 7)

#define DEFAULT_YEAR   2005
#define DEFAULT_MONTH  11
#define DEFAULT_DAY    22
#define DEFAULT_HOUR   15
#define DEFAULT_MINUTE 36
#define DEFAULT_SECOND 0

static struct datetime boot_utc;
static uint64_t boot_tsc;
static uint64_t tsc_hz;
static int tz_offset_minutes;
static bool time_initialised = false;

static inline void cpuid(uint32_t leaf,
                         uint32_t *eax, uint32_t *ebx,
                         uint32_t *ecx, uint32_t *edx)
{
    __asm__ volatile("cpuid"
        : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
        : "a"(leaf));
}

static uint64_t detect_tsc_hz(void)
{
    uint32_t eax, ebx, ecx, edx;
    cpuid(0, &eax, &ebx, &ecx, &edx);
    if (eax >= 0x15) {
        cpuid(0x15, &eax, &ebx, &ecx, &edx);
        if (ecx != 0 && eax != 0)
            return (uint64_t)ecx * (uint64_t)ebx / (uint64_t)eax;
    }
    return 2000000000ULL;
}

static uint8_t cmos_read(uint8_t reg)
{
    outb(CMOS_ADDR, reg);
    return inb(CMOS_DATA);
}

static void cmos_write(uint8_t reg, uint8_t val)
{
    outb(CMOS_ADDR, reg);
    outb(CMOS_DATA, val);
}

static int bcd_to_bin(uint8_t bcd)
{
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

static uint8_t bin_to_bcd(int bin)
{
    return ((bin / 10) << 4) | (bin % 10);
}

static void rtc_wait_update(void)
{
    while (cmos_read(RTC_STATUS_A) & RTC_A_UIP);
}

static bool rtc_read_datetime(struct datetime *dt)
{
    uint8_t status_b = cmos_read(RTC_STATUS_B);
    bool is_bcd = !(status_b & RTC_B_DM);
    bool is_24h = (status_b & RTC_B_24H);

    rtc_wait_update();

    uint8_t sec  = cmos_read(RTC_SECONDS);
    uint8_t min  = cmos_read(RTC_MINUTES);
    uint8_t hour = cmos_read(RTC_HOURS);
    uint8_t day  = cmos_read(RTC_DAY_MONTH);
    uint8_t mon  = cmos_read(RTC_MONTH);
    uint8_t yr   = cmos_read(RTC_YEAR);
    uint8_t cen  = cmos_read(RTC_CENTURY);

    uint8_t status_d = cmos_read(RTC_STATUS_D);
    if (!(status_d & RTC_D_VRT))
        return false;

    if (is_bcd) {
        sec  = bcd_to_bin(sec);
        min  = bcd_to_bin(min);
        hour = bcd_to_bin(hour);
        day  = bcd_to_bin(day);
        mon  = bcd_to_bin(mon);
        yr   = bcd_to_bin(yr);
        cen  = bcd_to_bin(cen);
    }

    if (!is_24h) {
        bool pm = hour & 0x80;
        hour &= 0x7F;
        if (pm) hour += 12;
        if (hour == 12 && !pm) hour = 0;
    }

    if (sec > 59 || min > 59 || hour > 23 ||
        day > 31 || day < 1 || mon > 12 || mon < 1)
        return false;

    int year;
    if (cen >= 19)
        year = cen * 100 + yr;
    else
        year = 2000 + yr;

    if (year < 1970 || year > 2099)
        year = 2000 + yr;

    dt->year   = year;
    dt->month  = (int)mon;
    dt->day    = (int)day;
    dt->hour   = (int)hour;
    dt->minute = (int)min;
    dt->second = (int)sec;
    return true;
}

static bool rtc_write_datetime(const struct datetime *dt)
{
    if (dt->year < 1970 || dt->year > 2099) return false;
    if (dt->month < 1 || dt->month > 12) return false;
    if (dt->day < 1 || dt->day > 31) return false;
    if (dt->hour > 23 || dt->minute > 59 || dt->second > 59) return false;

    uint8_t status_b = cmos_read(RTC_STATUS_B);
    bool is_bcd = !(status_b & RTC_B_DM);

    int year_byte = dt->year % 100;
    int century = dt->year / 100;

    uint8_t sec  = is_bcd ? bin_to_bcd(dt->second) : dt->second;
    uint8_t min  = is_bcd ? bin_to_bcd(dt->minute) : dt->minute;
    uint8_t hour = is_bcd ? bin_to_bcd(dt->hour)   : dt->hour;
    uint8_t day  = is_bcd ? bin_to_bcd(dt->day)    : dt->day;
    uint8_t mon  = is_bcd ? bin_to_bcd(dt->month)  : dt->month;
    uint8_t yr   = is_bcd ? bin_to_bcd(year_byte)  : year_byte;
    uint8_t cen  = is_bcd ? bin_to_bcd(century)    : century;

    outb(CMOS_ADDR, RTC_STATUS_B);
    uint8_t prev = inb(CMOS_DATA);
    outb(CMOS_ADDR, RTC_STATUS_B | 0x80);
    outb(CMOS_DATA, prev | RTC_B_SET);

    cmos_write(RTC_SECONDS,    sec);
    cmos_write(RTC_MINUTES,    min);
    cmos_write(RTC_HOURS,      hour);
    cmos_write(RTC_DAY_MONTH,  day);
    cmos_write(RTC_MONTH,      mon);
    cmos_write(RTC_YEAR,       yr);
    cmos_write(RTC_CENTURY,    cen);

    outb(CMOS_ADDR, RTC_STATUS_B | 0x80);
    outb(CMOS_DATA, prev & ~RTC_B_SET);
    return true;
}

static bool is_leap_year(int year)
{
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

static int days_in_month(int year, int month)
{
    static const int dim[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (month < 1 || month > 12) return 0;
    if (month == 2 && is_leap_year(year)) return 29;
    return dim[month - 1];
}

static void normalize_datetime(struct datetime *dt)
{
    while (dt->second >= 60) { dt->second -= 60; dt->minute++; }
    while (dt->second < 0)   { dt->second += 60; dt->minute--; }
    while (dt->minute >= 60) { dt->minute -= 60; dt->hour++; }
    while (dt->minute < 0)   { dt->minute += 60; dt->hour--; }
    while (dt->hour >= 24)   { dt->hour -= 24;   dt->day++; }
    while (dt->hour < 0)     { dt->hour += 24;   dt->day--; }
    while (dt->day > days_in_month(dt->year, dt->month)) {
        dt->day -= days_in_month(dt->year, dt->month);
        dt->month++;
        if (dt->month > 12) { dt->month = 1; dt->year++; }
    }
    while (dt->day < 1) {
        dt->month--;
        if (dt->month < 1) { dt->month = 12; dt->year--; }
        dt->day += days_in_month(dt->year, dt->month);
    }
}

void time_init(void)
{
    if (time_initialised) return;
    time_initialised = true;

    tsc_hz = detect_tsc_hz();
    boot_tsc = rdtsc();
    tz_offset_minutes = 0;

    if (!rtc_read_datetime(&boot_utc)) {
        boot_utc.year   = DEFAULT_YEAR;
        boot_utc.month  = DEFAULT_MONTH;
        boot_utc.day    = DEFAULT_DAY;
        boot_utc.hour   = DEFAULT_HOUR;
        boot_utc.minute = DEFAULT_MINUTE;
        boot_utc.second = DEFAULT_SECOND;
    }
}

int time_get(struct datetime *dt)
{
    if (!time_initialised) return -1;

    uint64_t elapsed = 0;
    uint64_t now_tsc = rdtsc();
    if (now_tsc > boot_tsc)
        elapsed = (now_tsc - boot_tsc) / tsc_hz;

    struct datetime utc = boot_utc;
    utc.second  += (int)(elapsed % 60); elapsed /= 60;
    utc.minute  += (int)(elapsed % 60); elapsed /= 60;
    utc.hour    += (int)(elapsed % 24); elapsed /= 24;
    utc.day     += (int)elapsed;
    normalize_datetime(&utc);

    *dt = utc;
    dt->minute += tz_offset_minutes % 60;
    dt->hour   += tz_offset_minutes / 60;
    normalize_datetime(dt);
    return 0;
}

int time_set(const struct datetime *dt)
{
    if (!time_initialised) return -1;
    if (dt->year < 1970 || dt->year > 2099) return -1;
    if (dt->month < 1 || dt->month > 12) return -1;
    if (dt->day < 1 || dt->day > 31) return -1;
    if (dt->hour > 23 || dt->minute > 59 || dt->second > 59) return -1;

    struct datetime utc = *dt;
    utc.minute -= tz_offset_minutes % 60;
    utc.hour   -= tz_offset_minutes / 60;
    normalize_datetime(&utc);

    boot_utc = utc;
    boot_tsc = rdtsc();

    rtc_write_datetime(&utc);
    return 0;
}

uint64_t time_uptime_seconds(void)
{
    if (!time_initialised) return 0;
    uint64_t now_tsc = rdtsc();
    if (now_tsc > boot_tsc)
        return (now_tsc - boot_tsc) / tsc_hz;
    return 0;
}

int time_get_tz(void)
{
    return tz_offset_minutes;
}

void time_set_tz(int offset_minutes)
{
    tz_offset_minutes = offset_minutes;
}
