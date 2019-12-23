/** \file
 *  \brief ISN Transport Layer
 *  \author Uros Platise <uros@isotel.eu>
 *  \see isn_trans.h
 * 
 * \cond Implementation
 * \addtogroup GR_ISN_Trans
 */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * (c) Copyright 2019, Isotel, http://isotel.eu
 */

#include "isn_trans.h"

#define PROTO_SIZE  4

/**\{ */

static int isn_trans_getsendbuf(isn_layer_t *drv, void **dest, size_t size) {
    isn_trans_t *obj = (isn_trans_t *)drv;
    int osize = obj->parent->getsendbuf(obj->parent, dest, size+PROTO_SIZE);
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
    *(buf++) = obj->txcounter & 0xFF;
    *buf     = (obj->txcounter >> 8) & 0xFF;
    obj->txcounter += 1;
    return obj->parent->send(obj->parent, dest, size+PROTO_SIZE);
}

static size_t isn_trans_recv(isn_layer_t *drv, const void *src, size_t size, isn_driver_t *caller) {
    isn_trans_t *obj = (isn_trans_t *)drv;
    const uint8_t *buf = src;
    if (*buf == ISN_PROTO_TRANL) {
        return obj->child->recv(obj->child, buf+PROTO_SIZE, size-PROTO_SIZE, drv) + PROTO_SIZE;
    }
    return 0;
}

void isn_translong_init(isn_trans_t *obj, isn_layer_t* child, isn_layer_t* parent, uint8_t port) {
    obj->drv.getsendbuf = isn_trans_getsendbuf;
    obj->drv.send       = isn_trans_send;
    obj->drv.recv       = isn_trans_recv;
    obj->drv.free       = isn_trans_free;
    obj->parent         = parent;
    obj->child          = child;
    obj->port           = port;
    obj->txcounter      = 0;
}

/** \} \endcond */
