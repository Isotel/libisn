#include "isn_frame.h"

#define FRAME_MAXSIZE           64                  // max short/compact frame len
#define SNCF_CRC8_POLYNOMIAL    ((uint8_t) 0x4D)    // best polynom for data sizes up to 64 bytes

static isn_bindings_t *bindings_drivers;
static isn_layer_t* parent_driver;
static isn_frame_mode_t crc_enabled;
volatile uint32_t *sys_counter;
static uint32_t frame_timeout;

/** Slow compact implementation, may need speed up */
static uint8_t crc8(const uint8_t b) {
    uint8_t remainder = b;

    for (uint8_t bit = 8; bit > 0; --bit) {
        if (remainder & 0x80) {
            remainder = (remainder << 1) ^ SNCF_CRC8_POLYNOMIAL;
        } else {
            remainder = (remainder << 1);
        }
    }
    return remainder;
}

/**
 * Allocate a byte more for a header (protocol number) and optional checksum at the end
 */
int isn_frame_getsendbuf(uint8_t **buf, size_t size) {
    if (size > FRAME_MAXSIZE) size = FRAME_MAXSIZE;       // limited by the frame protocol
    int xs = 1 + (int)crc_enabled;
    xs = parent_driver->getsendbuf(buf, size + xs) - xs;
    if (buf) {
        if (*buf) (*buf)++;
    }
    return xs;
}


void isn_frame_free(const uint8_t *buf) {
    if (buf) parent_driver->free( buf-1 );
}


int isn_frame_send(uint8_t *buf, size_t size) {
    uint8_t *start = --buf;
    assert(size <= FRAME_MAXSIZE);
    *buf = 0xC0 - 1 + size;             // Header, assuming short frame
    if (crc_enabled) {
        *buf ^= 0x40;                   // Update header for the CRC (0x40 was set just above)
        size += 1;                      // we need one more space for CRC at the end
        uint8_t crc = 0;
        for (int i=0; i<size; i++) crc = crc8(crc ^ *buf++);
        *buf = crc;
    }
    parent_driver->send(start, size+1); // Size +1 for the front header
    return size-1-(int)crc_enabled;     // return back userpayload only
}


static void pass(int protocol, const uint8_t * buf, uint8_t size, isn_layer_t *caller) {
    isn_bindings_t *layer = bindings_drivers;
    layer--;
    do {
        layer++;
        if (layer->protocol == protocol) {
            if (layer->driver->recv) {
                assert2( layer->driver->recv(buf, size, caller) == buf );
                return;
            }
        }
    }
    while (layer->protocol >= 0);
}


const uint8_t * isn_frame_recv(const uint8_t *buf, size_t size, isn_layer_t *caller) {
    typedef enum driver_input_state {
        IS_NONE       = 0,
        IS_IN_MESSAGE = 1,
    } driver_input_state_t;

    static driver_input_state_t state = IS_NONE;
    static uint8_t crc;
    static uint8_t recv_buf[FRAME_MAXSIZE];
    static uint8_t recv_size = 0;
    static uint8_t recv_len  = 0;
    static uint32_t last_ts  = 0;

    if ((*sys_counter - last_ts) > frame_timeout) {
        state = IS_NONE;
        recv_size = recv_len = 0;
    }
    last_ts = *sys_counter;

    for (uint8_t i=0; i<size; i++, buf++) {
        switch (state) {
            case IS_NONE: {
                if (*buf > 0x80) {
                    if (recv_size) {
                        pass(ISN_PROTO_OTHERWISE, recv_buf, recv_size, caller);
                        recv_size = recv_len = 0;
                    }
                    state    = IS_IN_MESSAGE;
                    if (crc_enabled) {
                        crc  = crc8(*buf);
                    }
                    recv_len = (*buf & 0x3F) + 1;
                }
                else {
                    recv_buf[recv_size++] = *buf;   // collect other data to be passed to OTHER ..
                }
                break;
            }
            case IS_IN_MESSAGE: {
                if (recv_size == recv_len && crc_enabled) {
                    if (*buf == crc) {
                        pass(recv_buf[0], recv_buf, recv_size, &isn_frame);
                    }
                    recv_size = recv_len = 0;
                    state = IS_NONE;
                }
                else {
                    recv_buf[recv_size++] = *buf;
                    if (crc_enabled) {
                        crc = crc8(crc ^ *buf);
                    }
                    else if (recv_size == recv_len) {
                        pass(recv_buf[0], recv_buf, recv_size, &isn_frame);
                        recv_size = recv_len = 0;
                        state = IS_NONE;
                    }
                }
                break;
            }
        }
    }

    // flush non-framed (packed) data immediately
    if (recv_size && recv_len == 0) {
        pass(ISN_PROTO_OTHERWISE, recv_buf, recv_size, caller);
        recv_size = 0;
    }
    return buf;
}


void isn_frame_init(isn_frame_mode_t mode, isn_bindings_t* bindings, isn_layer_t* parent, volatile uint32_t *counter, uint32_t timeout) {
    crc_enabled      = mode;
    bindings_drivers = bindings;
    parent_driver    = parent;
    sys_counter      = counter;
    frame_timeout    = timeout;
}


isn_layer_t isn_frame = {
    isn_frame_getsendbuf,
    isn_frame_send, 
    isn_frame_recv,
    isn_frame_free
};
