/** \file
 *  \brief ISN USBUART Driver for PSoC4 and PSoC5 Implementation
 *  \author Uros Platise <uros@isotel.eu>
 *  \see isn_usbuart.h
 * 
 * \addtogroup GR_ISN_PSoC_USBUART
 * 
 * # Tested
 *
 *  - Families: PSoC4, PSoC5
 * 
 * \cond Implementation
 */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 * 
 * (c) Copyright 2019, Isotel, http://isotel.eu
 */

#include <string.h>
#include "project.h"
#include "PSoC/isn_usbuart.h"

/**\{ */

/**
 * Allocate buffer if buf is given, or just query for availability if buf is NULL
 * 
 * \returns desired or limited (max) size in the case desired size is too big
 */
static int isn_usbuart_getsendbuf(isn_layer_t *drv, void **dest, size_t size, const isn_layer_t *caller) {
    isn_usbuart_t *obj = (isn_usbuart_t *)drv;
    if (USBUART_CDCIsReady() && !obj->buf_locked) {
        if (dest) {
            obj->buf_locked = 1;
            *dest = obj->txbuf;
        }
        return (size > TXBUF_SIZE) ? TXBUF_SIZE : size;
    }
    if (dest) {
        *dest = NULL;
    }
    return -1;
}

static void isn_usbuart_free(isn_layer_t *drv, const void *ptr) {
    isn_usbuart_t *obj = (isn_usbuart_t *)drv;
    if (ptr == obj->txbuf) {
        obj->buf_locked = 0;             // we only support one buffer so we may free
    }
}

static int isn_usbuart_send(isn_layer_t *drv, void *dest, size_t size) {
    assert(size <= TXBUF_SIZE);
    if (size) {
        while( !USBUART_CDCIsReady() ); // todo: timeout assert
        USBUART_PutData(dest, size);
    }
    isn_usbuart_free(drv, dest);    // free buffer, however need to block use of buffer until sent out
    return size;
}

size_t isn_usbuart_poll(isn_usbuart_t *obj) {
    size_t size = 0;
    if (USBUART_DataIsReady()) {
        size = USBUART_GetCount()
        if ( (size + obj->rx_size) > UART_RXBUF_SIZE ) size = UART_RXBUF_SIZE - obj->rx_size;
        if ( size ) {
            USBUART_GetData(&obj->rxbuf[obj->rx_size], size);
            obj->rx_size += size;
        }
        else obj->rx_dropped++; // It hasn't been really dropped yet
    }
    if (obj->rx_size) {        
        size = obj->child_driver->recv(obj->child_driver, obj->rxbuf, obj->rx_size, &obj->drv);
        if (size < obj->rx_size) {
            obj->rx_retry++;    // Packet could not be fully accepted, retry next time
            memmove(obj->rxbuf, &obj->rxbuf[size], obj->rx_size - size);
            obj->rx_size -= size;
        }
        else obj->rx_size = 0;  // handles case if recv() returns size higher than rx_size
    }
    return size;
}

void isn_usbuart_init(isn_usbuart_t *obj, int mode, isn_layer_t* child) {
    obj->drv.getsendbuf = isn_usbuart_getsendbuf;
    obj->drv.send = isn_usbuart_send;
    obj->drv.recv = NULL;
    obj->drv.free = isn_usbuart_free;
    obj->child_driver = child;
    obj->buf_locked = 0;
    obj->rx_size = 0;
    obj->rx_retry = 0;
    obj->rx_dropped = 0;

    USBUART_Start(0, mode);
    while(0 == USBUART_GetConfiguration());
    USBUART_CDC_Init();
}

/** \} \endcond */
