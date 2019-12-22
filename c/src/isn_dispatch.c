/** \file
 *  \brief ISN Protocol Receiver Dispatcher Implementation
 *  \author Uros Platise <uros@isotel.eu>
 *  \see isn_dispatch.h
 * 
 * \cond Implementation
 * \addtogroup GR_ISN_Dispatch
 */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 * 
 * (c) Copyright 2019, Isotel, http://isotel.eu
 */

#include "isn_dispatch.h"

/**\{ */

static const void * isn_dispatch_recv(isn_layer_t *drv, const void *buf, size_t size, isn_driver_t *caller) {
    isn_dispatch_t *obj = (isn_dispatch_t *)drv;
    isn_bindings_t *child = obj->childs;
    int protocol = *(const uint8_t *)buf;
    if (protocol >= 0x80) protocol = 0x80;  // exception is the FRAME single byte protocol

    child--;
    do {
        child++;
        if (child->protocol == protocol || child->protocol == ISN_PROTO_OTHER) {
            isn_driver_t *driver = (isn_driver_t *)child->driver;
            if (driver->recv) {
                assert2( driver->recv(driver, buf, size, caller) != NULL );
                return buf;
            }
        }
    }
    while (child->protocol >= 0);
    return NULL;
}

void isn_dispatch_init(isn_dispatch_t *obj, isn_bindings_t* childs) {
    obj->drv.recv = isn_dispatch_recv;
    obj->childs   = childs;
}

/** \} \endcond */
