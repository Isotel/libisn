#include "isn_clock.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

static isn_clock_counter_t clock_dum = 0;
volatile const isn_clock_counter_t * const isn_clock_counter = &clock_dum;

isn_clock_counter_t isn_clock_update(){
    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    clock_dum = current_time.tv_sec * (int)1e6 + current_time.tv_usec;
#ifdef ISN_CLOCK_DEBUG_TIME    
    printf("seconds: %ld, micro seconds: %ld, time: %ld\n", current_time.tv_sec, current_time.tv_usec, clock_dum);
#endif
    return clock_dum;
}

void isn_clock_init() { 
}

void isn_clock_start() { 
    isn_clock_init();
}

int isn_clock_wfi(isn_clock_counter_t until_time) { 
    return -1;
}

void isn_clock_foreign_wakeup() { 
}

