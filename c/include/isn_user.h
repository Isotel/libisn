/** \file
 *  \author Uros Platise <uros@isotel.eu>
 *  \see https://www.isotel.eu/isn/user.html
 */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 * 
 * (c) Copyright 2019, Isotel, http://isotel.eu
 */

#ifndef __ISN_USER_H__
#define __ISN_USER_H__

#include "isn.h"

/*--------------------------------------------------------------------*/
/* DEFINITIONS                                                        */
/*--------------------------------------------------------------------*/

typedef struct {
    /* ISN Abstract Class Driver */
    isn_driver_t drv;

    /* Private data */
    isn_driver_t* parent;
    isn_driver_t* child;

    uint8_t user_id;
}
isn_user_t;

/*----------------------------------------------------------------------*/
/* Public functions                                                     */
/*----------------------------------------------------------------------*/

/** User Layer
 * 
 * \param child layer
 * \param parent protocol layer, which is typically a PHY, or FRAME
 * \param user_id user id from ISN_PROTO_USERx
 */
void isn_user_init(isn_user_t *obj, isn_layer_t* child, isn_layer_t* parent, uint8_t user_id);

#endif
