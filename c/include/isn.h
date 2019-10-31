/** \file
 *  \brief ISN Protocl Abstract Class
 *  \author Uros Platise <uros@isotel.eu>
 *  \see https://www.isotel.eu/isn
 * 
 * \defgroup GR_ISN ISN Driver Abstract class
 * 
 * # Scope
 * 
 * The [ISOTEL Sensor Network Protocol](https://www.isotel.eu/isn/overview.html) 
 * defines a set of simple re-usable protocol objects that can be used as stand-alone
 * or combined, on-demand, into a complex protocol structures.
 * 
 * ISN Abtract Driver class defines the architecture of the 
 * [ISN Protocol Layer Stack implementation in C](https://www.isotel.eu/isn/), 
 * imitating object-like oriented programming. In this approach
 * individual layers may be arbitrarily stacked, chained, one with another to provide
 * desired protocol complexity. Structures can also be created on demand, dynamically.
 * 
 * # Concept
 * 
 * Each protocol layer, is described by the isn_driver_s structure (object)
 * 
 * - receives data from its **parent**, post-process received data, and forwards it to a **child** or more **children**.
 * - in reverse direction a child (children) respond to a request and send data back to this
 *   object, which pre-process it, and sends further to its parent.
 * 
 * The abstract class isn_driver_s therefore defines the two basic methods:
 * 
 * - isn_driver_s.recv() - to receive information from parent layer
 * - isn_driver_s.send() - to send back information to parent layer
 * 
 * And additional two methods to handle buffers:
 * 
 * - isn_driver_s.getsendbuf() - to pre-allocate sending buffer
 * - isn_driver_s.free() - to free it
 * 
 * The first two methods are best described in the following example, and
 * the second two are described in the later section.
 * 
 * ## Example
 * 
 * Common and very basic example:
 * 
 * \dot
 * digraph {
 *   rankdir=LR;
 *   ISN_UART -> ISN_FRAME [label="recv"]
 *   ISN_FRAME -> ISN_MSG [label="recv"]
 * 
 *   ISN_MSG -> ISN_FRAME [label="send"]
 *   ISN_FRAME -> ISN_UART [label="send"]
 * }
 * \enddot
 * 
 * The UART low-level phy driver is receiving information from its pin UART_RX.
 * However UART information typically does not provide sufficient framing information
 * to clearly denote start and end of a packet (it could with the use of the BREAK signal, 
 * which is unfortunately not widely used and not well supported in hardware).
 * 
 * To overcome this issue UART object forwards the received data to its child,
 * the low-overhead \ref GR_ISN_Frame layer via the recv() method. It can
 * unpack between 1..64 bytes with single byte over-head and optional 8-bit CRC.
 * It also clears buffers on timeout, and also captures out-of-frame information.
 * The payload of properly received information is thus forwarded to the next layer, 
 * the ISN Message layer to its isn_driver_s.recv() method.
 * 
 * The \ref GR_ISN_Message is a powerful replacement for printf() and scanf() specially
 * designed to meet sensors and sensor networks.
 * The 2-byte low-overhead protocol directly maps C structs into messages.
 * The host may communicate and interact over UART interface with the device, 
 * retrieving descriptive information from the device with dynamic content,
 * setting (configuring) it, with very low overhead, by re-transmitting only
 * the arguments in their raw form.
 * 
 * As Message layer receives a request or an update via UART, and FRAME layer, 
 * it generates a response, which is then send by calling the isn_driver_s.send() method of
 * the parent FRAME protocol layer. 
 * FRAME layer pre-process the data by adding the header and optional CRC.
 * Then it calls its parent, the UART layer, which finally sends the resulting packet over the PHY.
 * 
 * ## Buffers
 * 
 * To avoid copying the data between the layers, the isn_driver_s proposes
 * the following concept:
 * 
 * The last object in the queue (as Message Layer in above example), calls a 
 * chained request to the top-most parent (via Frame Layer to the UART layer)
 * via isn_driver_s.getsendbuf() to pre-allocate a buffer of some size. 
 * Each individual layer in this chain is adding its size to it, for example:
 * 
 * - Message Layer requires 5 bytes to send,
 * - Frame Layer adds another 2 for its header and CRC,
 * - so UART Layer receives a request for 7 bytes.
 * 
 * The top-most parent checks about the availability, and size and returns pointer to it.
 * It may also suggest a lower size if the required size is not available, and is then
 * up to the initiator to decide, whether it can pack its information to the smaller
 * sized buffer than required. Otherwise it must call isn_driver_s.free() method.
 * As the buffer is obtained it may place its data to it and passes information 
 * with the isn_driver_s.send() to its parent.
 * 
 * The same method isn_driver_s.getsendbuf() can also be used just to check
 * buffer availability in which case pointer to destination pointer is not
 * given NULL. In this case buffer will not be allocated by the UART object.
 * 
 * Advantage of this concept is reduced copying and code simplification, which
 * requires that only the end-points provide sufficient I/O buffering to 
 * satisfy transmission (speed) requirements.
 * 
 * # Protocol Construction
 * 
 * Each protocol has an unique 8-bit identifier, denoted by macros ISN_PROTO_...
 * in the isn.h. 
 * 
 * Each protocol object checks the incoming payload against the identifier,
 * and when it accepts the packet then the isn_driver_s.recv() returns pointer
 * to the same buffer, if it nacks, it returns NULL.
 * 
 * In the above UART example the protocol structure is strait forward, 
 * with only one possible solution. Let's look at the following example:
 * 
 * \dot
 * digraph {
 *   rankdir=LR;
 *   ISN_UART -> ISN_FRAME [label="recv"]
 *   ISN_FRAME -> ISN_DISPATCH [label="recv"]
 *   ISN_FRAME -> "ISN_OTHER (Ping)" [label="recv"]
 *   ISN_DISPATCH -> ISN_USER [label="recv"]
 *   ISN_DISPATCH -> ISN_MSG [label="recv"]
 * 
 *   ISN_MSG -> ISN_FRAME [label="send"]
 *   ISN_USER -> ISN_FRAME [label="send"]
 *   ISN_FRAME -> ISN_UART [label="send"]
 * }
 * \enddot
 * 
 * In this more complex case an input packet could represent some proprietary 
 * stream packed into the USER stream, and in parallel, the device provides
 * a message layer.
 * Because the UART packet has indeterministic frame, the first object is still
 * the FRAME layer, but now followed with the DISPATCH object. This particular
 * object checks for the protocol identifier and forwards the data to appropriate
 * child. 
 * 
 * However one may notice that the DISPATCH layer is only found in one 
 * direction.
 * A protocol object either provides all of the 4 methods, as are
 * \ref GR_ISN_Frame, \ref GR_ISN_Message, \ref GR_ISN_User and others 
 * or it can act just a receiver (isn_receiver_t) providing the isn_driver_s.recv()
 * method only, as are the \ref GR_ISN_Dispatch and \ref GR_ISN_Loopback objects.
 * 
 * ## The NULL as Ping Protocol
 * 
 * Host sends the 0x00 (NULL) Ping character as a way to send keep-alive signal to notify device
 * about active connection, and expects device to respond with a message or other 
 * protocol.
 * 
 * In the above UART case which first needs the FRAME layer for proper decoding of 
 * packets into frames, the FRAME layer provide another child target called `other`
 * in the isn_frame_init().
 * The `other` child captures all of the following information:
 * 
 * - ping signal, and
 * - ASCII characters that are within the range between 1..127 (standard terminal)
 * 
 * The device may also send back ASCII to the UART (as long it is in the 1..127 range).
 * Therefore ISN protocols allow simoultaneous operation of standard ASCII terminals
 * for easy configuration of the device in parallel to the ISN protocol stack 
 * without interference.
 * 
 * ## The NULL as Padding Byte
 * 
 * In addition the 0x00 character may be used as padding byte after certain protocol
 * to fill the packet side. This may be desirable in special cases, like speeding up
 * USB bulk transfers.
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

/**\{ */

