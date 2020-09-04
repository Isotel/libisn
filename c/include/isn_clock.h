/** \file
 *  \brief Clock
 *  \author Uros Platise <uros@isotel.eu>
 *  \see \ref GR_ISN_CLOCK, isn_clock.c
 *
 * \defgroup GR_ISN_CLOCK Base System Clock
 *
 * # Scope
 *
 * Provides basic 32-bit free running counter.
 *
 * Requirements:
 *  - Module requires 32-bit TCPWM
 */

#ifndef __ISN_CLOCK_H__
#define __ISN_CLOCK_H__

#include <project.h>

/** \{ */

/*----------------------------------------------------------------------*/
/* DEFINITIONS                                                          */
/*----------------------------------------------------------------------*/

#define ISN_CLOCK_SINCE(T)      ((int32_t)isn_clock_counter - (int32_t)(T))
#define ISN_CLOCK_us(T)         (T)
#define ISN_CLOCK_ms(T)         (1000*(T))
#define ISN_CLOCK_s(T)          (1000000*(T))
#define ISN_CLOCK_NOW           *isn_clock_counter

typedef uint32_t isn_clock_counter_t;

extern volatile const isn_clock_counter_t *isn_clock_counter;    ///< Pointer to 32-bit free running clock counter

/*----------------------------------------------------------------------*/
/* Public functions                                                     */
/*----------------------------------------------------------------------*/

/** Initialize data structure, called automatically by the isn_clock_start(),
 *  however on multi-cores, only one core should call isn_clock_start() and
 *  the other isn_clock_init() only.
 */
void isn_clock_init();

/** Starts Clock and does not use any interrupt */
void isn_clock_start();

/** \} */

#endif
