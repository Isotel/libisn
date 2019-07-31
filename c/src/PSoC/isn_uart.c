/** \file
 *  \author Uros Platise <uros@isotel.eu>
 *  \see isn_uart.c
 * 
 * Tested on CY8CKIT-062-BLE.
 */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 * 
 * (c) Copyright 2019, Isotel, http://isotel.eu
 */


#include "project.h"
#include "PSoC/isn_uart.h"

/** PSoC 4100PS uart functions maping*/
#if(CYDEV_CHIP_FAMILY_USED == CYDEV_CHIP_FAMILY_PSOC4)
    #define UART_GetNumInTxFifo()       UART_SpiUartGetTxBufferSize()
    #define UART_PutArray(dest, size)   UART_SpiUartPutArray(dest, size)
    #define UART_GetNumInRxFifo()       UART_SpiUartGetRxBufferSize()


extern uint16_t test;    
    
    static void UART_GetArray(void *buffer, uint32_t size) {
        uint8_t *buf = (uint8_t *) buffer;   
        for (uint8_t i=0; i<size; i++) {
            buf[i] = (uint8_t)UART_SpiUartReadRxData();
        }
    }
#endif

static int UART_TX_is_ready(size_t size) {
    return ((UART_UART_TX_BUFFER_SIZE - UART_GetNumInTxFifo()) > size) ? 1 : 0;
}

/**
 * Allocate buffer if buf is given, or just query for availability if buf is NULL
 * 
 * \returns desired or limited (max) size in the case desired size is too big
 */
static int isn_uart_getsendbuf(isn_layer_t *drv, void **dest, size_t size) {
    isn_uart_t *obj = (isn_uart_t *)drv;
    if (UART_TX_is_ready(size) && !obj->buf_locked) {
        if (dest) {
            obj->buf_locked = 1;
            *dest = obj->txbuf;
        }
        return (size > UART_TXBUF_SIZE) ? UART_TXBUF_SIZE : size;
    }
    if (dest) {
        *dest = NULL;
    }
    return -1;
}

static void isn_uart_free(isn_layer_t *drv, const void *ptr) {
    isn_uart_t *obj = (isn_uart_t *)drv;
    if (ptr == obj->txbuf) {
        obj->buf_locked = 0;             // we only support one buffer so we may free
    }
}

static int isn_uart_send(isn_layer_t *drv, void *dest, size_t size) {
    assert(size <= TXBUF_SIZE);
    while( !UART_TX_is_ready(size) );   // todo: timeout assert
    UART_PutArray(dest, size);
    isn_uart_free(drv, dest);           // free buffer, however need to block use of buffer until sent out
    return size;
}

size_t isn_uart_poll(isn_uart_t *obj) {
    size_t size = 0;
    if (UART_GetNumInRxFifo()) {
        UART_GetArray(obj->rxbuf, size = UART_GetNumInRxFifo());    // Fetch data even if size = 0 to reinit the OUT EP
        if (size) {
            assert2(obj->child_driver->recv(obj->child_driver, obj->rxbuf, size, &obj->drv) == obj->rxbuf);
        }
    }
    return size;
}

void isn_uart_init(isn_uart_t *obj, isn_layer_t* child) {
    obj->drv.getsendbuf = isn_uart_getsendbuf;
    obj->drv.send = isn_uart_send;
    obj->drv.recv = NULL;
    obj->drv.free = isn_uart_free;
    obj->child_driver = child;
    obj->buf_locked = 0;

    UART_Start();
}
