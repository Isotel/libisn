/** \file
 *  \brief ISN UART UDB Driver for PSoC6 Implementation
 *  \author Uros Platise <uros@isotel.eu>, Tomaz Kanalec <tomaz@isotel.eu>
 *  \see isn_uart.h
 *
 * \addtogroup GR_ISN_PSoC_UART
 *
 * \cond Implementation
 *
 * \section Features
 *
 * - UART UDB driver with 4 B UDB FIFO
 * - Zero-Copy Rx and Tx 256 B Circular Buffers, getsendbuf() returns
 *   a pointer directed to a transmit circular buffer, and recv()
 *   forwards data directly from receive FIFO
 * - Tested on PSoC6
 *
 * \section Description
 *
 * Uses 256 bytes long RX and TX FIFO buffers. With zero-copy approach
 * rx buffer is forward in single or two pieces, when content wrapped,
 * and tx buffer finds a linear non-wrapped segment of the required
 * or max available size.
 *
 * UDB block has only 4 B depth, so Rx interrupt is triggered on each
 * received byte and Tx interrupt when fifo is empty, to keep the
 * transfer as smooth as possible.
 *
 * Receiver buffer spawns an event whenever number of bytes received
 * is above the threshold, and on timeout since the last byte received.
 *
 * Size 256 was chosen to simplify circular buffers. Increasing the
 * size requires code modifications.
 *
 * The transmit buffer will under all conditions always, when data
 * are sent, provide a buffer of at least 128 B, an important condition
 * which eliminates the need for fragmentation.
 *
 * \section Support for fixed configuration 8E1 UDB implementation
 *
 * Recently support was added for resource limited fixed configuration
 * implementation of UART 8E1 with 4-B Rx and Tx buffers.
 * Implementation uses 7-bit down-counters to represent current
 * state of the buffers, and uses rising edge tx interrupt to
 * avoid re-triggering and simplifies code, and level rx interrupt
 * to be sure all rx bytes have been read.
 */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * (c) Copyright 2019 - 2021, Isotel, http://isotel.eu
 */

#include <project.h>
#include <string.h>
#include "isn_reactor.h"
#include "PSoC/isn_uart.h"

#define RX_FIFO_SIZE    256             ///< Interrupt and Zero-Copy Receive Buffer, this size of 256 simplifes circular buffers with 8-bit integers
#define TX_FIFO_SIZE    256             ///< Interrupt and Zero-Copy Transmit Buffer, this size of 256 simplifes circular buffers with 8-bit integers

static uint8_t rx_buf[RX_FIFO_SIZE];
static volatile uint8_t rxb_wri = 0;    ///< Write index, followed by rxb_rdi, wri must never be incremented over rdi, effective buffer size is thus -1
static volatile uint8_t rxb_rdi = 0;    ///< when rxb_rdi == rxb_wri, buffer is empty
static volatile uint32_t rxb_err = 0;   ///< Counts number of errorneous uart bytes
static volatile uint32_t rxb_dropped =0;///< Counts number of dropped uart bytes
static volatile isn_clock_counter_t ts = 0;

static uint8_t tx_buf[TX_FIFO_SIZE];
static volatile uint8_t txb_wri = 0;    ///< Write index, followed by txb_rdi, wri must never be incremented over rdi, effective buffer size is thus -1
static volatile uint8_t txb_rdi = 0;    ///< when txb_rdi == txb_wri, buffer is empty
static volatile uint8_t txb_wrw = 255;  ///< Dynamic Wrap point to simulate linear buffers, at which point rd/wr index wrap
static uint8_t txb_alloc_wrw = 255, txb_alloc_wri = 255, txb_alloc_size = 0;    // invalidate, so free cannot work

static volatile isn_reactor_queue_t queue;
static volatile int uart_imm_trigger = ISN_REACTOR_TASKLET_INVALID, uart_timeout_trigger = ISN_REACTOR_TASKLET_INVALID;
static isn_reactor_mutex_t hmutex;
static isn_clock_counter_t delay;
static size_t recv_thr = 1;
static isn_uart_t *this;

#if defined(UART_RX8E1_DESERIALIZER_F0_REG)

#define UART_RX8E1_COUNT_MASK   0x7F
static uint8_t uart_rx8e1_count;    ///< reflects the last/old state of the rx down-counter register

#define UART_TX8E1_COUNT_MASK   0x7F
static uint8_t uart_tx8e1_count;    ///< reflects the last/old state of the rx down-counter register

#define UART_RXDATA_REG     UART_RX8E1_DESERIALIZER_F0_REG
#define UART_TXDATA_REG     UART_TX8E1_SERIALIZER_F0_REG

