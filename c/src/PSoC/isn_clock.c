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
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * (c) Copyright 2019 - 2021, Isotel, http://isotel.eu
 */

#include "project.h"
#include "isn_clock.h"
#include "config.h"


/** \{ */

/*--------------------------------------------------------------------*/
/* PSoC5                                                              */
/*--------------------------------------------------------------------*/

#if (CYDEV_CHIP_FAMILY_USED == CYDEV_CHIP_FAMILY_PSOC5)

volatile const isn_clock_counter_t * const isn_clock_counter = ISN_CLOCK_1MHz_Counter_A0_PTR;

extern uint32_t clock_wakeups;

static void ISN_CLOCK_1MHz_Start() {}

CY_ISR_PROTO(wakeup_isr);
CY_ISR(wakeup_isr) {
    clock_wakeups++;
}

void isn_clock_init() {
    IRQ_ISN_CLOCK_StartEx(&wakeup_isr);  // Uses dummy interrupt handler provided in the IRQ_ISN_CLOCK.c, just used to wake-up the CPU
}

int isn_clock_wfi(isn_clock_counter_t until_time) {
    uint32_t state = CyEnterCriticalSection();
    if ( ISN_CLOCK_SINCE(until_time) < -ISN_CLOCK_us(5) ) {
        //clock_wakeups = -ISN_CLOCK_SINCE(until_time);
        *ISN_CLOCK_1MHz_Counter_D0_PTR = until_time;
        asm volatile("wfi");
    }
    CyExitCriticalSection(state);
    return 1;
}


/*--------------------------------------------------------------------*/
/* PSoC6                                                              */
/*--------------------------------------------------------------------*/

#elif (CYDEV_CHIP_FAMILY_USED == CYDEV_CHIP_FAMILY_PSOC6)

volatile const isn_clock_counter_t * const isn_clock_counter = &((TCPWM_V1_Type *)(ISN_CLOCK_1MHz_HW))->CNT[ISN_CLOCK_1MHz_CNT_NUM].COUNTER; //&clock_dummy;

void isn_clock_init() {
#ifdef IRQ_ISN_CLOCK__INTC_ASSIGNED
    void isr() {
        ISN_CLOCK_1MHz_ClearInterrupt( CY_TCPWM_INT_ON_CC_OR_TC );
    }

    Cy_SysInt_Init(&IRQ_ISN_CLOCK_cfg, isr);
    NVIC_EnableIRQ(IRQ_ISN_CLOCK_cfg.intrSrc);
#endif
}

/** \todo Possibly if ISR is not available and events are sched in time then loop forever,
 *     and only enter sleep mode if there are no timed events at all, as other events
 *     may anyway require some other source of interrupt to happen
 */
int isn_clock_wfi(isn_clock_counter_t until_time) {
    uint32_t state = CyEnterCriticalSection();
    int pending = 0;
    if ( ISN_CLOCK_SINCE(until_time) < -ISN_CLOCK_us(5) ) {
#ifdef IRQ_ISN_CLOCK__INTC_ASSIGNED
        ISN_CLOCK_1MHz_SetCompare0( until_time );
#else
#pragma message("IRQ_ISN_CLOCK interrupt not present on this core: isn_clock_wfi() works but without timeout")
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

void isn_clock_foreign_wakeup() {
    ISN_CLOCK_1MHz_SetInterrupt(CY_TCPWM_INT_ON_CC);
}

#else
# error "isn_clock unsupported platform"
#endif


/*--------------------------------------------------------------------*/
/* Common                                                             */
/*--------------------------------------------------------------------*/

void isn_clock_start() {
    ISN_CLOCK_1MHz_Start();
    isn_clock_init();
}

/** \} \endcond */
