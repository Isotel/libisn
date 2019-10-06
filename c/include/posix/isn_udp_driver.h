/** \file
 *  \brief Isotel Sensor Network UDP Driver Implementation
 *  \author Stanislav <stanislav@isotel.eu>, Uros Platise <uros@isotel.eu>
 *
 * \defgroup GR_ISN_UDP ISN POSIX UDP Driver
 *
 * # Scope
 *
 * Implements UDP server and client device driver for the POSIX 
 * environment.
 * 
 * # Usage
 * 
 * UDP provides a packet oriented transfer. The receiver should
 * first implement a dispatch layer (\ref GR_ISN_Dispatch) to 
 * decode the PING and other (as typically Message) layers.
 * 
 * Instance is created with the isn_udp_driver_create() given the 
 * server listening port, which port is also used as outgoing udp 
 * port. Use 0 for any port.
 * Clients may be added at any time using the isn_udp_driver_addclient().
 */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 * 
 * (c) Copyright 2019, Isotel, http://isotel.eu
 */

#ifndef ISN_UDP_DRIVER_H
#define ISN_UDP_DRIVER_H

#include "isn_logger.h"

typedef struct isn_udp_driver_s isn_udp_driver_t;
typedef int64_t time_ms_t;

/**
 * Create a new UDP driver instance
 * 
 * \param port listening port, or use 0 for any port
 * \param child binding layer
 * \returns a valid isn_udp_driver_t instance or NULL or error with errno set.
 */
isn_udp_driver_t *isn_udp_driver_create(uint16_t serverport, isn_layer_t *child);

/**
 * Add a new UDP client
 * 
 * \param isn_udp_driver_t structure
 * \param hostname a string containing either hostname or IP
 * \param port is a string containing service name or port number
 * \returns non-zero on success
 */
int isn_udp_driver_addclient(isn_udp_driver_t *driver, const char *hostname, const char *port);

/**
 * Free UDP driver instance
 * 
 * Make sure other layers have released all handles to it prior calling this function.
 */
void isn_udp_driver_free(isn_udp_driver_t *driver);

/**
 * Poll for incoming data and process it.
 */
int isn_udp_driver_poll(isn_udp_driver_t *driver, time_ms_t timeout);

/**
 * Set logger (debugging) level
 */
void isn_udp_driver_setlogging(isn_logger_level_t level);

#endif //ISN_UDP_DRIVER_H
