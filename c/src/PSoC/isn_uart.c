/** \file
 *  \brief ISN UART Driver for PSoC4, PSoC5, and PSoC6 Implementation
 *  \author Uros Platise <uros@isotel.eu>, Tomaz Kanalec <tomaz@isotel.eu>
 *  \see isn_uart.h
 *
 * \addtogroup GR_ISN_PSoC_UART
 *
 * # Tested
 *
 *  - Families: PSoC4, PSoC5, PSoC6
 *  - Kits: CY8CKIT-062-BLE
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
#include "PSoC/isn_uart.h"

/**\{ */

#if(CYDEV_CHIP_FAMILY_USED == CYDEV_CHIP_FAMILY_PSOC4)
    #define UART_GetNumInTxFifo()       UART_SpiUartGetTxBufferSize()
    #define UART_PutArray(dest, size)   UART_SpiUartPutArray(dest, size)
    #define UART_GetNumInRxFifo()       UART_SpiUartGetRxBufferSize()

    static void UART_GetArray(void *buffer, uint32_t size) {
        uint8_t *buf = (uint8_t *) buffer;
        for (uint8_t i=0; i<size; i++) {
            *buf++ = (uint8_t)UART_SpiUartReadRxData();
        }
    }
#endif

#if(CYDEV_CHIP_FAMILY_USED == CYDEV_CHIP_FAMILY_PSOC5)
    #define UART_GetNumInRxFifo()       UART_GetRxBufferSize()
    #define UART_GetNumInTxFifo()       UART_GetTxBufferSize()

    static void UART_GetArray(void *buffer, uint32_t size) {
        uint8_t *buf = (uint8_t *) buffer;
        for (uint8_t i=0; i<size; i++) {
            *buf++ = (uint8_t)UART_ReadRxData();
        }
    }
#endif

#if(CYDEV_CHIP_FAMILY_USED == CYDEV_CHIP_FAMILY_PSOC6)
#define UART_TX_BUFFER_SIZE     128
#endif

static int UART_TX_is_ready(size_t size) {
#if (UART_TX_BUFFER_SIZE >= 64)
    return ((UART_TX_BUFFER_SIZE - UART_GetNumInTxFifo()) > size) ? 1 : 0;
#else
#warning UART may stall CPU because buffer size is less than 64 B
    return 1;
#endif
}

/**
 * Allocate buffer if buf is given, or just query for availability if buf is NULL
 *
 * \returns desired or limited (max) size in the case desired size is too big
 */
static int isn_uart_getsendbuf(isn_layer_t *drv, void **dest, size_t size, const isn_layer_t *caller) {
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
    isn_uart_t *obj = (isn_uart_t *)drv;
    assert(size <= UART_TXBUF_SIZE);
    if (size) {
        while( !UART_TX_is_ready(size) );   // todo: timeout assert
        UART_PutArray(dest, size);
        obj->tx_counter += size;
    }
    isn_uart_free(drv, dest);           // free buffer, however need to block use of buffer until sent out
    return size;
}

/**
 * Check if there are new bytes pending in the buffer, and collect them in the RX buffer.
 * In the next step try to forward data as long they're not accepted by the receiver.
 */
int isn_uart_poll(isn_uart_t *obj) {
    size_t size = UART_GetNumInRxFifo();
    if (size) {
        if ( (size + obj->rx_size) > UART_RXBUF_SIZE ) size = UART_RXBUF_SIZE - obj->rx_size;
        if ( size ) {
            UART_GetArray(&obj->rxbuf[obj->rx_size], size);
            obj->rx_size += size;
            obj->rx_counter += size;
        }
        else obj->rx_dropped++; // It hasn't been really dropped yet
    }
    if (obj->rx_size) {
        size = obj->child_driver->recv(obj->child_driver, obj->rxbuf, obj->rx_size, &obj->drv);
        if (size < obj->rx_size) {
            obj->rx_retry++;    // Packet could not be fully accepted, retry next time
            memmove(obj->rxbuf, &obj->rxbuf[size], obj->rx_size - size);
        }
        obj->rx_size -= size;
    }
    return size;
}

int isn_uart_collect(isn_uart_t *obj, size_t maxsize, volatile uint32_t *counter, uint32_t timeout) {
    static uint32_t ts = 0;
    size_t size = UART_GetNumInRxFifo();
    if (size) {
        if ( (size + obj->rx_size) > UART_RXBUF_SIZE ) size = UART_RXBUF_SIZE - obj->rx_size;
        if ( size ) {
            UART_GetArray(&obj->rxbuf[obj->rx_size], size);
            obj->rx_size += size;
            obj->rx_counter += size;
            ts = *counter;
        }
        else obj->rx_dropped++; // It hasn't been really dropped yet \todo Read overflow from low-level driver
    }
    if (obj->rx_size >= maxsize || ((*counter - ts) > timeout && obj->rx_size > 0)) {
        size = obj->child_driver->recv(obj->child_driver, obj->rxbuf, obj->rx_size > maxsize ? maxsize : obj->rx_size, &obj->drv);
        if (size < obj->rx_size) {
            obj->rx_retry++;    // Packet could not be fully accepted, retry next time
            memmove(obj->rxbuf, &obj->rxbuf[size], obj->rx_size - size);
        }
        obj->rx_size -= size;
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
    obj->rx_size    = 0;
    obj->rx_dropped = 0;
    obj->rx_counter = 0;
    obj->tx_counter = 0;
    UART_Start();
}

/** \} \endcond */
