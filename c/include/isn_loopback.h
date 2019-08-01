/** \file
 *  \author Uros Platise <uros@isotel.eu>
 */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 * 
 * (c) Copyright 2019, Isotel, http://isotel.eu
 */

#ifndef __ISN_LOOPBACK_H__
#define __ISN_LOOPBACK_H__

#include "isn.h"

/*--------------------------------------------------------------------*/
/* DEFINITIONS                                                        */
/*--------------------------------------------------------------------*/

typedef struct {
    /* ISN Abstract Class Driver */
    isn_receiver_t drv;

    /* Private data */
    isn_driver_t* target;
}
isn_loopback_t;

/*----------------------------------------------------------------------*/
/* Public functions                                                     */
/*----------------------------------------------------------------------*/

/** User Layer
 * 
 * \param target layer where data received should be copied to
 */
void isn_loopback_init(isn_user_t *obj, isn_layer_t* target);

#endif
