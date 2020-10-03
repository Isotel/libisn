/** \file
 *  \brief ISN Protocol I/O
 *  \author Uros Platise <uros@isotel.org>
 *  \see isn_io.c
 */
/**
 * \ingroup GR_ISN
 * \defgroup GR_ISN_IO I/O Methods
 *
 * # Scope
 *
 * Provides helper functions to operate on isn streams.
 */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * (c) Copyright 2020, Isotel, http://isotel.org
 */

#ifndef __ISN_IO_H__
#define __ISN_IO_H__

#include "isn_def.h"

#ifdef __cplusplus
extern "C"
{
#endif

/*----------------------------------------------------------------------*/
/* Public functions                                                     */
/*----------------------------------------------------------------------*/

/** Write to any layer
 *
 * \param layer with capability of transmission
 * \param src daa
 * \param size
 * \returns result from the layer send() method, or -1 on insufficient buffer availabilty
 */
int isn_write(isn_layer_t *layer, const void *src, size_t size);

#ifdef __cplusplus
}
#endif

#endif
