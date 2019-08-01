/** \file
 *  \author Uros Platise <uros@isotel.eu>
 *  \see https://www.isotel.eu/isn/frame.html
 */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 * 
 * (c) Copyright 2019, Isotel, http://isotel.eu
 */

#ifndef __ISN_FRAME_H__
#define __ISN_FRAME_H__

#include "isn.h"

/*--------------------------------------------------------------------*/
/* DEFINITIONS                                                        */
/*--------------------------------------------------------------------*/

#define ISN_FRAME_MAXSIZE   64      ///< max short/compact frame len

typedef enum {
    ISN_FRAME_MODE_SHORT    = 0,    ///< 1-byte overhead (header)
    ISN_FRAME_MODE_COMPACT  = 1     ///< 2-bytes overhead (header + 8-bit crc)
}
isn_frame_mode_t;

typedef struct {
    /* ISN Abstract Class Driver */
    isn_driver_t drv;

    /* Private data */
    isn_driver_t* child;
    isn_driver_t* other;
    isn_driver_t* parent;
    isn_frame_mode_t crc_enabled;
    volatile uint32_t *sys_counter;
    uint32_t frame_timeout;

    uint8_t state;
    uint8_t crc;
    uint8_t recv_buf[ISN_FRAME_MAXSIZE];
    uint8_t recv_size;
    uint8_t recv_len;
    uint32_t last_ts;
}
isn_frame_t;

/*----------------------------------------------------------------------*/
/* Public functions                                                     */
/*----------------------------------------------------------------------*/

/** Short and Compact Frame Layer
 * 
 * \param mode selects short (without CRC) or compact (with CRC) which is typically used over noisy lines, as UART
 * \param bindings provide a map of possible child protocols, and terminate with the PROTOCOL_OTHERWISE which provides 
 *        a bypass to raw terminal
 * \param parent protocol layer, which is typically a PHY, or UART or USBUART, ..
 * \param counter a pointer to a free running counter at arbitrary frequency
 * \param timeout defines period with reference to the counter after which reception is treated as invalid and to be discarded
 *        A 100 ms is a good choice.
 */
void isn_frame_init(isn_frame_t *obj, isn_frame_mode_t mode, isn_layer_t* child, isn_layer_t* other, isn_layer_t* parent, volatile uint32_t *counter, uint32_t timeout);

#endif
