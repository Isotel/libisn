/** \file
 *  \brief ISN UART Driver for PSoC4, PSoC5, and PSoC6
 *  \author Uros Platise <uros@isotel.org>
 *  \see isn_uart.c
 *
 * \defgroup GR_ISN_PSoC_UART PSoC UART Driver
 *
 * # Scope
 *
 * Tiny implementation of the ISN Device Driver for the
 * Cypress PSoC4, PSoC5 and PSoC6 UART and supports non-blocking and blocking mode.
 *
 * # Usage
 *
 * Place UART component in the PSoC Creator 4.2 and name it UART only.
 *
 * - if the TX buffer is below 64 bytes, device may operate in blocking mode, if
 *   desired packet cannot fit the hardware fifo,
 * - otherwise device driver operates in non-blocking mode.
 *
 */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * (c) Copyright 2019, Isotel, http://isotel.org
 */

#ifndef __ISN_UART_H__
#define __ISN_UART_H__

#include "isn_reactor.h"

/** ISN Layer Driver */
typedef struct {
    /* ISN Abstract Class Driver */
    isn_driver_t drv;

    /* Private data */
    isn_driver_t* child_driver;
    int buf_locked;
}
isn_uart_t;

/*----------------------------------------------------------------------*/
/* Public functions                                                     */
/*----------------------------------------------------------------------*/

/** Polling based data fetch
 *
 *  Collect new data to frames, which are then forwarded based on timing properties.
 *
 * \param maxsize defines maximum forwarding packet capacity
 * \param frame timeout, measured from the lastly received byte
 * \returns returns remaining bytes in the FIFO
 */
int isn_uart_collect(isn_uart_t *obj, size_t maxsize, uint32_t timeout);

/** Enable Reactor
 *
 * \param receive_threshold triggers a receive call whenever fifo has received this amount of bytes
 * \param timeout trigger a receive call since the last byte received, defined by the frame_timeout
 */
void isn_uart_radiate(isn_uart_t *obj, size_t receive_threshold, isn_clock_counter_t timeout, isn_reactor_queue_t priority_queue,
                      isn_reactor_mutex_t busy_mutex, isn_reactor_mutex_t holdon_mutex);

/** Modify the priority queue
 *
 * \param priority_queue, reactor queue or NULL to disable
 * \returns previous queue
 */
isn_reactor_queue_t isn_uart_radiate_setqueue(isn_reactor_queue_t priority_queue);

/* Low-level functions */

/** \return Size of pending receive buffer to be collected by the isn_uart_collect() or
 *          automatically triggered by the event if isn_uart_radiate() function initialized it.
 */
size_t isn_uart_getrecvsize();

/** \return next received byte or -1 if non is available directly from the receive fifo
 *          buffer. isn_uart_collect() will therefore not collect manually taken bytes.
 */
int isn_uart_getrecvbyte();

/** Initialize
 *
 * \param obj
 * \param child use the next layer, like isn_frame
 */
void isn_uart_init(isn_uart_t *obj, isn_layer_t* child);

#endif
