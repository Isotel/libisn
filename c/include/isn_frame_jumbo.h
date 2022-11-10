/** \file
 *  \brief ISN Jambo Frame Protocol up to 8192 B frames with 32-bit CRC
 *  \author Uros Platise <uros@isotel.org>
 *  \see https://www.isotel.org/isn/frame.html
 */
/**
 * \ingroup GR_ISN
 * \defgroup GR_ISN_Frame_Jumbo Jumbo Frame Layer Driver
 *
 * # Scope
 *
 * Implements Device side of the [ISN Frame Layer Protocol](https://www.isotel.org/isn/frame.html)
 * which encapsulated ordered data into a streams, and decapsulates data from the unordered streams.
 * It is a two byte overhead protocol and may pack from 1 to 8192 bytes with 32-bit CRC appended at the end.
 *
 * # Concept
 *
 * Streaming (unordered) devices such as are \ref GR_ISN_PSoC_UART, \ref GR_ISN_PSoC_USBUART, \ref GR_ISN_User,
 * and others, do not provide sufficient framing information to denote where are the start and end
 * of the data within some packet.
 * This is necessary in order to be able to post-process the received information by other protocols.
 */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * (c) Copyright 2022, Isotel, http://isotel.org
 */

#ifndef __ISN_FRAME_JUMBO_H__
#define __ISN_FRAME_JUMBO_H__

#include "isn_def.h"
#include "isn_clock.h"

#ifdef __cplusplus
extern "C" {
#endif

/*--------------------------------------------------------------------*/
/* DEFINITIONS                                                        */
/*--------------------------------------------------------------------*/

#define ISN_FRAME_JUMBO_MAXSIZE   8192   /// \todo Make below struct as template with adjustable static len

typedef struct {
    /* ISN Abstract Class Driver */
    isn_driver_t drv;

    /* Private data */
    isn_driver_t* child;
    isn_driver_t* other;
    isn_driver_t* parent;
    isn_clock_counter_t frame_timeout;

    uint32_t crc;
    uint8_t state;
    uint8_t recv_fwed;
    uint8_t recv_size;
    uint16_t recv_len;
    uint32_t last_ts;
    uint8_t recv_buf[ISN_FRAME_JUMBO_MAXSIZE];   // make this parameter user defined
}
isn_frame_jumbo_t;

/*----------------------------------------------------------------------*/
/* Public functions                                                     */
/*----------------------------------------------------------------------*/

/** Long Frame Layer
 *
 * \param obj
 * \param child layer
 * \param other layer to which all the traffic that is outside the frames is redirected, like terminal I/O
 * \param parent protocol layer, which is typically a PHY, or UART or USBUART, ..
 * \param timeout defines period with reference to the isn counter after which reception is treated as invalid and to be discarded.
 */
void isn_frame_jumbo_init(isn_frame_jumbo_t *obj, isn_layer_t* child, isn_layer_t* other, isn_layer_t* parent, isn_clock_counter_t timeout);

/** Creates an instance of a Short and Compact Frame Layer
 *  Instance is allocated with malloc and can be freed with isn_frame_drop()
 *
 * \returns object instance
 */
isn_frame_jumbo_t* isn_frame_jumbo_create();

/** Drops a valid instance created by isn_frame_create()
 */
void isn_frame_jumbo_drop(isn_frame_jumbo_t *obj);

#ifdef __cplusplus
}
#endif

#endif
