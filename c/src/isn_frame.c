/** \file
 *  \brief ISN Short and Compact (with CRC) Frame Protocol up to 64 B frames Implementation
 *  \author Uros Platise <uros@isotel.org>
 *  \see isn_frame.h
 */
/**
 * \ingroup GR_ISN
 * \cond Implementation
 * \addtogroup GR_ISN_Frame
 */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * (c) Copyright 2019 - 2021, Isotel, http://isotel.org
 */

#include <string.h>
#include <stdlib.h>
#include "isn_clock.h"
#include "isn_frame.h"

/**\{ */

#ifndef __ISN_FRAME_SLOWCRC8__

static inline __attribute__((always_inline)) uint8_t crc8(const uint8_t b) {
    static const uint8_t crc8_table[256] = {
        0, 77, 154, 215, 121, 52, 227, 174, 242, 191, 104, 37, 139, 198, 17,
        92, 169, 228, 51, 126, 208, 157, 74, 7, 91, 22, 193, 140, 34, 111,
        184, 245, 31, 82, 133, 200, 102, 43, 252, 177, 237, 160, 119, 58, 148,
        217, 14, 67, 182, 251, 44, 97, 207, 130, 85, 24, 68, 9, 222, 147, 61,
        112, 167, 234, 62, 115, 164, 233, 71, 10, 221, 144, 204, 129, 86, 27,
        181, 248, 47, 98, 151, 218, 13, 64, 238, 163, 116, 57, 101, 40, 255,
        178, 28, 81, 134, 203, 33, 108, 187, 246, 88, 21, 194, 143, 211, 158,
        73, 4, 170, 231, 48, 125, 136, 197, 18, 95, 241, 188, 107, 38, 122,
        55, 224, 173, 3, 78, 153, 212, 124, 49, 230, 171, 5, 72, 159, 210,
        142, 195, 20, 89, 247, 186, 109, 32, 213, 152, 79, 2, 172, 225, 54,
        123, 39, 106, 189, 240, 94, 19, 196, 137, 99, 46, 249, 180, 26, 87,
        128, 205, 145, 220, 11, 70, 232, 165, 114, 63, 202, 135, 80, 29, 179,
        254, 41, 100, 56, 117, 162, 239, 65, 12, 219, 150, 66, 15, 216, 149,
        59, 118, 161, 236, 176, 253, 42, 103, 201, 132, 83, 30, 235, 166, 113,
        60, 146, 223, 8, 69, 25, 84, 131, 206, 96, 45, 250, 183, 93, 16, 199,
        138, 36, 105, 190, 243, 175, 226, 53, 120, 214, 155, 76, 1, 244, 185,
        110, 35, 141, 192, 23, 90, 6, 75, 156, 209, 127, 50, 229, 168
    };
    return crc8_table[b];
}

#else

/** Compact implementation */
static uint8_t crc8(uint8_t b) {
    for (uint8_t bit = 8; bit > 0; --bit) {
        uint8_t x = b << 1;
        if (b & 0x80) x ^= ISNCF_CRC8_POLYNOMIAL_NORMAL;
        b = x;
    }
    return b;
}

#endif

/**
 * Allocate a byte more for a header (protocol number) and optional checksum at the end
 */
static int isn_frame_getsendbuf(isn_layer_t *drv, void **dest, size_t size, const isn_layer_t *caller) {
    isn_frame_t *obj = (isn_frame_t *)drv;
    if (size > ISN_FRAME_MAXSIZE) size = ISN_FRAME_MAXSIZE; // limited by the frame protocol
    int xs = 1 + (int)obj->crc_enabled;
    xs = obj->parent->getsendbuf(obj->parent, dest, size + xs, caller) - xs;
    uint8_t **buf = (uint8_t **)dest;
    if (buf) {
        if (*buf) (*buf)++;
    }
    return xs;
}


static void isn_frame_free(isn_layer_t *drv, const void *ptr) {
    isn_frame_t *obj = (isn_frame_t *)drv;
    const uint8_t *buf = ptr;
    if (buf) obj->parent->free(obj->parent, buf - 1);
}

