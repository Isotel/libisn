/** \file
 *  \author Uros Platise <uros@isotel.eu>, Stanislav <stanislav@isotel.eu>
 *  \see isn_usbfs.h
 * 
 * \addtogroup GR_ISN_PSoC_USBFS
 * 
 * # Tested
 * 
 *  - PSoC5 CY8C5888, DeweLab project
 * 
 * \cond Implementation
 * 
 * # Implementation
 * 
 * Represents the most simple (tiny) implementation of bulk USB 
 * transfer via one receiving EP 1, and 7 sending EPs with single
 * buffering in C code. It should reach sending speeds up to 400 kB/s.
 * 
 * Speed may be improved by adding additional TX and RX FIFOs, 
 * and by zero-padding technique to fill the entire packet.
 */
/*
 * (c) Copyright 2019, Isotel, http://isotel.eu
 */

#include "project.h"
#include "PSoC/isn_usbfs.h"

/**\{ */

/** OUT EP in USB therminology (EP that receives data from the host) */
#define USB_RECV_EP     1

/** IN EP in USB therminology (EP that sends data to the host) */
#define USB_SEND_EPst   2
#define USB_SEND_EPend  8

/**
 * Allocate buffer if buf is given, or just query for availability if buf is NULL
 * 
 * \returns desired or limited (max) size in the case desired size is too big
 */
static int isn_usbfs_getsendbuf(isn_layer_t *drv, void **dest, size_t size) {
    isn_usbfs_t *obj = (isn_usbfs_t *)drv;

    if (USBFS_GetEPState(obj->next_send_ep) == USBFS_IN_BUFFER_EMPTY && !obj->buf_locked) {
        if (dest) {
            obj->buf_locked = obj->next_send_ep;
            *dest = obj->txbuf;
        }
        return (size > USB_BUF_SIZE) ? USB_BUF_SIZE : size;
    }
    if (dest) {
        *dest = NULL;
    }
    return -1;
}

static void isn_usbfs_free(isn_layer_t *drv, const void *ptr) {
    isn_usbfs_t *obj = (isn_usbfs_t *)drv;
    if (ptr == obj->txbuf) {
        obj->buf_locked = 0;             // we only support one buffer so we may free
    }
}

static int isn_usbfs_send(isn_layer_t *drv, void *dest, size_t size) {
    assert(size <= TXBUF_SIZE);
    isn_usbfs_t *obj = (isn_usbfs_t *)drv;
    USBFS_LoadInEP(obj->buf_locked, dest, size);
    if (++obj->next_send_ep > USB_SEND_EPend) obj->next_send_ep = USB_SEND_EPst;
    isn_usbfs_free(drv, dest);
    return size;
}

size_t isn_usbfs_poll(isn_usbfs_t *obj) {
    size_t size = 0;

    if (USBFS_GetEPState(USB_RECV_EP) == USBFS_OUT_BUFFER_FULL) {        
        USBFS_ReadOutEP(USB_RECV_EP, (uint8_t*)obj->rxbuf, size = USBFS_GetEPCount(USB_RECV_EP));
        USBFS_EnableOutEP(USB_RECV_EP);
        if (size) {
            assert2(obj->child_driver->recv(obj->child_driver, obj->rxbuf, size, &obj->drv) == obj->rxbuf);
        }
    }
    return size;
}

void isn_usbfs_init(isn_usbfs_t *obj, int mode, isn_layer_t* child) {
    obj->drv.getsendbuf = isn_usbfs_getsendbuf;
    obj->drv.send = isn_usbfs_send;
    obj->drv.recv = NULL;
    obj->drv.free = isn_usbfs_free;
    obj->child_driver = child;
    obj->buf_locked = 0;
    obj->next_send_ep = USB_SEND_EPst; /** first free EP */

    USBFS_Start(0u, mode);
    while (0u == USBFS_GetConfiguration()) {}
    USBFS_EnableOutEP(USB_RECV_EP);
}

/**
 * WINUSB driverless installation
 * 
 * \note This function is called directly from the Cypress PSoC USBFS Component Driver.
 * 
 * USBFS configuration -> Advanced -> [X] Handle vendor request in user code
 * USBFS configuration -> String descriptor -> [X] Include MS OS String Descriptor
 *
 * if device is not detected properly in Windows:
 *   1. in "device manager" - uninstall device
 *   2. in regedit -
 *      a) open "HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\usbflags"
 *      b) delete key [DEVICE_VID][DEVICE_PID][0000] key
 *         for example for the device with VID 1CED and PID 4001 key 1CED40010000 must be deleted
 *         NB! please be careful with registry editor
 *
 */
uint8_t USBFS_HandleVendorRqst() {
    static const uint8_t CYCODE USBFS_MSOS_CONFIGURATION_DESCR_WINUSB[USBFS_MSOS_CONF_DESCR_LENGTH] = {
            /*  Length of the descriptor 4 bytes       */   0x28u, 0x00u, 0x00u, 0x00u,
            /*  Version of the descriptor 2 bytes      */   0x00u, 0x01u,
            /*  wIndex - Fixed:INDEX_CONFIG_DESCRIPTOR */   0x04u, 0x00u,
            /*  bCount - Count of device functions.    */   0x01u,
            /*  Reserved : 7 bytes                     */   0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
            /*  bFirstInterfaceNumber                  */   0x00u,
            /*  Reserved                               */   0x01u,
            /*  compatibleID    - "WINUSB\0\0"         */   (uint8) 'W', (uint8) 'I', (uint8) 'N', (uint8) 'U',
                                                            (uint8) 'S', (uint8) 'B', 0x00u, 0x00u,
            /*  subcompatibleID - "00000\0\0"          */   (uint8) '0', (uint8) '0', (uint8) '0', (uint8) '0',
                                                            (uint8) '0', 0x00u, 0x00u, 0x00u,
            /*  Reserved : 6 bytes                     */   0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u
    };

    uint8 requestHandled = USBFS_FALSE;

    /* Check request direction: D2H or H2D. */
    if (USBFS_bmRequestTypeReg & USBFS_RQST_DIR_D2H) {
        /* Handle direction from device to host. */
        switch (USBFS_bRequestReg) {
            case USBFS_GET_EXTENDED_CONFIG_DESCRIPTOR:
#if defined(USBFS_ENABLE_MSOS_STRING)
                USBFS_currentTD.pData = (volatile uint8*) &USBFS_MSOS_CONFIGURATION_DESCR_WINUSB[0u];
                USBFS_currentTD.count = USBFS_MSOS_CONFIGURATION_DESCR_WINUSB[0u];
                requestHandled = USBFS_InitControlRead();
#endif /* (USBFS_ENABLE_MSOS_STRING) */
                break;

            default:
                break;
        }
    }
    return (requestHandled);
}

/** \} \endcond */