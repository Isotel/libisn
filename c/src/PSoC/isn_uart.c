/** \file
 * Tested on CY8CKIT-062-BLE.
 */

#include "project.h"
#include "PSoC/isn_uart.h"

static int UART_TX_is_ready(size_t size) {
    return ((TXFIFO_SIZE - UART_GetNumInTxFifo()) > size) ? 1 : 0;
}

/**
 * Allocate buffer if buf is given, or just query for availability if buf is NULL
 * 
 * \returns desired or limited (max) size in the case desired size is too big
 */
int isn_uart_getsendbuf(isn_layer_t *drv, uint8_t **buf, size_t size) {
    isn_uart_t *obj = (isn_uart_t *)drv;
    if (UART_TX_is_ready(size) && !obj->buf_locked) {
        if (buf) {
            obj->buf_locked = 1;
            *buf = obj->txbuf;
        }
        return (size > TXBUF_SIZE) ? TXBUF_SIZE : size;
    }
    if (buf) {
        *buf = NULL;
    }
    return -1;
}

void isn_uart_free(isn_layer_t *drv, const uint8_t *buf) {
    isn_uart_t *obj = (isn_uart_t *)drv;
    if (buf == obj->txbuf) {
        obj->buf_locked = 0;             // we only support one buffer so we may free
    }
}

int isn_uart_send(isn_layer_t *drv, uint8_t *buf, size_t size) {
    assert(size <= TXBUF_SIZE);
    while( !UART_TX_is_ready(size) ); // todo: timeout assert
    UART_PutArray(buf, size);
    isn_uart_free(drv, buf);          // free buffer, however need to block use of buffer until sent out
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
