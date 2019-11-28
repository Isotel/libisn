/** \file
 *  \brief ISN Short and Compact (with CRC) Frame Protocol up to 64 B frames
 *  \author Uros Platise <uros@isotel.eu>
 *  \see https://www.isotel.eu/isn/frame.html
 * 
 * \defgroup GR_ISN_Frame ISN Driver for Frame Layer
 * 
 * # Scope
 * 
 * Implements Device side of the [ISN Frame Layer Protocol](https://www.isotel.eu/isn/frame.html)
 * which encapsulated ordered data into a streams, and decapsulates data from the unordered streams.
 * It is a single byte overhead protocol and may pack from 1 to 64 bytes with optional 8-bit CRC
 * appended at the end.
 * 
 * # Concept
 * 
 * Streaming (unordered) devices such as are \ref GR_ISN_PSoC_UART, \ref GR_ISN_PSoC_USBUART, \ref GR_ISN_User,
 * and others, do not provide sufficient framing information to denote where are the start and end
 * of the data within some packet.
 * This is necessary in order to be able to post-process the received information by other protocols.
 * 
 * This frame objects pre-appends data with a frame start pre-amble
 * that also consists of payload length in a single byte and comes in two modes:
 * 
 * - `ISN_FRAME_MODE_SHORT` (1-byte overhead),
 * - `ISN_FRAME_MODE_COMPACT` which appends CRC at the end (2-bytes overhead)
 * 
 * An example of usage:
 * ~~~
 * const void * terminal_recv(isn_layer_t *drv, const void *src, size_t size, isn_driver_t *caller) {
 *     void *obuf = NULL;
 *     const uint8_t *buf = src;
 * 
 *     if (size==1 && *buf==ISN_PROTO_PING) {
 *         isn_msg_send(&isn_message, 1, ISN_MSG_PRI_NORMAL);
 *     }
 *     else {
 *         // Echo everything back to the terminal
 *         if ( caller->getsendbuf(caller, &obuf, size)==size ) {
 *             memcpy(obuf, buf, size);
 *             caller->send(caller, obuf, size);
 *         }
 *         else {
 *             caller->free(caller, obuf);
 *         }
 *     }
 *     return buf;
 * }
 * 
 * isn_frame_init(&isn_frame, ISN_FRAME_MODE_COMPACT, &isn_dispatch, &(isn_receiver_t){terminal_recv}, &isn_usbuart, &counter_1kHz, 100);
 * isn_usbuart_init(&isn_usbuart, USBUART_3V_OPERATION, &isn_frame);
 * ~~~
 * where:
 *   - USBUART layer is the top parent which received data is framed with the CRC,
 *   - FRAME layer forwards valid frames to the \ref GR_ISN_Dispatch to handle multiple children as (\ref GR_ISN_Message, \ref GR_ISN_User)
 *   - and in addition captures non-framed data by the terminal_recv() which checks for:
 *     - Ping Signal, in which case it marks the first Message to be sent out,
 *     - Otherwise it treats incoming data as terminal I/O; in this example it simply echos it back to the `caller`
 *   - FRAME layer requires a free running counter variable to detect timeouts, which period is defined in LSB counts of that same
 *     variable; so for an 1 kHz free running counter, the timeout in above case is 100 ms.
 * 
 * # Receiver
 * 
 * As a receiver it may accept two kind of data:
 * 
 * 1. Terminal data in range from 0..127, and
 * 2. Framed data starting with a header which value is always >= 0x80
 * 
 * ## Terminal data
 * 
 * As long frame does not detect a frame preamble byte (>=0x80) it operates in terminal mode.
 * All the data received are passed over to the `other` child. The device may use this receiver
 * to support a standard terminal I/O mode, as well as to detect a Ping signal, see \ref GR_ISN.
 * 
 * ## Framed Data
 * 
 * In this case it waits for a valid pre-amble identifier, when detected it starts collecting received data.
 * If CRC is enabled, it must end with a proper CRC otherwise data is discarded and error counter incremented.
 * To recover from errors this implementation uses a frame timeout principle, which defines the maximum
 * time between two successive recv(), in which case if packet is terminated in-between buffer is flushed
 * and error counter is incremented.
 * 
 * Advanced techniques as implemented on the host side, like Isotel Precision IDM, walk over the stream
 * to detect header with respective valid crc to resynchronize even in cases where stream is dense and 
 * timeout technique would not work.
 * 
 * The use of CRC, thus `ISN_FRAME_MODE_COMPACT` mode, is suggested on all noisy streams.
 * The use of SHORT protocol is suggested as inner layers on the top of some reliable protocol, like USB.
 * 
 * # Sender
 * 
 * As a sender this object receives ordered data from its child, adds header and optional CRC, and 
 * sends data to its parent.
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

#include "isn_def.h"

#ifdef __cplusplus
extern "C" {
#endif

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
    uint32_t rx_frames;
    uint32_t rx_errors;
    uint32_t last_ts;
}
isn_frame_t;

/*----------------------------------------------------------------------*/
/* Public functions                                                     */
/*----------------------------------------------------------------------*/

/** Short and Compact Frame Layer
 * 
 * \param mode selects short (without CRC) or compact (with CRC) which is typically used over noisy lines, as UART
 * \param child layer
 * \param other layer to which all the traffic that is outside the frames is redirected, like terminal I/O
 * \param parent protocol layer, which is typically a PHY, or UART or USBUART, ..
 * \param counter a pointer to a free running counter at arbitrary frequency
 * \param timeout defines period with reference to the counter after which reception is treated as invalid and to be discarded
 *        A 100 ms is a good choice.
 */
void isn_frame_init(isn_frame_t *obj, isn_frame_mode_t mode, isn_layer_t* child, isn_layer_t* other, isn_layer_t* parent, volatile uint32_t *counter, uint32_t timeout);

#ifdef __cplusplus
}
#endif

#endif