#define ISN_PROTO_FRAME     0x80
#define ISN_PROTO_MSG       0x7F
#define ISN_PROTO_TRANS     0x7E
#define ISN_PROTO_TRANL     0x7D

#define ISN_PROTO_USERMAX   ISN_PROTO_USER15
#define ISN_PROTO_USER15    0x0f
#define ISN_PROTO_USER14    0x0e
#define ISN_PROTO_USER13    0x0d
#define ISN_PROTO_USER12    0x0c
#define ISN_PROTO_USER11    0x0b
#define ISN_PROTO_USER10    0x0a
#define ISN_PROTO_USER9     0x09
#define ISN_PROTO_USER8     0x08
#define ISN_PROTO_USER7     0x07
#define ISN_PROTO_USER6     0x06
#define ISN_PROTO_USER5     0x05
#define ISN_PROTO_USER4     0x04
#define ISN_PROTO_USER3     0x03
#define ISN_PROTO_USER2     0x02
#define ISN_PROTO_USER1     0x01

#define ISN_PROTO_PING      0x00

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
     * \returns src buf pointer, same as the one provided or NULL if receiver did not accept the packet
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
     * Free buffer provided by getsendbuf().
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
 * Callback event handler
 */
typedef void* (* isn_events_handler_t)(const void* arg);

/**\} */

/* Helpers */
#ifdef ARRAY_SIZE
# error "ARRAY_SIZE already defined"
#endif
#define ARRAY_SIZE(x)   (sizeof (x) / sizeof (*x))
#define LAMBDA(c_)      ({ c_ _;})
#define assert2(x)      (void)(x)

#ifndef UNUSED
#if defined(__GNUC__)
# define UNUSED(x) UNUSED_ ## x __attribute__((unused))
#elif defined(__LCLINT__)
# define UNUSED(x) /*@unused@*/ x
#else
# define UNUSED(x) x
#endif
#endif

#endif
