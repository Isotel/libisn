/** \file
 *  \brief ISN Protocol Receiver Duplicator
 *  \author Uros Platise <uros@isotel.org>
 *  \see isn_dup.c
 */
/**
 * \ingroup GR_ISN
 * \defgroup GR_ISN_Dup Duplicates Receiving Stream
 * 
 * # Scope
 * 
 * Implements the ISN Duplicate Driver as a supporting element in building
 * more complex structures. It is fully transparent to the Protocol Structure, 
 * does not alter any data.
 * 
 * # Concept
 * 
 * It is a receiver only object, and dispatches the incoming data to two receivers.
 * 
 * An example of usage, dispatching information among:
 * 
 * - uart is receiving data that is to be further redirected to some debug port
 * - and at the same time received locally
 * 
 * ~~~
 * isn_dup_init(&isn_dup, &isn_redirect1, &isn_cframe);
 * ~~~
 */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 * 
 * (c) Copyright 2020, Isotel, http://isotel.org
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

typedef struct {
    /* ISN Abstract Class Driver */
    isn_receiver_t drv;

    /* Private data */
    isn_receiver_t* childs[2];

    size_t dup_errors;  ///< Incremented when one of the receiver was unable to receive data
}
isn_dup_t;

/*----------------------------------------------------------------------*/
/* Public functions                                                     */
/*----------------------------------------------------------------------*/

/** Dispatcher
 * 
 * \param obj
 * \param child1 the first receiving stream
 * \param child2 and the duplicate receiving stream
 */
void isn_dup_init(isn_dup_t *obj, isn_layer_t *child1, isn_layer_t *child2);

#ifdef __cplusplus
}
#endif

#endif