static int isn_frame_send(isn_layer_t *drv, void *dest, size_t size) {
    isn_frame_t *obj = (isn_frame_t *)drv;
    uint8_t *buf = dest;
    uint8_t *start = --buf;
    assert(size <= ISN_FRAME_MAXSIZE);
    obj->drv.stats.tx_packets++;
    obj->drv.stats.tx_counter += size;
    *buf = 0xC0 - 1 + size;             // Header, assuming short frame
    size_t frame_size = size + 1;       // Add header to the payload size
    if (obj->crc_enabled) {
        *buf ^= 0x40;                   // Update header for the CRC (0x40 was set just above)
        uint8_t crc = 0;
        for (int i=0; i<frame_size; i++) crc = crc8(crc ^ *buf++);
        *buf = crc;
        frame_size++;                   // Add CRC size
    }
    obj->parent->send(obj->parent, start, frame_size);
    return size;
}

#define IS_NONE         0
#define IS_IN_MESSAGE   1
#define IS_FW_MESSAGE   2

static size_t isn_frame_recv(isn_layer_t *drv, const void *src, size_t size, isn_layer_t *caller) {
    isn_frame_t *obj = (isn_frame_t *)drv;
    const volatile uint8_t *buf = src;

    if (isn_clock_elapsed(obj->last_ts) > obj->frame_timeout) {
        obj->state = IS_NONE;
        if (obj->recv_len) {
            obj->drv.stats.rx_dropped++;
        }
        obj->recv_size = obj->recv_len = 0;
    }
    obj->last_ts = isn_clock_now();

    if (!src || !size) {
        obj->drv.stats.rx_dropped++;
        return size;
    }

    for (uint8_t i=0; i<size; i++, buf++) {
        switch (obj->state) {
            case IS_NONE: {
                if (*buf > 0x80) {
                    if (obj->recv_size) {
                        if (obj->other) obj->other->recv(obj->other, obj->recv_buf, obj->recv_size, caller);
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
                        obj->state = IS_FW_MESSAGE;
                        obj->recv_fwed = 0;
                        obj->drv.stats.rx_packets++;
                        obj->drv.stats.rx_counter += obj->recv_size;
                    }
                    else {
                        obj->drv.stats.rx_errors++;
                        obj->recv_size = obj->recv_len = 0;
                        obj->state = IS_NONE;
                    }
                }
                else {
                    obj->recv_buf[obj->recv_size++] = *buf;
                    if (obj->crc_enabled) {
                        obj->crc = crc8(obj->crc ^ *buf);
                    }
                    else if (obj->recv_size == obj->recv_len) {
                        obj->state = IS_FW_MESSAGE;
                        obj->recv_fwed = 0;
                        obj->drv.stats.rx_packets++;
                        obj->drv.stats.rx_counter += obj->recv_size;
                    }
                }
                break;
            }
            default:
                break;
        }

        if (obj->state == IS_FW_MESSAGE) {
            size_t forwarded_bytes = obj->child->recv(obj->child, &obj->recv_buf[obj->recv_fwed], obj->recv_size, obj);
            if (forwarded_bytes < obj->recv_size) {
                obj->recv_fwed += forwarded_bytes;
                obj->recv_size -= forwarded_bytes;
                obj->drv.stats.rx_retries++;
                return i;   // currently we received up to this size, return the rest
            }
            obj->recv_size = obj->recv_len = 0;
            obj->state = IS_NONE;
        }
    }

    // flush non-framed (packed) data immediately
    if (obj->recv_size && obj->recv_len == 0) {
        if (obj->other) obj->other->recv(obj->other, obj->recv_buf, obj->recv_size, caller);
        obj->recv_size = 0;
    }
    return size;
}

void isn_frame_init(isn_frame_t *obj, isn_frame_mode_t mode, isn_layer_t* child, isn_layer_t* other, isn_layer_t* parent, uint32_t timeout) {
    ASSERT(obj);
    ASSERT(parent);
    ASSERT(child);
    memset(&obj->drv, 0, sizeof(obj->drv));

    obj->drv.getsendbuf   = isn_frame_getsendbuf;
    obj->drv.send         = isn_frame_send;
    obj->drv.recv         = isn_frame_recv;
    obj->drv.free         = isn_frame_free;

    obj->parent           = parent;
    obj->crc_enabled      = mode;
    obj->child            = child;
    obj->other            = other;
    obj->frame_timeout    = timeout;

    obj->state            = IS_NONE;
    obj->recv_size        = 0;
    obj->recv_len         = 0;
    obj->last_ts          = 0;
}

isn_frame_t* isn_frame_create() {
    isn_frame_t* obj = malloc(sizeof(isn_frame_t));
    return obj;
}

void isn_frame_drop(isn_frame_t *obj) {
    free(obj);
}

/** \} \endcond */