/** \file
 *  \brief ISN Protocol Receiver Dispatcher Implementation
 *  \author Uros Platise <uros@isotel.org>
 *  \see isn_dispatch.h
 */
/**
 * \ingroup GR_ISN
 * \cond Implementation
 * \addtogroup GR_ISN_Dispatch
 */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * (c) Copyright 2019, Isotel, http://isotel.org
 */

#include "isn_dispatch.h"
#include <stdlib.h>

/**\{ */

static size_t isn_dispatch_recv(isn_layer_t *drv, const void *buf, size_t size, isn_layer_t *caller) {
    isn_dispatch_t *obj = (isn_dispatch_t *)drv;
    isn_bindings_t *child = obj->childs;
    int protocol = *(const uint8_t *)buf;

    // exceptions are frame protocols
    if ((protocol & ISN_PROTO_FRAME_MASK) == ISN_PROTO_FRAME) protocol = ISN_PROTO_FRAME;
    else if ((protocol & ISN_PROTO_FRAME_LONG_MASK) == ISN_PROTO_FRAME_LONG) protocol = ISN_PROTO_FRAME_LONG;
    else if ((protocol & ISN_PROTO_FRAME_JUMBO_MASK) == ISN_PROTO_FRAME_JUMBO) protocol = ISN_PROTO_FRAME_JUMBO;

    if (!buf || !size) return size;

    child--;
    do {
        child++;
        if (child->protocol == protocol || child->protocol == ISN_PROTO_OTHER) {
            isn_driver_t *driver = (isn_driver_t *)child->driver;
            if (driver->recv) {
                return driver->recv(driver, buf, size, caller);
            }
        }
    }
    while (child->protocol >= 0);
    return size;   // Ack all but account dropped packet
}

void isn_dispatch_init(isn_dispatch_t *obj, isn_bindings_t* childs) {
    ASSERT(obj);
    ASSERT(childs);
    obj->drv.recv = isn_dispatch_recv;
    obj->childs   = childs;
}

isn_dispatch_t* isn_dispatch_create() {
    isn_dispatch_t* obj = malloc(sizeof(isn_dispatch_t));
    return obj;
}

void isn_dispatch_drop(isn_dispatch_t *obj) {
    free(obj);
}

/** \} \endcond */
