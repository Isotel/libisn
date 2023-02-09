/** \file
 *  \brief ISN Long Frame Protocol up to 4096 B frames with 16-bit Implementation
 *  \author Uros Platise <uros@isotel.org>
 *  \see isn_frame_long.h
 */
/**
 * \ingroup GR_ISN
 * \cond Implementation
 * \addtogroup GR_ISN_Frame_Long
 */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * (c) Copyright 2022, Isotel, http://isotel.org
 */

#include <string.h>
#include <stdlib.h>
#include "isn_clock.h"
#include "isn_frame_long.h"

/**\{ */

#define ISN_FRAME_LONG_HEADER       2
#define ISN_FRAME_LONG_FOOTER       2
#define ISN_FRAME_LONG_OVERHEAD     (ISN_FRAME_LONG_HEADER + ISN_FRAME_LONG_FOOTER)

#define CRC16_CCITT_INITVALUE       0xFFFF  // CRC16_CCIT_FALSE

#ifndef __ISN_FRAME_LONG_CRC16_NOLOOKUP__

static inline __attribute__((always_inline)) uint16_t crc16_ccitt(uint16_t crc, uint8_t c) {
    static const uint16_t ccitt_crc16_table[256] = {
        0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
        0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
        0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
        0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de,
        0x2462, 0x3443, 0x0420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485,
        0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
        0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695, 0x46b4,
        0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc,
        0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
        0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b,
        0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12,
        0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
        0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41,
        0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49,
        0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70,
        0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78,
        0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f,
        0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
        0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e,
        0x02b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256,
        0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
        0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
        0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c,
        0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
        0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab,
        0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882, 0x28a3,
        0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
        0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92,
        0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9,
        0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1,
        0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8,
        0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0x0ed1, 0x1ef0
    };
    uint8_t pos = (crc >> 8) ^ c;
    return (crc << 8) ^ ccitt_crc16_table[pos];
}

#else

static inline __attribute__((always_inline)) uint16_t crc16_ccitt(uint16_t crc, uint8_t c) {
    crc = (uint8_t)(crc >> 8) | (crc << 8);
    crc ^= c;
    crc ^= (uint8_t)(crc & 0xff) >> 4;
    crc ^= crc << 12;
    crc ^= (crc & 0xff) << 5;
    return crc;
}

#endif

/**
 * Allocate a byte more for a header (protocol number) and optional checksum at the end
 */
static int isn_frame_long_getsendbuf(isn_layer_t *drv, void **dest, size_t size, const isn_layer_t *caller) {
    isn_frame_long_t *obj = (isn_frame_long_t *)drv;
    if (size > ISN_FRAME_LONG_MAXSIZE) size = ISN_FRAME_LONG_MAXSIZE; // limited by the frame protocol
    int xs = obj->parent->getsendbuf(obj->parent, dest, size + ISN_FRAME_LONG_OVERHEAD, caller) - ISN_FRAME_LONG_OVERHEAD;
    uint8_t **buf = (uint8_t **)dest;
    if (buf) {
        if (*buf) (*buf)+=ISN_FRAME_LONG_HEADER;
    }
    return xs;
}


static void isn_frame_long_free(isn_layer_t *drv, const void *ptr) {
    isn_frame_long_t *obj = (isn_frame_long_t *)drv;
    const uint8_t *buf = ptr;
    if (buf) obj->parent->free(obj->parent, buf - ISN_FRAME_LONG_HEADER);
}

static int isn_frame_long_send(isn_layer_t *drv, void *dest, size_t size) {
    isn_frame_long_t *obj = (isn_frame_long_t *)drv;
    uint8_t *buf = &((uint8_t *)dest)[-ISN_FRAME_LONG_HEADER];
    uint8_t *start = buf;

    assert(size <= ISN_FRAME_LONG_MAXSIZE);
    obj->drv.stats.tx_counter += size;
    obj->drv.stats.tx_packets++;

    *buf++ = ISN_PROTO_FRAME_LONG | ((size - 1) >> 8);
    *buf++ = (size - 1) & 0xFF;

    uint16_t crc = CRC16_CCITT_INITVALUE;
    for (int i=0; i<(size + ISN_FRAME_LONG_HEADER); i++) crc = crc16_ccitt(crc, start[i]);
    buf   += size;
    *buf++ = crc >> 8;
    *buf   = crc & 0xFF;

    obj->parent->send(obj->parent, start, size + ISN_FRAME_LONG_OVERHEAD);
    return size;
}

