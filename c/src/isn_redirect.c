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

static size_t isn_redirect_recv(isn_layer_t *drv, const void *src, size_t size, isn_layer_t *caller) {
    isn_redirect_t *obj = (isn_redirect_t *)drv;
    isn_driver_t *target = (obj->target) ? obj->target : (isn_driver_t *)caller;
    void *obuf = NULL;
    int bs = target->getsendbuf(target, &obuf, size, drv);
    if ( bs == size || (bs > 0 && obj->en_fragment) ) {
        memcpy(obuf, src, bs);
        target->send(target, obuf, bs);
        obj->drv.stats.tx_counter += bs;
        return bs;
    }
    obj->drv.stats.tx_retries++;
    target->free(target, obuf);
    return 0;
}

void isn_redirect_init(isn_redirect_t *obj, isn_layer_t* target) {
    memset(&obj->drv, 0, sizeof(obj->drv));
    obj->drv.recv = isn_redirect_recv;
    obj->target   = target;
    obj->en_fragment=0;
}

/** \} \endcond */
