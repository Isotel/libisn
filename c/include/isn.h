/** \file
 *  \author Uros Platise <uros@isotel.eu>
 *  \see https://www.isotel.eu/isn
 */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 * 
 * (c) Copyright 2019, Isotel, http://isotel.eu
 */

#ifndef __ISN_H__
#define __ISN_H__

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#define ISN_PROTO_FRAME     0x80
#define ISN_PROTO_MSG       0x7F
#define ISN_PROTO_TRANS     0x7E
#define ISN_PROTO_TRANL     0x7D

#define ISN_PROTO_USERMAX   ISN_PROTO_USER7
#define ISN_PROTO_USER7     0x07
#define ISN_PROTO_USER6     0x06
#define ISN_PROTO_USER5     0x05
#define ISN_PROTO_USER4     0x04
#define ISN_PROTO_USER3     0x03
#define ISN_PROTO_USER2     0x02
#define ISN_PROTO_USER1     0x01

#define ISN_PROTO_PING      0x00
#define ISN_PROTO_OTHERWISE -1

/** C-like Weak Abstract class of isn_layer_t */
typedef void isn_layer_t;

/**
 * ISN Layer (Driver)
 */
typedef struct isn_driver_s {
    /** Receive Data
     * 
     * If low-level driver have single buffer implementations then they will request
     * the buffer to be returned on return, to notify them that it's free. Multi-buffer
     * implementation may return NULL, and later release it with free().
     * 
     * \param src pointer to received data
     * \param size size of the received data
     * \param caller device driver structure, enbles simple echoing or multi-path replies
     * \returns buf pointer, same as the one provided or NULL
     */
    const void * (*recv)(isn_layer_t *drv, const void *src, size_t size, struct isn_driver_s *caller);

    /** Allocate buffer for transmission thru layers
     * 
     * If buf is NULL then function only performs a check on availability and returns
     * possible size for allocation. Once buffer is allocated it will be automatically
     * freed by the send() below. Otherwise user needs to call free() function below.
     * 
     * Note that size 0 still means availability, as it stands for empty packet.
     * If there is no space available function must return -1.
     * 
     * \param dest reference to a local pointer, which is updated, pointed to given/allocated buffer
     * \param size requested size
     * \returns obtained size, and buf pointer is set; if size cannot be obtained buf is (must be) set to NULL
     */
    int (*getsendbuf)(isn_layer_t *drv, void **dest, size_t size);

    /** Send Data
     * 
     * buf should be first allocated with the getsendbuf() which at the same time prepares space
     * for lower layers. buf returned by the getsendbuf() should be filled with data and 
     * passed to this function. It also frees the buffer (so user should not call free() 
     * function below)
     * 
     * \param dest returned by the getsendbuf()
     * \param size which should be equal or less than the one returned by the getsendbuf()
     * \return number of bytes sent
     */
    int (*send)(isn_layer_t *drv, void *dest, size_t size);

    /** Free Buffer
     * 
     * Free buffer either provided by getsendbuf() or as received by the recv()
     * 
     * Note: current implementations only have single buffer on receive, so it
     *   only relates to the getsendbuf()
     */
    void (*free)(isn_layer_t *drv, const void *ptr);
}
isn_driver_t;

/**
 * ISN Layer Receiver only
 */
typedef struct {
    const void * (*recv)(isn_layer_t *drv, const void *buf, size_t size, isn_driver_t *caller);
}
isn_receiver_t;

/**
 * ISN Protocol to Layer Drivers Bindings
 */
typedef struct {
    int protocol;
    isn_layer_t *driver;
}
isn_bindings_t;

/**
 * Callback event handler
 */
typedef void* (* isn_events_handler_t)(const void* arg);

/* Helpers */
#define SIZEOF(x)       (sizeof (x) / sizeof (*x))
#define LAMBDA(c_)      ({ c_ _;})
#define assert2(x)      (void)(x)

#endif
