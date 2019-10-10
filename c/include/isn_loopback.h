/** \file
 *  \brief ISN Loopback Driver
 *  \author Uros Platise <uros@isotel.eu>
 *  \see isn_loopback.c
 * 
 * \defgroup GR_ISN_Loopback ISN Loopback Driver
 * 
 * # Scope
 * 
 * Implements the ISN Loopback Driver as a supporting element in building
 * more complex structures. It is fully transparent to the Protocol Structure, 
 * does not alter any data.
 * 
 * # Concept
 * 
 * It is a receiver only object, and loops back the received data to the target object.
 * 
 * Example below is a bi-directional bridge and re-framer between the UART and USBFS where:
 * 
 * - UART is carrying data packed with the CRC (ISN_FRAME_MODE_COMPACT),
 * - and USBFS encapsulated the same data into a SHORT FRAME without the CRC (ISN_FRAME_MODE_SHORT).
 * ~~~
 * isn_uart_init(&isn_uart, &isn_cframe);
 * isn_frame_init(&isn_cframe, ISN_FRAME_MODE_COMPACT, &isn_loopback_uartrx, NULL, &isn_uart, &counter_1kHz, 100);
 * isn_loopback_init(&isn_loopback_uartrx, &isn_sframe);
 * isn_frame_init(&isn_sframe, ISN_FRAME_MODE_SHORT, &isn_loopback_uarttx, NULL, &isn_usbfs, &counter_1kHz, 100);
 * isn_loopback_init(&isn_loopback_uarttx, &isn_cframe); 
 * isn_usbfs_init(&isn_usbfs, USBFS_DWR_POWER_OPERATION, &isn_sframe);
 * ~~~
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
void isn_loopback_init(isn_loopback_t *obj, isn_layer_t* target);

#endif
