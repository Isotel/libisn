/** \file
 *  \brief ISN User (Custom) Protocol Layer Implementation
 *  \author Uros Platise <uros@isotel.eu>
 *  \see isn_user.h
 * 
 * \cond Implementation
 * \addtogroup GR_ISN_User
 */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 * 
 * (c) Copyright 2019, Isotel, http://isotel.eu
 */

#include "isn_user.h"

/**\{ */

static int isn_user_getsendbuf(isn_layer_t *drv, void **dest, size_t size) {
    isn_user_t *obj = (isn_user_t *)drv;
    int osize = obj->parent->getsendbuf(obj->parent, dest, size+1);
    uint8_t **buf = (uint8_t **)dest;
    if (buf) {
        if (*buf) (*buf)++; // add protocol header at the front
    }
    return osize-1;
}

static void isn_user_free(isn_layer_t *drv, const void *ptr) {
    isn_user_t *obj = (isn_user_t *)drv;
    const uint8_t *buf = ptr;
    if (buf) obj->parent->free(obj->parent, buf-1);
}

static int isn_user_send(isn_layer_t *drv, void *dest, size_t size) {
    isn_user_t *obj = (isn_user_t *)drv;
    uint8_t *buf = dest;
    *(--buf) = obj->user_id;
    return obj->parent->send(obj->parent, buf, size+1);    
}

static const void* isn_user_recv(isn_layer_t *drv, const void *src, size_t size, isn_driver_t *caller) {
    isn_user_t *obj = (isn_user_t *)drv;
    const uint8_t *buf = src;
    if (*buf == obj->user_id) {
        return obj->child->recv(obj->child, buf+1, size-1, drv);
    }
    return NULL;
}

void isn_user_init(isn_user_t *obj, isn_layer_t* child, isn_layer_t* parent, uint8_t user_id) {
    obj->drv.getsendbuf = isn_user_getsendbuf;
    obj->drv.send       = isn_user_send;
    obj->drv.recv       = isn_user_recv;
    obj->drv.free       = isn_user_free;
    obj->user_id        = user_id;
    obj->child          = child;
    obj->parent         = parent;
}

/** \} \endcond */
