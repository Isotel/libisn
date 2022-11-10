/** \file
 *  \brief ISN Protocol Receiver Dispatcher
 *  \author Uros Platise <uros@isotel.org>
 *  \see isn_dispatch.c
 */
/**
 * \ingroup GR_ISN
 * \defgroup GR_ISN_Dispatch Protocol Dispatcher
 * 
 * # Scope
 * 
 * Implements the ISN Dispatch Driver as a supporting element in building
 * more complex structures. It is fully transparent to the Protocol Structure, 
 * does not alter any data.
 * 
 * # Concept
 * 
 * It is a receiver only object, and dispatches the incoming data
 * based on a list of provided children objects given by isn_bindings_t.
 * 
 * List must be terminated either by:
 * 
 * - `ISN_PROTO_LISTEND`, or
 * - `ISN_PROTO_OTHER` with a valid receiver, which would also receive data if none of the previously
 *   matched protocols on the list matches.
 * 
 * Dispatcher can only work properly after data is properly formatted, means, that
 * the first byte in the packet provides the next protocol ID. 
 * Typically it is a child of \ref GR_ISN_Frame, \ref GR_ISN_PSoC_USBFS, UDP Layer and others.
 * 
 * An example of usage, dispatching information among:
 * 
 * - proprietary User protocol 1 receiving D/A, stream, 
 * - Message layer for device configuration, and 
 * - Ping - keep alive receiver
 * 
 * ~~~
 * static isn_bindings_t isn_bindings[] = {
 *    {ISN_PROTO_USER1, &isn_user},
 *    {ISN_PROTO_MSG, &isn_message},
*     {ISN_PROTO_PING, &(isn_receiver_t){ping_recv} },
 *    {ISN_PROTO_LISTEND, NULL}
 * };
 * 
 * isn_dispatch_init(&isn_dispatch, isn_bindings);
 * isn_usbfs_init(&isn_usbfs, USBFS_DWR_POWER_OPERATION, &isn_dispatch);
 * ~~~
 */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 * 
 * (c) Copyright 2019, Isotel, http://isotel.org
 */

#ifndef __ISN_DISPATCH_H__
#define __ISN_DISPATCH_H__

#include "isn_def.h"

#ifdef __cplusplus
extern "C"
{
#endif

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

isn_dispatch_t* isn_dispatch_create();

void isn_dispatch_drop(isn_dispatch_t *obj);

/** Dispatcher
 * 
 * \param obj
 * \param childs binding layers
 */
void isn_dispatch_init(isn_dispatch_t *obj, isn_bindings_t* childs);

#ifdef __cplusplus
}
#endif

#endif
