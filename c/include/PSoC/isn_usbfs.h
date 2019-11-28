/** \file
 *  \brief ISN USBFS Bulk USB Driver for PSoC4 and PSoC5
 *  \author Uros Platise <uros@isotel.eu>
 *  \see isn_usbfs.h
 * 
 * \defgroup GR_ISN_PSoC_USBFS ISN Driver for PSoC USBFS
 * 
 * # Scope
 * 
 * Tiny implementation of the ISN Bulk Device Driver for the Cypress PSoC5 USBFS.
 * 
 * # Usage
 * 
 * - Place USBFS component in the PSoC Creator 4.2 and name it USBFS.
 * - Configure the first EP as OUT, and all other 7 as IN.
 * - Enable call to the external USBFS_HandleVendorRqst()
 * 
 * Isotel Precision IDM requires that Ping Protocol is also implemented.
 * See \ref GR_ISN_Dispatch for example.
 */
/*
 * (c) Copyright 2019, Isotel, http://isotel.eu
 */

#ifndef __ISN_USBFS_H__
#define __ISN_USBFS_H__

#include "isn_def.h"

#define USB_BUF_SIZE  64

/** ISN Layer Driver */
typedef struct {
    /* ISN Abstract Class Driver */
    isn_driver_t drv;

    /* Private data */
    isn_driver_t* child_driver;
    uint8_t txbuf[USB_BUF_SIZE];
    uint8_t rxbuf[USB_BUF_SIZE];
    int buf_locked;
    int next_send_ep;
}
isn_usbfs_t;

/*----------------------------------------------------------------------*/
/* Public functions                                                     */
/*----------------------------------------------------------------------*/

/** Polls for a new data received from PC and dispatch them 
 * \returns number of bytes received
 */
size_t isn_usbfs_poll(isn_usbfs_t *obj);

/** Initialize
 * 
 * \param mode USBFS_3V_OPERATION, USBFS_5V_OPERATION, USBFS_DWR_POWER_OPERATION
 * \param child use the next layer, like isn_frame
 */
void isn_usbfs_init(isn_usbfs_t *obj, int mode, isn_layer_t* child);

#endif
