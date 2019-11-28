/** \file
 *  \brief ISN Redirect Driver Implementation
 *  \author Uros Platise <uros@isotel.eu>
 *  \see isn_redirect.h
 * 
 * \cond Implementation
 * \addtogroup GR_ISN_Redirect
 */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 * 
 * (c) Copyright 2019, Isotel, http://isotel.eu
 */

#include <string.h>
#include "isn_redirect.h"

/**\{ */

static const void* isn_redirect_recv(isn_layer_t *drv, const void *src, size_t size, isn_driver_t *caller) {
    isn_redirect_t *obj = (isn_redirect_t *)drv;
    isn_driver_t *target = (obj->target) ? obj->target : caller;
    void *obuf = NULL;
    if ( target->getsendbuf(target, &obuf, size)==size ) {
        memcpy(obuf, src, size);
        target->send(target, obuf, size);
    }
    else {
        target->free(target, obuf);
    }
    return src;
}

void isn_redirect_init(isn_redirect_t *obj, isn_layer_t* target) {
    obj->drv.recv = isn_redirect_recv;
    obj->target   = target;
}

/** \} \endcond */
