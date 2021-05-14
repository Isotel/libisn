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
 * Clock runs at 1 MHz. If higher resolution than 1 us is needed,
 * provide faster clock and update the CLOCK_ms() etc macros.
 *
 * Transition to low power modes are also easily possible.
 */

#include "project.h"
#include "isn_clock.h"
#include "config.h"

volatile const isn_clock_counter_t *isn_clock_counter;

/** \{ */

void isn_clock_init() {
#ifdef ClockInt__INTC_ASSIGNED
    void isr() {
        CLOCK_ClearInterrupt( CY_TCPWM_INT_ON_CC_OR_TC );
    }

    Cy_SysInt_Init(&ClockInt_cfg, isr);
    NVIC_EnableIRQ(ClockInt_cfg.intrSrc);
#endif

    isn_clock_counter = &TCPWM_CNT_COUNTER(CLOCK_HW, CLOCK_CNT_NUM);
}

void isn_clock_start() {
    isn_clock_init();
    CLOCK_Start();
}

int isn_clock_wfi(isn_clock_counter_t until_time) {
    uint32_t state = CyEnterCriticalSection();
    int pending = 0;
    if ( ISN_CLOCK_SINCE(until_time) < -ISN_CLOCK_us(5) ) {
#ifdef ClockInt__INTC_ASSIGNED
        CLOCK_SetCompare0( until_time );
#else
#pragma message("Clock interrupt not enabled: isn_clock_wfi() works but without timeout")
#endif
        if(CY_SYSPM_SUCCESS != Cy_SysPm_CpuEnterSleep(CY_SYSPM_WAIT_FOR_INTERRUPT)) {
            pending = -1;
        }
        else {
            pending = 1;
        }
    }
    Cy_SysLib_ExitCriticalSection(state);
    return pending;
}

/** \} \endcond */
