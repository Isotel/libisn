/** \file
 *  \brief ISN User (Custom) Protocol Layer
 *  \author Uros Platise <uros@isotel.eu>
 *  \see https://www.isotel.eu/isn/user.html
 * 
 * \defgroup GR_ISN_User ISN Driver for User Layer
 * 
 * # Scope
 * 
 * Implements Device side of the [ISN User Layer Protocol](https://www.isotel.eu/isn/user.html)
 * that allows the device to encapsulate custom streams to be exchanged with the external entity.
 * These streams may be represented on the external entity, as IDM host side, as telnet ports,
 * which allow seamless integration with 3rd party software.
 * 
 * # Concept
 * 
 * User layer is a single byte overhead protocol defined by the `ISN_PROTO_USERx` macros in
 * the isn.h.
 * 
 * User should implement at least the receiving object, and may generate (send) data directly
 * from user methods. 
 * 
 * Example of user stream USER1 attached to the USBFS directly:
 * ~~~
 * void sendstr(const char *str, size_t len) {
 *     void *obuf;
 *     if (isn_user.drv.getsendbuf(&isn_user, &obuf, len) == len) { // we want exactly the required amount otherwise free it
 *         memcpy(obuf, str, len);
 *         isn_user.drv.send(&isn_user, obuf, len);
 *     }
 *     else {
 *         isn_user.drv.free(&isn_user, obuf);
 *     }
 * }
 * 
 * const void * userstream_recv(isn_layer_t *drv, const void *src, size_t size, isn_driver_t *caller) {    
 *     sendstr(src, size);    // echo back
 * }
 * 
 * isn_usbfs_init(&isn_usbfs, USBFS_DWR_POWER_OPERATION, &isn_user);
 * isn_user_init(&isn_user, &(isn_receiver_t){userstream_recv}, &isn_usbfs, ISN_PROTO_USER1);
 * ~~~
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

#include "isn_def.h"

#ifdef __cplusplus
extern "C" {
#endif

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

#ifdef __cplusplus
}
#endif

#endif