/**
 * First reset the count
 * and then clear the buffer, as if counter counted one more
 * isr will try to fetch it, but if it misses one it will never
 * clear an interrupt; important if interrupt is rising edge
 * sensitive; as level sensitive may have issues due to
 * clearing -> verify on scope
 */
void UART_Start() {
    UART_RX8E1_TIMER_Start();
    UART_RX8E1_COUNT_Stop();
    UART_TX8E1_COUNT_Stop();
    UART_RX8E1_COUNT_COUNT_REG = uart_rx8e1_count = 0;
    UART_TX8E1_COUNT_COUNT_REG = uart_tx8e1_count = 0;
    UART_RX8E1_COUNT_Start();
    UART_TX8E1_COUNT_Start();
    UART_RX8E1_DESERIALIZER_F0_CLEAR;
}

#endif

/**\{ */

/** Called when sufficient bytes are stored in the fifo buffer. If not all are handled,
 *  re-call will happen on the next byte reception, if this does not happen, is backed up
 *  with the timed event
 */
static void *rx_imm_event(void *arg) {
    isn_uart_collect(this, -1, 0);
    return NULL;
}

/** Flushes bytes in the case threshold trigger left something, or if the threshold never triggered.
 *  This event must retrigger as long all bytes are not flushed.
 *  If interrupt executes just before the return if finds this event as valid, but it changes its
 *  reoccurance timer, so even if NULL is returned, but with the time set in the future, the event
 *  will retrigger.
 */
static void *rx_timeout_event(void *arg) {
    return isn_uart_collect(this, -1, 0) > 0 ? &rx_timeout_event : NULL;
}

