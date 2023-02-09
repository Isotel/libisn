/** \file
 *  \brief Clock Implementation
 *  \author Uros Platise <uros@isotel.org>
 *  \see \ref GR_ISN_CLOCK, isn_clock.h
 */
/**
 * \ingroup GR_ISN_PSoC
 * \addtogroup GR_ISN_CLOCK
 *
 * \cond Implementation
 *
 * \section Implementation
 *
 * Clock runs at 120 MHz. If higher resolution than 1 us is needed,
 * provide faster clock and update the CLOCK_ms() etc macros.
 *
 * Transition to low power modes are also easily possible.
 */

#include "isn_clock.h"

volatile const isn_clock_counter_t * const isn_clock_counter;

/** \{ */

void isn_clock_init() {
    //  Full-width periodic up-count
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER3);
    TimerConfigure(TIMER3_BASE, TIMER_CFG_PERIODIC_UP);
    TimerEnable(TIMER3_BASE, TIMER_A);

    void **non_const = (void **)&isn_clock_counter;
    *(volatile isn_clock_counter_t **)non_const = &HWREG(TIMER3_BASE + TIMER_O_TAV);
}

void isn_clock_start() {
    isn_clock_init();
}

/** \todo Possibly if ISR is not available and events are sched in time then loop forever,
 *     and only enter sleep mode if there are no timed events at all, as other events
 *     may anyway require some other source of interrupt to happen
 */
int isn_clock_wfi(isn_clock_counter_t until_time) {
    return 0;
}

void isn_clock_foreign_wakeup() {
}

/** \} \endcond */
