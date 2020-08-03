/** \file
 *  \brief ISN Protocol I/O
 *  \author Uros Platise <uros@isotel.eu>
 *  \see isn_io.h
 *
 * \cond Implementation
 * \addtogroup GR_ISN_IO
 */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * (c) Copyright 2019, Isotel, http://isotel.eu
 */

#include <string.h>
#include "isn_io.h"

/**\{ */

int isn_write(isn_driver_t *layer, const void *src, size_t size) {
    void *buf;
    if (layer->getsendbuf(layer, &buf, size, layer) == size) {
        memcpy(buf, src, size);
        return layer->send(layer, buf, size);
    }
    layer->free(layer, buf);
    return -1;
}

/** \} \endcond */