#define IS_NONE         0
#define IS_IN_PROTOCOL  1
#define IS_IN_MESSAGE   2
#define IS_IN_CRC       3
#define IS_FW_MESSAGE   4

static size_t isn_frame_long_recv(isn_layer_t *drv, const void *src, size_t size, isn_layer_t *caller) {
    isn_frame_long_t *obj = (isn_frame_long_t *)drv;
    const volatile uint8_t *buf = src;

    if (obj->state != IS_FW_MESSAGE && isn_clock_elapsed(obj->last_ts) > obj->frame_timeout) {
        obj->state = IS_NONE;
        if (obj->recv_len) obj->drv.stats.rx_dropped++;
        obj->recv_size = obj->recv_len = 0;
    }
    obj->last_ts = isn_clock_now();

    if (!src || !size) {
        obj->drv.stats.rx_dropped++;
        return size;
    }

    for (uint8_t i=0; i<size;) {
        switch (obj->state) {
            case IS_NONE: {
                if ( (*buf & ISN_PROTO_FRAME_LONG_MASK) == ISN_PROTO_FRAME_LONG) {
                    if (obj->recv_size) {
                        if (obj->other) obj->other->recv(obj->other, obj->recv_buf, obj->recv_size, caller);
                        obj->recv_size = obj->recv_len = 0;
                    }
                    obj->state    = IS_IN_PROTOCOL;
                    obj->crc      = crc16_ccitt(CRC16_CCITT_INITVALUE, *buf);
                    obj->recv_len = ((uint16_t)(*buf & ~ISN_PROTO_FRAME_LONG_MASK)) << 8;
                }
                else {
                    obj->recv_buf[obj->recv_size++] = *buf;   // collect other data to be passed to OTHER ..
                }
                i++; buf++;
                break;
            }
            case IS_IN_PROTOCOL: {
                obj->state = IS_IN_MESSAGE;
                obj->crc  = crc16_ccitt(obj->crc, *buf);
                obj->recv_len |= *buf;
                obj->recv_len++;
                i++; buf++;
                break;
            }
            case IS_IN_MESSAGE: {
                if (obj->recv_size == obj->recv_len) {
                    obj->state = IS_IN_CRC;
                    obj->crc ^= ((uint16_t)*buf) << 8;  // xor msb crc
                }
                else {
                    obj->recv_buf[obj->recv_size++] = *buf;
                    obj->crc = crc16_ccitt(obj->crc, *buf);
                }
                i++; buf++;
                break;
            }
            case IS_IN_CRC: {
                obj->crc ^= *buf;   // xor lsb crc and compare if zero
                if (!obj->crc) {
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
                i++; buf++;
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

void isn_frame_long_init(isn_frame_long_t *obj, isn_layer_t* child, isn_layer_t* other, isn_layer_t* parent, uint32_t timeout) {
    ASSERT(obj);
    ASSERT(parent);
    ASSERT(child);
    memset(&obj->drv, 0, sizeof(obj->drv));

    obj->drv.getsendbuf   = isn_frame_long_getsendbuf;
    obj->drv.send         = isn_frame_long_send;
    obj->drv.recv         = isn_frame_long_recv;
    obj->drv.free         = isn_frame_long_free;

    obj->parent           = parent;
    obj->child            = child;
    obj->other            = other;
    obj->frame_timeout    = timeout;

    obj->state            = IS_NONE;
    obj->recv_size        = 0;
    obj->recv_len         = 0;
    obj->last_ts          = 0;
}

isn_frame_long_t* isn_frame_long_create() {
    isn_frame_long_t* obj = malloc(sizeof(isn_frame_long_t));
    return obj;
}

void isn_frame_long_drop(isn_frame_long_t *obj) {
    free(obj);
}

/** \} \endcond */