/** Copy from UDB to FIFO and take timestamp of last received data */
#if (CYDEV_CHIP_FAMILY_USED == CYDEV_CHIP_FAMILY_PSOC5)
CY_ISR_PROTO(rx_isr__);
CY_ISR(rx_isr__) {
#elif (CYDEV_CHIP_FAMILY_USED == CYDEV_CHIP_FAMILY_PSOC6)
static void rx_isr__() {
#endif
#if !defined(UART_RX8E1_DESERIALIZER_F0_REG)
    uint8_t status;
    while ((status = UART_RXSTATUS_REG) & (UART_RX_STS_FIFO_NOTEMPTY | UART_RX_STS_PAR_ERROR | UART_RX_STS_STOP_ERROR | UART_RX_STS_OVERRUN)) {
        if (status & (UART_RX_STS_PAR_ERROR | UART_RX_STS_STOP_ERROR | UART_RX_STS_OVERRUN)) {
            rxb_err++;
        }
        if (status & UART_RX_STS_FIFO_NOTEMPTY) {
#else
        while (uart_rx8e1_count != (UART_RX8E1_COUNT_COUNT_REG & UART_RX8E1_COUNT_MASK)) {
            uart_rx8e1_count = (uart_rx8e1_count-1) & UART_RX8E1_COUNT_MASK;
#endif
            uint8_t b = UART_RXDATA_REG;
            uint8_t wri_next = rxb_wri + 1; // & RX_FIFO_MASK, but not needed as it is 8-bit
            if (wri_next != rxb_rdi) {
                __atomic_store_n(&rx_buf[rxb_wri], b, __ATOMIC_SEQ_CST);
                __atomic_store_n(&rxb_wri, wri_next,  __ATOMIC_SEQ_CST);
            }
            else {
                rxb_dropped++;
            }
        }
#if !defined(UART_RX8E1_DESERIALIZER_F0_REG)
    }
#endif

    ts = isn_clock_now();
    if (queue) {
        // Trigger immediate event if threshold is reached and postpone timeout event
        if ((uint8_t)(rxb_wri - rxb_rdi) > recv_thr && !isn_reactor_isvalid(uart_imm_trigger, rx_imm_event, NULL)) {
            uart_imm_trigger = queue(rx_imm_event, NULL, isn_clock_now(), hmutex);
        }
        // Spawn a timeout event to flush, if it exists, prolong the timeout
        if (!isn_reactor_isvalid(uart_timeout_trigger, rx_timeout_event, NULL)) {
            uart_timeout_trigger = queue(rx_timeout_event, NULL, ISN_REACTOR_DELAY_ticks(delay), hmutex);
        }
        else isn_reactor_change_timed(uart_timeout_trigger, rx_timeout_event, NULL, ISN_REACTOR_DELAY_ticks(delay));
    }
}

/** Copy from FIFO to TX, and wrap at txb_wrw position */
#if (CYDEV_CHIP_FAMILY_USED == CYDEV_CHIP_FAMILY_PSOC5)
CY_ISR_PROTO(tx_isr__);
CY_ISR(tx_isr__) {
#elif (CYDEV_CHIP_FAMILY_USED == CYDEV_CHIP_FAMILY_PSOC6)
static void tx_isr__() {
#endif
    // \bug Cypress UDB UART Transmission bug, workaround fix.
    // \note Here is a bug with the UDB block, instead of if () there was while () however
    //       reg status did not reflect the latest state under heavy cpu load, and loop
    //       inserted (lost) tx bytes. Probably when cpu was heavily loaded more than a
    //       single byte was to be pushed. Is it UDB or this particular implementation
    //       bug not sure.
#if !defined(UART_TX8E1_SERIALIZER_F0_REG)
    if (UART_TXSTATUS_REG & UART_TX_STS_FIFO_NOT_FULL) {
#else
    uint8_t uart_tx8e1_count_limit = ((UART_TX8E1_COUNT_COUNT_REG & UART_TX8E1_COUNT_MASK) - 4) & UART_TX8E1_COUNT_MASK;
    while (uart_tx8e1_count != uart_tx8e1_count_limit) {
#endif
        if (txb_rdi != txb_wri) {
            if (txb_rdi == txb_wrw) txb_rdi = 0;
            uart_tx8e1_count = (uart_tx8e1_count-1) & UART_TX8E1_COUNT_MASK;
            UART_TXDATA_REG = tx_buf[txb_rdi++];
        }
        else {
#if defined(UART_TX8E1_SERIALIZER_F0_REG)
            break;    // used to be a while loop, removed due to uart udb bug, and now breaks the for loop
#else
# if (CYDEV_CHIP_FAMILY_USED == CYDEV_CHIP_FAMILY_PSOC5)
            IRQ_UART_TX_Disable();
# elif (CYDEV_CHIP_FAMILY_USED == CYDEV_CHIP_FAMILY_PSOC6)
            NVIC_DisableIRQ(IRQ_UART_TX_cfg.intrSrc);
# endif
#endif
        }
    }
}

/**
 * Allocate buffer if buf is given, or just query for availability if buf is NULL
 *
 * \returns desired or limited (max) size in the case desired size is too big
 */
static int isn_uart_getsendbuf(isn_layer_t *drv, void **dest, size_t size, const isn_layer_t *caller) {
    isn_uart_t *obj = (isn_uart_t *)drv;

    if (!obj->buf_locked) {
        txb_alloc_wrw = txb_wrw;    // restore settings from last actual transmission as there is
        txb_alloc_wri = txb_wri;    // a possibilty that there were several getsendbuf() attempts meanwhile
        uint8_t txb_cur_rdi = txb_rdi;  // take snap shot, as it may change during irq

        if (txb_cur_rdi > txb_wri) {
            size_t free = (size_t)(txb_cur_rdi) - (size_t)(txb_wri) - 1;          // [ .. wri <-> rdi ..]
            if (size > free) size = free;
        }
        else {
            size_t free_start = txb_cur_rdi;                            // [ <-> rdi .. wri .. ], actual size is -1
            size_t free_end = (TX_FIFO_SIZE-1) - (size_t)txb_wri;   // [ .. rdi .. wri <-> ]

            if (size < free_end || free_end >= free_start) {        // prefered place to stretch fifo to max
                if (size > free_end) size = free_end;
                txb_alloc_wrw = txb_wri + size;                     // move wrap index to the end
            }
            else if (free_start > 0) {
                free_start--;
                if (size > free_start) size = free_start;
                txb_alloc_wrw = txb_wri;
                txb_alloc_wri = 0;
            }
            else {
                size = 0;
            }
        }
        if (size > 0) {
            if (dest) {
                ASSERT(((size_t)txb_alloc_wri + size) < TX_FIFO_SIZE);
                obj->buf_locked = 1;
                *dest = &tx_buf[txb_alloc_wri];
            }
            return txb_alloc_size = (uint8_t)size;
        }
    }
    if (dest) {
        *dest = NULL;
        obj->drv.stats.tx_retries++;
    }
    return -1;
}

static void isn_uart_free(isn_layer_t *drv, const void *ptr) {
    isn_uart_t *obj = (isn_uart_t *)drv;
    if (ptr == &tx_buf[txb_alloc_wri]) {
        obj->buf_locked = 0;                // we only support one buffer so we may free
    }
}

static int isn_uart_send(isn_layer_t *drv, void *dest, size_t size) {
    isn_uart_t *obj = (isn_uart_t *)drv;
    assert(size <= txb_alloc_size);
    if (size) {
        __atomic_store_n(&txb_wrw, txb_alloc_wrw, __ATOMIC_SEQ_CST);
        __atomic_store_n(&txb_wri, txb_alloc_wri + size, __ATOMIC_SEQ_CST);
        obj->drv.stats.tx_counter += size;
        obj->drv.stats.tx_packets++;
        __atomic_memsync();
#if (CYDEV_CHIP_FAMILY_USED == CYDEV_CHIP_FAMILY_PSOC5)
        IRQ_UART_TX_SetPending();
# if !defined(UART_TX8E1_SERIALIZER_F0_REG)
        IRQ_UART_TX_Enable();
# endif
#elif (CYDEV_CHIP_FAMILY_USED == CYDEV_CHIP_FAMILY_PSOC6)
# if !defined(UART_TX8E1_SERIALIZER_F0_REG)
        NVIC_EnableIRQ(IRQ_UART_TX_cfg.intrSrc);
# endif
        NVIC_SetPendingIRQ(IRQ_UART_TX_cfg.intrSrc);
#endif
        obj->buf_locked = 0;
    }
    return size;
}

/**
 * Zero-Copy Forward of the received buffer
 *
 * Overflown buffers are split into linear sections, and multiple calls.
 */
int isn_uart_collect(isn_uart_t *obj, size_t maxsize, uint32_t timeout) {
    size_t size = (uint8_t)(rxb_wri - rxb_rdi);

    if (size >= maxsize || (isn_clock_elapsed(ts) > timeout && size > 0)) {
        if (size > maxsize) size = maxsize;
        do {
            size_t linsize = size;
            if (((size_t)rxb_rdi + linsize) >= RX_FIFO_SIZE) linsize = RX_FIFO_SIZE - rxb_rdi;
            size_t fw_size = obj->child_driver->recv(obj->child_driver, &rx_buf[rxb_rdi], linsize, obj);
            if (fw_size) obj->drv.stats.rx_packets++;
            rxb_rdi += fw_size;
            obj->drv.stats.rx_counter += fw_size;
            if (fw_size != linsize) {
                obj->drv.stats.rx_retries++;    // Packet could not be fully accepted, retry next time
                break;
            }
            size -= fw_size;
        }
        while(size > 0); // When packet wraps around it is sent in two portions
    }

    obj->drv.stats.rx_errors  = rxb_err;
    obj->drv.stats.rx_dropped = rxb_dropped;
    return (uint8_t)(rxb_wri - rxb_rdi);    // return remaining bytes that could not be sent and recalling of this func should be asap
}

size_t isn_uart_getrecvsize() {
    return (uint8_t)(rxb_wri - rxb_rdi);
}

int isn_uart_getrecvbyte() {
    if (rxb_wri != rxb_rdi) {
        this->drv.stats.rx_counter++;
        return rx_buf[rxb_rdi++];
    }
    return -1;
}

void isn_uart_radiate(isn_uart_t *obj, size_t receive_threshold, isn_clock_counter_t timeout, isn_reactor_queue_t priority_queue,
                      isn_reactor_mutex_t busy_mutex, isn_reactor_mutex_t holdon_mutex) {
    ASSERT(receive_threshold > 0);
    ASSERT(timeout > 0);
    this  = obj;
    queue = priority_queue;
    delay = timeout;
    recv_thr = receive_threshold;
    hmutex = holdon_mutex;
}

isn_reactor_queue_t isn_uart_radiate_setqueue(isn_reactor_queue_t priority_queue) {
    isn_reactor_queue_t prev_queue = queue;
    queue = priority_queue;
    return prev_queue;
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
    UART_Start();

#if (CYDEV_CHIP_FAMILY_USED == CYDEV_CHIP_FAMILY_PSOC5)
    IRQ_UART_TX_StartEx(&tx_isr__);
    IRQ_UART_RX_StartEx(&rx_isr__);
#elif (CYDEV_CHIP_FAMILY_USED == CYDEV_CHIP_FAMILY_PSOC6)
    Cy_SysInt_Init(&IRQ_UART_RX_cfg, rx_isr__);
    Cy_SysInt_Init(&IRQ_UART_TX_cfg, tx_isr__);
    NVIC_EnableIRQ(IRQ_UART_RX_cfg.intrSrc);
# if defined(UART_TX8E1_SERIALIZER_F0_REG)
    NVIC_EnableIRQ(IRQ_UART_TX_cfg.intrSrc);
# endif
#else
# error "Unsupported platform"
#endif
}

/** \} \endcond */
