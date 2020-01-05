/** \file
 *  \brief ISN UART Driver for PSoC4, PSoC5, and PSoC6
 *  \author Uros Platise <uros@isotel.eu>
 *  \see isn_uart.c
 *
 * \defgroup GR_ISN_PSoC_UART ISN Driver for PSoC UART
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
 * (c) Copyright 2019, Isotel, http://isotel.eu
 */

#ifndef __ISN_UART_H__
#define __ISN_UART_H__

#include "isn_def.h"

#define UART_TXBUF_SIZE  64
#define UART_RXBUF_SIZE  64

/** ISN Layer Driver */
typedef struct {
    /* ISN Abstract Class Driver */
    isn_driver_t drv;

    /* Private data */
    isn_driver_t* child_driver;
    uint8_t txbuf[UART_TXBUF_SIZE];
    uint8_t rxbuf[UART_RXBUF_SIZE];
    int buf_locked;
    size_t rx_size;
    size_t rx_dropped;
    size_t rx_counter;
    size_t rx_retry;
    size_t tx_counter;
}
isn_uart_t;

/*----------------------------------------------------------------------*/
/* Public functions                                                     */
/*----------------------------------------------------------------------*/

/** Polls for a new data received from PC and dispatch them
 * \returns number of bytes received or negative value of dropped bytes
 */
int isn_uart_poll(isn_uart_t *obj);

/** Collect new data to frames, which are then forwarded based on timing properties
 * \returns number of bytes received or negative value of dropped bytes
 */
int isn_uart_collect(isn_uart_t *obj, size_t maxsize, volatile uint32_t *counter, uint32_t timeout);

/** Initialize
 *
 * \param child use the next layer, like isn_frame
 */
void isn_uart_init(isn_uart_t *obj, isn_layer_t* child);

#endif
