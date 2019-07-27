#ifndef TIME_UTILS_H
#define TIME_UTILS_H

#include <time.h>

typedef int64_t time_ms_t;

static time_ms_t monotonic_ms_time() {
    struct timespec tm;
    if (clock_gettime(CLOCK_MONOTONIC, &tm) != 0) {
        return -1;
    }
    return tm.tv_sec * 1000 + tm.tv_nsec / 1000000;
}

#endif //TIME_UTILS_H
