#ifndef ZIRVIUM_KERNEL_TIME_TIME_H
#define ZIRVIUM_KERNEL_TIME_TIME_H

#include <stdint.h>

struct datetime {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
};

void time_init(void);
int time_get(struct datetime *dt);
int time_set(const struct datetime *dt);
uint64_t time_uptime_seconds(void);

#endif
