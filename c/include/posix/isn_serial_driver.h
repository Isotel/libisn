/** \file
 *  \brief ISN serial Driver Implementation
 *  \author Stanislav <stanislav@isotel.eu>, Uros Platise <uros@isotel.eu>
 *
 * \defgroup GR_ISN_SERIAL ISN POSIX serial Driver
 *
 * # Scope
 *
 * Implements serial server and client device driver for the POSIX
 * environment.
 *
 * # Usage
 *
 * serial provides a packet oriented transfer. The receiver should
 * first implement a dispatch layer (\ref GR_ISN_Dispatch) to
 * decode the PING and other (as typically Message) layers.
 *
 * Instance is created with the isn_serial_driver_create() given the
 * serial port.
 */

/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * (c) Copyright 2019, Isotel, http://isotel.eu
 */

#ifndef ISN_SERIAL_DRIVER_H
#define ISN_SERIAL_DRIVER_H

#include "isn_logger.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct isn_serial_driver_s isn_serial_driver_t;
typedef int64_t time_ms_t;

typedef enum {
    ISN_FLOW_CONTROL_NONE,
    ISN_FLOW_CONTROL_SOFTWARE,
    ISN_FLOW_CONTROL_HARDWARE,
} isn_serial_driver_flow_control_t;

typedef enum {
    ISN_PARITY_NONE,
    ISN_PARITY_ODD,
    ISN_PARITY_EVEN,
} isn_serial_driver_parity_t;

typedef struct {
    int baud_rate;
    int data_bits;
    isn_serial_driver_flow_control_t flow_control;
    isn_serial_driver_parity_t parity;
    int stop_bits;
    int write_timeout_ms;
} isn_serial_driver_params_t;

/**
 * Default serial port parameters
 * baud_rate = 115200
 * data_bits = 8
 * flow_control = ISN_FLOW_CONTROL_NONE
 * parity = ISN_PARITY_NONE
 * stop_bits = 1
 * write_timeout_ms = 1000
 */
extern isn_serial_driver_params_t isn_serial_driver_default_params;

/**
 * Create a new serial driver instance
 *
 * \param port name
 * \params serial port parameters, NULL for default parameters
 * \param child binding layer
 * \returns a valid isn_serial_driver_t instance or NULL or error with errno set.
 */
isn_serial_driver_t* isn_serial_driver_create(const char* port, const isn_serial_driver_params_t* params,
                                              isn_layer_t* child);

/**
 * Free serial driver instance
 *
 * Make sure other layers have released all handles to it prior calling this function.
 */
void isn_serial_driver_free(isn_serial_driver_t* driver);

/**
 * Poll for incoming data and process it.
 */
int isn_serial_driver_poll(isn_serial_driver_t* driver, time_ms_t timeout);

/**
 * Set logger (debugging) level
 */
void isn_serial_driver_setlogging(isn_logger_level_t level);

#ifdef __cplusplus
}
#endif

#endif //ISN_SERIAL_DRIVER_H
