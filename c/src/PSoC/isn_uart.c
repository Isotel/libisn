/** \file
 *  \brief ISN UART Driver for PSoC4, PSoC5, and PSoC6 Implementation
 *  \author Uros Platise <uros@isotel.org>, Tomaz Kanalec <tomaz@isotel.org>
 *  \see isn_uart.h
 */
/**
 * \ingroup GR_ISN_PSoC
 * \addtogroup GR_ISN_PSoC_UART
 *
 * # Tested
 *
 *  - Families: PSoC4, PSoC5, PSoC6
 *  - Kits: CY8CKIT-062-BLE
 *
 * \cond Implementation
 * 
 * This is a provisory driver and needs major redesign:
 * 
 * - use of interrupts (for <100 kbps there is no need for DMA)
 * - import circular, variable len, buffers from uart_udb
 * - add radioactivity
 */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * (c) Copyright 2019, Isotel, http://isotel.org
 */

#include <string.h>
#include "project.h"
#include "config.h"
#include "isn_clock.h"
#include "PSoC/isn_uart.h"

#define RX_FIFO_SIZE    64
#define TX_FIFO_SIZE    64

static uint8_t rx_buf[RX_FIFO_SIZE];
static uint8_t rx_size;

static uint8_t tx_buf[TX_FIFO_SIZE];

/**\{ */

#define UART_TIMEOUT        ISN_CLOCK_ms(100)

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
            *dest = tx_buf;
        }
        return (size > TX_FIFO_SIZE) ? TX_FIFO_SIZE : size;
    }
    if (dest) {
        *dest = NULL;
    }
    return -1;
}

static void isn_uart_free(isn_layer_t *drv, const void *ptr) {
    isn_uart_t *obj = (isn_uart_t *)drv;
    if (ptr == tx_buf) {
        obj->buf_locked = 0;             // we only support one buffer so we may free
    }
}

static int isn_uart_send(isn_layer_t *drv, void *dest, size_t size) {
    isn_uart_t *obj = (isn_uart_t *)drv;
    assert(size <= TX_FIFO_SIZE);
    if (size) {
        ASSERT_UNTIL( UART_TX_is_ready(size), UART_TIMEOUT );
        UART_PutArray(dest, size);
        obj->drv.stats.tx_counter += size;
        obj->drv.stats.tx_packets++;
    }
    isn_uart_free(drv, dest);           // free buffer, however need to block use of buffer until sent out
    return size;
}

/**
 * Check if there are new bytes pending in the buffer, and collect them in the RX buffer.
 * In the next step try to forward data as long they're not accepted by the receiver.
 */
int isn_uart_collect(isn_uart_t *obj, size_t maxsize, uint32_t timeout) {
    static uint32_t ts = 0;
    size_t size = UART_GetNumInRxFifo();
    if (size) {
        if ( (size + rx_size) > RX_FIFO_SIZE ) size = RX_FIFO_SIZE - rx_size;
        if ( size ) {
            UART_GetArray(&rx_buf[rx_size], size);
            rx_size += size;
            obj->drv.stats.rx_counter += size;
            ts = isn_clock_now();
        }
        else obj->drv.stats.rx_dropped++; // It hasn't been really dropped yet \todo Read overflow from low-level driver
    }
    if (rx_size >= maxsize || ((isn_clock_now() - ts) > timeout && rx_size > 0)) {
        size = obj->child_driver->recv(obj->child_driver, rx_buf, rx_size > maxsize ? maxsize : rx_size, obj);
        if (size) obj->drv.stats.rx_packets++;
        if (size < rx_size) {
            obj->drv.stats.rx_retries++;    // Packet could not be fully accepted, retry next time
            memmove(rx_buf, &rx_buf[size], rx_size - size);
            rx_size -= size;
        }
        else rx_size = 0;  // handles case if recv() returns size higher than rx_size
    }
    return size;
}

int isn_uart_poll(isn_uart_t *obj) {
    return isn_uart_collect(obj, 1, 0);
}

void isn_uart_init(isn_uart_t *obj, isn_layer_t* child) {
    ASSERT(obj);
    ASSERT(child);
    memset(&obj->drv, 0, sizeof(obj->drv));
    obj->drv.getsendbuf = isn_uart_getsendbuf;
    obj->drv.send = isn_uart_send;
    obj->drv.recv = NULL;
    obj->drv.free = isn_uart_free;
    obj->child_driver = child;
    obj->buf_locked = 0;
    rx_size    = 0;
    UART_Start();
}

/** \} \endcond */
