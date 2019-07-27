/** \file
 *  \author Uros Platise <uros@isotel.eu>
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

#include "isn.h"
    
#define TXFIFO_SIZE  128
#define RXFIFO_SIZE  128   

#define TXBUF_SIZE  64
#define RXBUF_SIZE  64

/** ISN Layer Driver */
typedef struct {
    /* ISN Abstract Class Driver */
    isn_driver_t drv;

    /* Private data */
    isn_driver_t* child_driver;
    uint8_t txbuf[TXBUF_SIZE];
    uint8_t rxbuf[RXBUF_SIZE];
    int buf_locked;
}
isn_uart_t;

/*----------------------------------------------------------------------*/
/* Public functions                                                     */
/*----------------------------------------------------------------------*/

/** Polls for a new data received from PC and dispatch them 
 * \returns number of bytes received
 */
size_t isn_uart_poll(isn_uart_t *obj);

/** Initialize
 * 
 * \param child use the next layer, like isn_frame
 */
void isn_uart_init(isn_uart_t *obj, isn_layer_t* child);

#endif
