/** \file
 *  \brief Clock Implementation
 *  \author Uros Platise <uros@isotel.eu>
 *  \see \ref GR_ISN_CLOCK, isn_clock.h
 *
 * \addtogroup GR_ISN_CLOCK
 *
 * \cond Implementation
 *
 * \section Implementation
 *
 * Clock runs at 1 MHz. If higher resolution than 1 us is needed,
 * provide faster clock and update the CLOCK_ms() etc macros.
 * 
 * Transition to low power modes are also easily possible.
 */

#include <project.h>
#include "isn_clock.h"
#include "config.h"

volatile const isn_clock_counter_t *isn_clock_counter;

/** \{ */

void isn_clock_init() {
    isn_clock_counter = &TCPWM_CNT_COUNTER(CLOCK_HW, CLOCK_CNT_NUM);
}

void isn_clock_start() {
    isn_clock_init();
    CLOCK_Start();    
}

/** \} \endcond */
