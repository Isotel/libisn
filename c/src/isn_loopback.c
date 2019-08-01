/** \file
 *  \author Uros Platise <uros@isotel.eu>
 *  \see isn_loopback.h
 */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 * 
 * (c) Copyright 2019, Isotel, http://isotel.eu
 */

#include "isn_loopback.h"

static const void* isn_loopback_recv(isn_layer_t *drv, const void *src, size_t size, isn_driver_t *caller) {
    isn_loopback_t *obj = (isn_loopback_t *)drv;
    void *obuf = NULL;
    if ( obj->target->getsendbuf(obj->target, &obuf, size)==size ) {
        memcpy(obuf, src, size);
        obj->target->send(obj->target, obuf, size);
    }
    else {
        obj->target->free(obj->target, obuf);
    }
    return src;
}

void isn_loopback_init(isn_loopback_t *obj, isn_layer_t* target) {
    obj->drv.recv = isn_loopback_recv;
    obj->target   = target;
}
