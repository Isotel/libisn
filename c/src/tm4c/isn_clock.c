#include "isn_clock.h"

volatile const isn_clock_counter_t * const isn_clock_counter;

/** \{ */

void isn_clock_init() {
    void **non_const = (void **)&isn_clock_counter;
    *(volatile isn_clock_counter_t **)non_const = &HWREG(TIMER3_BASE + TIMER_O_TAV);
}

void isn_clock_start() {
    isn_clock_init();
}

int isn_clock_wfi(isn_clock_counter_t until_time) {
    return 0;
}

void isn_clock_foreign_wakeup() {
}

/** \} \endcond */
