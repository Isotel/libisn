/** \file
 *  \brief ISN Transport Layer
 *  \author Uros Platise <uros@isotel.org>
 *  \see isn_trans.h
 */
/**
 * \ingroup GR_ISN
 * \cond Implementation
 * \addtogroup GR_ISN_Trans
 */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * (c) Copyright 2019, Isotel, http://isotel.org
 */

#include <string.h>
#include "isn_trans.h"

#define PROTO_SIZE  4

/**\{ */

static int isn_trans_getsendbuf(isn_layer_t *drv, void **dest, size_t size, const isn_layer_t *caller) {
    isn_trans_t *obj = (isn_trans_t *)drv;
    int osize = obj->parent->getsendbuf(obj->parent, dest, size+PROTO_SIZE, drv);
    uint8_t **buf = (uint8_t **)dest;
    if (buf) {
        if (*buf) (*buf)+=PROTO_SIZE; // add protocol header at the front
    }
    return osize-PROTO_SIZE;
}

static void isn_trans_free(isn_layer_t *drv, const void *ptr) {
    isn_trans_t *obj = (isn_trans_t *)drv;
    const uint8_t *buf = ptr;
    if (buf) obj->parent->free(obj->parent, buf-PROTO_SIZE);
}

static int isn_trans_send(isn_layer_t *drv, void *dest, size_t size) {
    isn_trans_t *obj = (isn_trans_t *)drv;
    uint8_t *buf = dest;
    buf -= PROTO_SIZE;
    dest = buf;
    *(buf++) = ISN_PROTO_TRANL;
    *(buf++) = obj->port;
    *(buf++) = obj->drv.stats.tx_packets & 0xFF;
    *buf     = (obj->drv.stats.tx_packets >> 8) & 0x3F;
    obj->drv.stats.tx_packets++;
    obj->drv.stats.tx_counter+=size;
    return obj->parent->send(obj->parent, dest, size+PROTO_SIZE);
}

static size_t isn_trans_recv(isn_layer_t *drv, const void *src, size_t size, isn_layer_t *caller) {
    isn_trans_t *obj = (isn_trans_t *)drv;
    const uint8_t *buf = src;
    if (*buf == ISN_PROTO_TRANL) {
        obj->drv.stats.rx_counter += size - PROTO_SIZE;
        obj->drv.stats.rx_packets++;
        return obj->child->recv(obj->child, buf+PROTO_SIZE, size-PROTO_SIZE, drv) + PROTO_SIZE;
    }
    return 0;
}

void isn_translong_init(isn_trans_t *obj, isn_layer_t* child, isn_layer_t* parent, uint8_t port) {
    memset(&obj->drv, 0, sizeof(obj->drv));
    obj->drv.getsendbuf = isn_trans_getsendbuf;
    obj->drv.send       = isn_trans_send;
    obj->drv.recv       = isn_trans_recv;
    obj->drv.free       = isn_trans_free;
    obj->parent         = parent;
    obj->child          = child;
    obj->port           = port;
}

/** \} \endcond */
