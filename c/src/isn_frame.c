/** \file
 *  \author Uros Platise <uros@isotel.eu>
 *  \see isn_frame.h
 */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 * 
 * (c) Copyright 2019, Isotel, http://isotel.eu
 */

#include "isn_frame.h"

#define SNCF_CRC8_POLYNOMIAL    ((uint8_t) 0x4D)    // best polynom for data sizes up to 64 bytes

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
static int isn_frame_getsendbuf(isn_layer_t *drv, void **dest, size_t size) {
    isn_frame_t *obj = (isn_frame_t *)drv;
    if (size > ISN_FRAME_MAXSIZE) size = ISN_FRAME_MAXSIZE; // limited by the frame protocol
    int xs = 1 + (int)obj->crc_enabled;
    xs = obj->parent_driver->getsendbuf(obj->parent_driver, dest, size + xs) - xs;
    uint8_t **buf = (uint8_t **)dest;
    if (buf) {
        if (*buf) (*buf)++;
    }
    return xs;
}


static void isn_frame_free(isn_layer_t *drv, const void *ptr) {
    isn_frame_t *obj = (isn_frame_t *)drv;
    const uint8_t *buf = ptr;
    if (buf) obj->parent_driver->free(obj->parent_driver, buf - 1);
}


static int isn_frame_send(isn_layer_t *drv, void *dest, size_t size) {
    isn_frame_t *obj = (isn_frame_t *)drv;
    uint8_t *buf = dest;
    uint8_t *start = --buf;
    assert(size <= ISN_FRAME_MAXSIZE);
    *buf = 0xC0 - 1 + size;             // Header, assuming short frame
    if (obj->crc_enabled) {
        *buf ^= 0x40;                   // Update header for the CRC (0x40 was set just above)
        size += 1;                      // we need one more space for CRC at the end
        uint8_t crc = 0;
        for (int i=0; i<size; i++) crc = crc8(crc ^ *buf++);
        *buf = crc;
    }
    obj->parent_driver->send(obj->parent_driver, start, size+1); // Size +1 for the front header
    return size-1-(int)obj->crc_enabled;     // return back userpayload only
}


static void pass(isn_frame_t *obj, int protocol, const uint8_t * buf, uint8_t size, isn_driver_t *caller) {
    isn_bindings_t *layer = obj->bindings_drivers;
    layer--;
    do {
        layer++;
        if (layer->protocol == protocol) {
            isn_driver_t *driver = (isn_driver_t *)layer->driver;
            if (driver->recv) {
                assert2( driver->recv(driver, buf, size, caller) == buf );
                return;
            }
        }
    }
    while (layer->protocol >= 0);
}


#define IS_NONE         0
#define IS_IN_MESSAGE   1

static const void * isn_frame_recv(isn_layer_t *drv, const void *src, size_t size, isn_driver_t *caller) {
    isn_frame_t *obj = (isn_frame_t *)drv;
    const uint8_t *buf = src;

    if ((*(obj->sys_counter) - obj->last_ts) > obj->frame_timeout) {
        obj->state = IS_NONE;
        obj->recv_size = obj->recv_len = 0;
    }
    obj->last_ts = *(obj->sys_counter);

    for (uint8_t i=0; i<size; i++, buf++) {
        switch (obj->state) {
            case IS_NONE: {
                if (*buf > 0x80) {
                    if (obj->recv_size) {
                        pass(obj, ISN_PROTO_OTHERWISE, obj->recv_buf, obj->recv_size, caller);
                        obj->recv_size = obj->recv_len = 0;
                    }
                    obj->state = IS_IN_MESSAGE;
                    if (obj->crc_enabled) {
                        obj->crc  = crc8(*buf);
                    }
                    obj->recv_len = (*buf & 0x3F) + 1;
                }
                else {
                    obj->recv_buf[obj->recv_size++] = *buf;   // collect other data to be passed to OTHER ..
                }
                break;
            }
            case IS_IN_MESSAGE: {
                if (obj->recv_size == obj->recv_len && obj->crc_enabled) {
                    if (*buf == obj->crc) {
                        pass(obj, obj->recv_buf[0], obj->recv_buf, obj->recv_size, drv);
                    }
                    obj->recv_size = obj->recv_len = 0;
                    obj->state = IS_NONE;
                }
                else {
                    obj->recv_buf[obj->recv_size++] = *buf;
                    if (obj->crc_enabled) {
                        obj->crc = crc8(obj->crc ^ *buf);
                    }
                    else if (obj->recv_size == obj->recv_len) {
                        pass(obj, obj->recv_buf[0], obj->recv_buf, obj->recv_size, drv);
                        obj->recv_size = obj->recv_len = 0;
                        obj->state = IS_NONE;
                    }
                }
                break;
            }
        }
    }

    // flush non-framed (packed) data immediately
    if (obj->recv_size && obj->recv_len == 0) {
        pass(obj, ISN_PROTO_OTHERWISE, obj->recv_buf, obj->recv_size, caller);
        obj->recv_size = 0;
    }
    return buf;
}


void isn_frame_init(isn_frame_t *obj, isn_frame_mode_t mode, isn_bindings_t* bindings, isn_layer_t* parent, volatile uint32_t *counter, uint32_t timeout) {
    obj->drv.getsendbuf   = isn_frame_getsendbuf;
    obj->drv.send         = isn_frame_send;
    obj->drv.recv         = isn_frame_recv;
    obj->drv.free         = isn_frame_free;

    obj->parent_driver    = parent;
    obj->crc_enabled      = mode;
    obj->bindings_drivers = bindings;
    obj->sys_counter      = counter;
    obj->frame_timeout    = timeout;

    obj->state            = IS_NONE;
    obj->recv_size        = 0;
    obj->recv_len         = 0;
    obj->last_ts          = 0;
}
