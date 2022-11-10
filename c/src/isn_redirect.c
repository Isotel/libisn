/** \file
 *  \brief ISN Redirect Driver Implementation
 *  \author Uros Platise <uros@isotel.org>
 *  \see isn_redirect.h
 */
/**
 * \ingroup GR_ISN
 * \cond Implementation
 * \addtogroup GR_ISN_Redirect
 */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * (c) Copyright 2019, Isotel, http://isotel.org
 */

#include <string.h>
#include <stdlib.h>
#include "isn_redirect.h"

/**\{ */

static size_t isn_redirect_recv(isn_layer_t *drv, const void *src, size_t size, isn_layer_t *caller) {
    isn_redirect_t *obj = (isn_redirect_t *)drv;
    isn_driver_t *target = (obj->target) ? obj->target : (isn_driver_t *)caller;
    void *obuf = NULL;
    int bs = target->getsendbuf(target, &obuf, size, caller);
    if ( bs == size || (bs > 0 && obj->en_fragment) ) {
        isn_memcpy(obuf, src, bs);
        target->send(target, obuf, bs);
        obj->drv.stats.tx_counter += bs;
        return bs;
    }
    else if (obuf) {
        target->free(target, obuf);
    }
    obj->drv.stats.tx_retries++;
    return 0;
}

void isn_redirect_init(isn_redirect_t *obj, isn_layer_t* target) {
    ASSERT(obj);
    memset(&obj->drv, 0, sizeof(obj->drv));
    obj->drv.recv = isn_redirect_recv;
    obj->target   = target;
    obj->en_fragment=0;
}

isn_redirect_t* isn_redirect_create() {
    isn_redirect_t* obj = malloc(sizeof(isn_redirect_t));
    return obj;
}

void isn_redirect_drop(isn_redirect_t *obj) {
    free(obj);
}

/** \} \endcond */
