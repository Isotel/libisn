/** \file
 *  \brief ISN Protocol Receiver Duplicate Implementation
 *  \author Uros Platise <uros@isotel.eu>
 *  \see isn_dup.h
 * 
 * \cond Implementation
 * \addtogroup GR_ISN_Dup
 * 
 * The duplicator does not provide intermediate buffering, thus it is unable to store
 * data in the case one of the receive is unable to receive it. So it takes a simple 
 * approach by returning max() from both receiver. Means:
 * 
 * - if first receiver is able to take a whole data,
 * - and the other not, 
 * 
 * then the callee will receive the number of data received by the first receiver, and
 * the 2nd will loose this packet, or part of the packet. So this has to be considered
 * in the design of a protocol stack, and interediate buffering implemented if that
 * is not to happen.
 */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 * 
 * (c) Copyright 2019, Isotel, http://isotel.eu
 */

#include "isn_dup.h"

/**\{ */

static size_t isn_dup_recv(isn_layer_t *drv, const void *buf, size_t size, isn_layer_t *caller) {
    isn_dup_t *obj = (isn_dup_t *)drv;
    size_t recv1 = obj->childs[0]->recv(obj->childs[0], buf, size, caller);
    size_t recv2 = obj->childs[1]->recv(obj->childs[1], buf, size, caller);
    if (recv1 != recv2) obj->dup_errors++;
    return (recv1 > recv2) ? recv1 : recv2;
}

void isn_dup_init(isn_dup_t *obj, isn_layer_t *child1, isn_layer_t *child2) {
    obj->drv.recv  = isn_dup_recv;
    obj->childs[0] = (isn_receiver_t *)child1;
    obj->childs[1] = (isn_receiver_t *)child2;
    obj->dup_errors = 0;
}

/** \} \endcond */
