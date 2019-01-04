#include "project.h"
#include "isn_usbuart.h"

#define TXBUF_SIZE  64
#define RXBUF_SIZE  64

static isn_layer_t* child_driver;
static int buf_locked = 0;
static uint8_t txbuf[TXBUF_SIZE];

/**
 * Allocate buffer if buf is given, or just query for availability if buf is NULL
 * 
 * \returns desired or limited (max) size in the case desired size is too big
 */
int isn_usbuart_getsendbuf(uint8_t **buf, size_t size) {
    if (USBUART_CDCIsReady() && !buf_locked) {
        if (buf) {
            buf_locked = 1;
            *buf = txbuf;
        }
        return (size > TXBUF_SIZE) ? TXBUF_SIZE : size;
    }
    return 0;
}

void isn_usbuart_free(const uint8_t *buf) {
    if (buf == txbuf) {
        buf_locked = 0;             // we only support one buffer so we may free
    }
}

int isn_usbuart_send(uint8_t *buf, size_t size) {
    assert(size <= TXBUF_SIZE);
    while( !USBUART_CDCIsReady() ); // todo: timeout assert
    USBUART_PutData(buf, size);
    isn_usbuart_free(buf);          // free buffer, however need to block use of buffer until sent out
    return size;
}

size_t isn_usbuart_poll() {
    static uint8_t buf[RXBUF_SIZE];
    size_t size = 0;
    if (USBUART_DataIsReady()) {
        USBUART_GetData(buf, size = USBUART_GetCount());    // Fetch data even if size = 0 to reinit the OUT EP
        if (size) {
            child_driver->recv(buf, size, &isn_usbuart);
        }
    }
    return size;
}

void isn_usbuart_init(int mode, isn_layer_t* child) {
    child_driver = child;
    USBUART_Start(0, mode);
    while(0 == USBUART_GetConfiguration());
    USBUART_CDC_Init();
}

isn_layer_t isn_usbuart = {
    isn_usbuart_getsendbuf,
    isn_usbuart_send,
    NULL,
    isn_usbuart_free
};
