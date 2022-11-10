/** \file
 *  \brief ISN Transport Layer
 *  \author Uros Platise <uros@isotel.org>
 *  \see isn_trans.h
 */
/**
 * \ingroup GR_ISN
 * \cond Implementation
 * \addtogroup GR_ISN_Trans
 *
 * Implemented 6-bit port simple static port binding mechanism.
 *
 * \todo Rx/Tx counters, order alignment, missing packet detection, etc, ..
 *
 * \todo Dynamic binding in case of mapping to external devices, as wireless devices, etc.
 */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * (c) Copyright 2019 - 2022, Isotel, http://isotel.org
 */

#include <string.h>
#include <stdlib.h>
#include "isn_trans.h"

#define PROTO_SIZE  2

/**\{ */

static int isn_trans_getsendbuf(isn_layer_t *drv, void **dest, size_t size, const isn_layer_t *caller) {
    isn_trans_t *obj = (isn_trans_t *)drv;
    size_t port = 0;

    // search for the layer, ... 2nd faster option is below
    for (; port<obj->tbl_size && obj->tbl[port].driver != caller; port++);
    if (port >= obj->tbl_size) return 0;

    //size_t port = (const isn_trans_dispatchtbl_t *)caller - obj->tbl;
    //if (port > obj->tbl_size) return 0; // Invalid port

    int osize = obj->parent->getsendbuf(obj->parent, dest, size+PROTO_SIZE, caller);
    uint8_t **buf = (uint8_t **)dest;
    if (buf) {
        if (*buf) {
            *((*buf)++) = ISN_PROTO_TRANS;
            *((*buf)++) = port << 2;
        }
    }
    return osize - PROTO_SIZE;
}

static void isn_trans_free(isn_layer_t *drv, const void *ptr) {
    isn_trans_t *obj = (isn_trans_t *)drv;
    const uint8_t *buf = ptr;
    if (buf) obj->parent->free(obj->parent, buf-PROTO_SIZE);
}

static int isn_trans_send(isn_layer_t *drv, void *dest, size_t size) {
    isn_trans_t *obj = (isn_trans_t *)drv;
    uint8_t *buf = dest;
    uint8_t port = *(--buf) >> 2;
    *buf |= obj->tbl[port].tx_counter & 0x03;
    obj->tbl[port].tx_counter++;
    obj->drv.stats.tx_packets++;
    obj->drv.stats.tx_counter += size;
    return obj->parent->send(obj->parent, buf - 1, size + PROTO_SIZE);
}

static size_t isn_trans_recv(isn_layer_t *drv, const void *src, size_t size, isn_layer_t *caller) {
    isn_trans_t *obj = (isn_trans_t *)drv;
    const uint8_t *buf = src;

    if (src && size) {
        if (*buf++ == ISN_PROTO_TRANS) {
            uint8_t portNcnt = *buf++;
            // \todo packet queing not yet supported, see notes in the .h header
            //       currently we accept everything

            uint8_t port = portNcnt >> 2;
            if (port < obj->tbl_size) {
                obj->drv.stats.rx_counter += size - PROTO_SIZE;
                obj->drv.stats.rx_packets++;
                isn_receiver_t *driver = (isn_receiver_t *)obj->tbl[port].driver;
                return driver->recv(driver, buf, size-PROTO_SIZE, drv) + PROTO_SIZE;
            }
        }
    }
    obj->drv.stats.rx_dropped++;
    return size;
}

void isn_trans_init(isn_trans_t *obj, isn_trans_dispatchtbl_t *tbl, size_t tbl_size, isn_layer_t* parent) {
    ASSERT(obj);
    ASSERT(tbl);
    ASSERT(tbl_size <= 64);
    ASSERT(parent);
    memset(&obj->drv, 0, sizeof(obj->drv));
    obj->drv.getsendbuf = isn_trans_getsendbuf;
    obj->drv.send       = isn_trans_send;
    obj->drv.recv       = isn_trans_recv;
    obj->drv.free       = isn_trans_free;
    obj->parent         = parent;
    obj->tbl            = tbl;
    obj->tbl_size       = tbl_size;
    for (size_t i=0; i<tbl_size; i++) tbl[i].rx_counter = tbl[i].tx_counter = tbl[i].rx_dropped = 0;
}

isn_trans_t* isn_trans_create() {
    isn_trans_t* obj = malloc(sizeof(isn_trans_t));
    return obj;
}

void isn_trans_drop(isn_trans_t *obj) {
    free(obj);
}

/** \} \endcond */
