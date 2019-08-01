/** \file
 *  \author Uros Platise <uros@isotel.eu>
 *  \see isn_dispatch.c
 */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 * 
 * (c) Copyright 2019, Isotel, http://isotel.eu
 */

#ifndef __ISN_DISPATCH_H__
#define __ISN_DISPATCH_H__

#include "isn.h"

/*--------------------------------------------------------------------*/
/* DEFINITIONS                                                        */
/*--------------------------------------------------------------------*/

/** Use this as protocol to match any of it, should be the last in the list */
#define ISN_PROTO_OTHER     -1

/** Otherwise use this to terminate the isn_bindings_t list, which is not needed if ISN_PROTO_OTHER is already given */
#define ISN_PROTO_LISTEND   -2

/**
 * ISN Protocol to Layer Drivers Bindings
 */
typedef struct {
    int protocol;
    isn_layer_t *driver;
}
isn_bindings_t;


typedef struct {
    /* ISN Abstract Class Driver */
    isn_receiver_t drv;

    /* Private data */
    isn_bindings_t* childs;
}
isn_dispatch_t;

/*----------------------------------------------------------------------*/
/* Public functions                                                     */
/*----------------------------------------------------------------------*/

/** Dispatcher
 * 
 * \param childs binding layers
 */
void isn_dispatch_init(isn_dispatch_t *obj, isn_bindings_t* childs);

#endif
