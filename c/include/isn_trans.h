/** \file
 *  \brief ISN Transport Layer
 *  \author Uros Platise <uros@isotel.org>
 *  \see https://www.isotel.org/isn/transport.html
 */
/**
 * \ingroup GR_ISN
 * \defgroup GR_ISN_Trans Transport Layer Driver
 *
 * # Scope
 *
 * Implements Device side of the [ISN Transport Protocol](https://www.isotel.org/isn/transport.html)
 *
 * # Concept
 *
 * Features:
 *
 * - short transport layer
 * - ports, 6-bit
 * - packet ordering (needs update in whole chain, as recv should return int instead of size_t)
 *
 * Notes:
 *
 * - RX buffer on reception of an aligned packet; protocol could reject the buffer with
 *   negative number, representing number of missing packets, i.e. 0 means reject but none is
 *   missing, -1 one is missing in between, ... this simplifies buffering scheme, and keeps
 *   the concept; buffering is at source and at sink, inbetween is zero-copy
 * - Specs are unaligned: tx counter is 16-bit and little endian, probably we will drop the
 *   old unaligned protocol
 *
 * 3: need 0, return -3
 * 0: need 0, return <ACK>
 * far: need 1, return <ACK> but internally drop, as we are too far from sync, and increment
 *      error counters in the trans protocol
 */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * (c) Copyright 2019 - 2022, Isotel, http://isotel.org
 */

#ifndef __ISN_TRANS_H__
#define __ISN_TRANS_H__

#include "isn_def.h"

#ifdef __cplusplus
extern "C" {
#endif

/*--------------------------------------------------------------------*/
/* DEFINITIONS                                                        */
/*--------------------------------------------------------------------*/

/** Transport Dispatch Table Entry
 *
 * Allocate n-entries for n-valid ports
 */
typedef struct {
    isn_layer_t *driver;    ///< Receiver callback, note that all ports could use the same receiver, as pointer to the element of this table is passed receiver can figure out the port number
    uint8_t tx_counter;     ///< Last send packet counter
    uint8_t rx_counter;     ///< Last received packet counter
    uint8_t rx_dropped;     ///< Incremented on unaligned packet drop, the receiver should keep eye on this parameter i.e. in streaming or stateful applications
}
ISN_PACKED_ALIGNED isn_trans_dispatchtbl_t; // as we only have bytes makes no sense to use not packed structure

typedef struct {
    /* ISN Abstract Class Driver */
    isn_driver_t drv;

    /* Private data */
    isn_driver_t* parent;
    isn_trans_dispatchtbl_t *tbl;
    size_t tbl_size;
}
isn_trans_t;


/*----------------------------------------------------------------------*/
/* Public functions                                                     */
/*----------------------------------------------------------------------*/

/** Create trans instance */
isn_trans_t* isn_trans_create();

/** Drop trans instance */
void isn_trans_drop(isn_trans_t *obj);

/** Initialize Short Transport Layer
 *
 * \param obj
 * \param tbl dispatch table, provide pointer to pre-allocated table with valid recv pointers, the other run-time parameters will be reset by this function
 * \param tbl_size number of entries (valid ports) of the dispatch table
 * \param parent protocol layer, which is typically a PHY, or FRAME
 */
void isn_trans_init(isn_trans_t *obj, isn_trans_dispatchtbl_t *tbl, size_t tbl_size, isn_layer_t* parent);

#ifdef __cplusplus
}
#endif

#endif
