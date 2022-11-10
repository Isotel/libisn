#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#define RX_FIFO_SIZE    256             ///< Interrupt and Zero-Copy Receive Buffer, this size of 256 simplifes circular buffers with 8-bit integers
#define TX_FIFO_SIZE    256             ///< Interrupt and Zero-Copy Transmit Buffer, this size of 256 simplifes circular buffers with 8-bit integers

int buf_locked = 0;

static uint8_t tx_buf[TX_FIFO_SIZE];
static volatile uint8_t txb_wri = 0;    ///< Write index, followed by txb_rdi, wri must never be incremented over rdi, effective buffer size is thus -1
static volatile uint8_t txb_rdi = 0;    ///< when txb_rdi == txb_wri, buffer is empty
static volatile uint8_t txb_wrw = 255;  ///< Dynamic Wrap point to simulate linear buffers, at which point rd/wr index wrap

static uint8_t txb_alloc_wrw = 255, txb_alloc_wri = 255, txb_alloc_size = 0;    // invalidate, so free cannot work

static size_t txed = 0;

static int isn_uart_getsendbuf(void **dest, size_t size) {
    if (!buf_locked) {
        txb_alloc_wrw = txb_wrw;    // restore settings from last actual transmission as there is
        txb_alloc_wri = txb_wri;    // a possibilty that there were several getsendbuf() attempts meanwhile

        if (txb_rdi > txb_wri) {
            size_t free = (size_t)(txb_rdi - txb_wri) - 1;          // [ .. wri <-> rdi ..]
            if (size > free) size = free;
            printf(" (#1) ");
        }
        else {
            size_t free_end = (TX_FIFO_SIZE-1) - (size_t)txb_wri;   // [ .. rdi .. wri <-> ]
            size_t free_start = txb_rdi;                            // [ <-> rdi .. wri .. ], actual size is -1

            if (size < free_end || free_end >= free_start) {         // prefered place to stretch fifo to max
                if (size > free_end) size = free_end;
                txb_alloc_wrw = txb_wri + size;                     // move wrap index to the end
                printf(" (#2) ");
            }
            else if (free_start > 0) {
                free_start--;
                if (size > free_start) size = free_start;
                txb_alloc_wrw = txb_wri;
                txb_alloc_wri = 0;
                printf(" (#3) ");
            }
            else {
                size = 0;
                printf(" (#4) ");
            }
        }
        if (size > 0) {
            if (dest) {
                buf_locked = 1;
                *dest = &tx_buf[txb_alloc_wri];
            }
            return txb_alloc_size = size;
        }
    }
    if (dest) {
        *dest = NULL;
    }
    return -1;
}

static int isn_uart_send(size_t size) {
    txb_wrw = txb_alloc_wrw;            // modify first as ISR first checks
    txb_wri = txb_alloc_wri + size;     // the wri index and then wraps
    buf_locked = 0;
}

static void tx() {
    static uint8_t cnt = 0;
    while (txb_rdi != txb_wri) {
        if (txb_rdi == txb_wrw) {
            printf(" (wrap:%d) ", txb_rdi);
            txb_rdi = 0;
        }
        if (cnt != tx_buf[txb_rdi]) {
            printf("Data consistency error: %d vs. %d\n", cnt-1, tx_buf[txb_rdi]);
            exit(1);
        }
        txb_rdi++;
        txed++;
        cnt++;
    }
}

static void fill(void *dest, int size) {
    static uint8_t cnt = 0;
    uint8_t *ptr = (uint8_t *)dest;
    if (ptr && size > 0) while(size-->0) *ptr++ = cnt++;
}

static void stats() {
    printf("wri=%d, rdi=%d, wrw=%d, alloc_wri=%d, alloc_wrw=%d\n", txb_wri, txb_rdi, txb_wrw, txb_alloc_wri, txb_alloc_wrw);
}

void main() {
    void *buf;
    int s;
    size_t sent = 0;
    for (int j=0; j<100; j++) {
        for (int i=1; i<200; i++) {
            printf("\nreq: %d, avail: %d: ", i, s=isn_uart_getsendbuf(&buf, i)); 
            if (s > 0) {
                fill(buf, s);
                isn_uart_send(s);
                sent += s;
            }
            if (i & 1) {
                tx();
                if (txed != sent) {
                    printf("Error, %ld != %ld\n", txed, sent);
                    return;
                }
            }
            stats();
        }
    }    
}