/** \file
 *  \brief Clock
 *  \author Uros Platise <uros@isotel.org>
 *  \see \ref GR_ISN_CLOCK, isn_clock.c
 */

/**
 * \ingroup GR_ISN
 * \defgroup GR_ISN_CLOCK Base System Clock
 *
 * # Scope
 *
 * Provides basic 32-bit free running counter with basic API for
 * a tick-less interrupt firmware architecture.
 *
 * Requirements:
 *  - Module requires 32-bit TCPWM
 */

#ifndef __ISN_CLOCK_H__
#define __ISN_CLOCK_H__

#include "isn_def.h"

/** \{ */

typedef uint32_t isn_clock_counter_t;

/** Pointer to 32-bit free running clock counter, could be replaced by direct register address */
extern volatile const isn_clock_counter_t * const isn_clock_counter;

/*----------------------------------------------------------------------*/
/* DEFINITIONS                                                          */
/*----------------------------------------------------------------------*/

#ifdef CONFIG_CLOCK_120MHz
# define ISN_CLOCK_us(T)        (120*(T))
# define ISN_CLOCK_ms(T)        (120000L*(T))
# define ISN_CLOCK_s(T)         (120000000L*(T))
#else
# define ISN_CLOCK_ticks(T)     (T)
# define ISN_CLOCK_us(T)        (T)
# define ISN_CLOCK_ms(T)        (1000*(T))
# define ISN_CLOCK_s(T)         (1000000*(T))
#endif

#define ISN_CLOCK_NOW           (*isn_clock_counter)
#define ISN_CLOCK_SINCE(T)      ((int32_t)*isn_clock_counter - (int32_t)(T))

/*----------------------------------------------------------------------*/
/* Public functions                                                     */
/*----------------------------------------------------------------------*/

/** A helper macro, works like while(condition) but limited with a timeout */
#define until(condition,timeout) for(isn_clock_counter_t Ts=ISN_CLOCK_NOW; !(condition) && ISN_CLOCK_SINCE(Ts) < timeout;)

static inline isn_clock_counter_t isn_clock_now() {
    return ISN_CLOCK_NOW;
}

/** A helper function to find out the diff between two timestamps, works only if distance is less than a half
 * \returns clock difference (a - b), handles overflows properly
 */
static inline int32_t isn_clock_diff(isn_clock_counter_t a, isn_clock_counter_t b) { 
    return (int32_t)(a - b);
};

/** Time elapsed since some timestamp, works only if distance is less than a half
 * \param since timestamp
 * \returns clock difference from (now - timestamp), handles overflows properly
 */
static inline int32_t isn_clock_elapsed(isn_clock_counter_t since) {
    return isn_clock_diff(ISN_CLOCK_NOW, since);
}

static inline int32_t isn_clock_remains(isn_clock_counter_t until) {
    return isn_clock_diff(until, ISN_CLOCK_NOW);
}

/** Initialize data structure, called automatically by the isn_clock_start(),
 *  however on multi-cores, only one core should call isn_clock_start() and
 *  the other isn_clock_init() only.
 */
void isn_clock_init();

/** Starts Clock and does not use any interrupt */
void isn_clock_start();

/** Wait for interrupt until some time
 *  \return -1 if The CPU Sleep mode was not entered because a registered sleep "check ready" callback returned a "not success" status
 *  \return 0 if due to less than some time it made no sense to enter the sleep
 *  \return 1 the CPU has just woken up from the CPU Sleep mode
 */
int isn_clock_wfi(isn_clock_counter_t until_time);

/** Handler that may be called by other processor to break the isn_clock_wfi() and return immediately */
void isn_clock_foreign_wakeup();

/** Manually updates the clock counter. \returns Current (latest) counter value. */
isn_clock_counter_t isn_clock_update();

/** \} */

#endif
