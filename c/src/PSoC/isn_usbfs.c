/** \file
 *  \brief ISN USBFS Bulk USB Driver for PSoC4 and PSoC5 Implementation
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

/**
 * NB! PSOC Creator ignore custom setting of the USBFS ARB PRIORITY interrupt
 * and set it to 0 (highest). See generated USBFS.h USBFS_ARB_PRIOR.
 *
 * We should set it to less than our high priority interrupt, for example ADC ISR
 * channel switching in multichannel configuration.
 *
 * NB!!! USBFS ARB PRIORITY must be higher (numerically less)
 * than ANY other USBFS EPx interrupt priorities (7 by default).
 */
#define CUSTOM_USBFS_ARB_PRIORITY   5

#if defined(USBFS_ep_0__INTC_PRIOR_NUM) && USBFS_ep_0__INTC_PRIOR_NUM <= CUSTOM_USBFS_ARB_PRIORITY
#error EP0 interrupt priority must be lower than ARB interrupt priority
#endif
#if defined(USBFS_ep_1__INTC_PRIOR_NUM) && USBFS_ep_1__INTC_PRIOR_NUM <= CUSTOM_USBFS_ARB_PRIORITY
#error EP1 interrupt priority must be lower than ARB interrupt priority
#endif
#if defined(USBFS_ep_2__INTC_PRIOR_NUM) && USBFS_ep_2__INTC_PRIOR_NUM <= CUSTOM_USBFS_ARB_PRIORITY
#error EP2 interrupt priority must be lower than ARB interrupt priority
#endif
#if defined(USBFS_ep_3__INTC_PRIOR_NUM) && USBFS_ep_3__INTC_PRIOR_NUM <= CUSTOM_USBFS_ARB_PRIORITY
#error EP3 interrupt priority must be lower than ARB interrupt priority
#endif
#if defined(USBFS_ep_4__INTC_PRIOR_NUM) && USBFS_ep_4__INTC_PRIOR_NUM <= CUSTOM_USBFS_ARB_PRIORITY
#error EP4 interrupt priority must be lower than ARB interrupt priority
#endif
#if defined(USBFS_ep_5__INTC_PRIOR_NUM) && USBFS_ep_2__INTC_PRIOR_NUM <= CUSTOM_USBFS_ARB_PRIORITY
#error EP2 interrupt priority must be lower than ARB interrupt priority
#endif
#if defined(USBFS_ep_6__INTC_PRIOR_NUM) && USBFS_ep_6__INTC_PRIOR_NUM <= CUSTOM_USBFS_ARB_PRIORITY
#error EP6 interrupt priority must be lower than ARB interrupt priority
#endif
#if defined(USBFS_ep_7__INTC_PRIOR_NUM) && USBFS_ep_7__INTC_PRIOR_NUM <= CUSTOM_USBFS_ARB_PRIORITY
#error EP7 interrupt priority must be lower than ARB interrupt priority
#endif
#if defined(USBFS_ep_8__INTC_PRIOR_NUM) && USBFS_ep_8__INTC_PRIOR_NUM <= CUSTOM_USBFS_ARB_PRIORITY
#error EP8 interrupt priority must be lower than ARB interrupt priority
#endif

/** OUT EP in USB therminology (EP that receives data from the host) */
#define USB_RECV_EP     1

/** IN EP in USB therminology (EP that sends data to the host) */
#define USB_SEND_EPst   2
static uint8_t USB_SEND_EPend = USB_SEND_EPst + 6;

/** INEP could be assigned to specific user layer to guarantee buf availability and
 *  same EP number through out continuous tranfers */
static isn_layer_t * inep_reservation[7] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL};

/**
 * Allocate buffer if buf is given, or just query for availability if buf is NULL
 *
 * \returns desired or limited (max) size in the case desired size is too big
 */
static int isn_usbfs_getsendbuf(isn_layer_t *drv, void **dest, size_t size, const isn_layer_t *caller) {
    isn_usbfs_t *obj = (isn_usbfs_t *)drv;

    if (!obj->buf_locked) {
        // Find appropriate resource
        int current_ep = obj->next_send_ep;
        while ( (inep_reservation[obj->next_send_ep - USB_SEND_EPst] != caller && 
                 inep_reservation[obj->next_send_ep - USB_SEND_EPst] != NULL) ||
               USBFS_GetEPState(obj->next_send_ep) != USBFS_IN_BUFFER_EMPTY) {

            if (++obj->next_send_ep > USB_SEND_EPend) obj->next_send_ep = USB_SEND_EPst;
            if (current_ep == obj->next_send_ep) goto none_avail;
        }
        if (dest) {
            obj->buf_locked = obj->next_send_ep;
            *dest = obj->txbuf;
        }
        return (size > USB_BUF_SIZE) ? USB_BUF_SIZE : size;
    }
none_avail:
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
    isn_usbfs_t *obj = (isn_usbfs_t *)drv;
    assert(size <= TXBUF_SIZE);
    if (size) {
        USBFS_LoadInEP(obj->buf_locked, dest, size);
        //if (++obj->next_send_ep > USB_SEND_EPend) obj->next_send_ep = USB_SEND_EPst;
    }
    isn_usbfs_free(drv, dest);
    return size;
}

/**
 * It is a packet oriented transfer, in which first part and if buffer is empty
 * it loads data from the EP buffer. In the 2nd part it tries to forward it, and
 * in the case it is unsuccessful it retries the next time.
 */
size_t isn_usbfs_poll(isn_usbfs_t *obj) {
    if (obj->rx_size == 0) {
        if (USBFS_GetEPState(USB_RECV_EP) == USBFS_OUT_BUFFER_FULL) {
            USBFS_ReadOutEP(USB_RECV_EP, (uint8_t*)obj->rxbuf, obj->rx_size = USBFS_GetEPCount(USB_RECV_EP));
            USBFS_EnableOutEP(USB_RECV_EP);
            obj->rx_counter += obj->rx_size;
        }
    }
    if (obj->rx_size) {
        if (obj->child_driver->recv(obj->child_driver, obj->rxbuf, obj->rx_size, &obj->drv) >= obj->rx_size) {
            obj->rx_size = 0;
        }
        else obj->rx_retry++;    // Packet could not be fully accepted, retry next time
    }
    return obj->rx_size;
}

void isn_usbfs_init(isn_usbfs_t *obj, int mode, isn_layer_t* child) {
    obj->drv.getsendbuf = isn_usbfs_getsendbuf;
    obj->drv.send = isn_usbfs_send;
    obj->drv.recv = NULL;
    obj->drv.free = isn_usbfs_free;
    obj->child_driver = child;
    obj->buf_locked = 0;
    obj->rx_size    = 0;
    obj->rx_counter = 0;
    obj->next_send_ep = USB_SEND_EPst; /** first free EP */

    USBFS_Start(0u, mode);
    CyIntSetPriority(USBFS_ARB_VECT_NUM, CUSTOM_USBFS_ARB_PRIORITY);
    while (0u == USBFS_GetConfiguration()) {}
    USBFS_EnableOutEP(USB_RECV_EP);
}

void isn_usbfs_set_maxinbufs(uint8_t count) {
    if (count < 1) count = 1; else if (count > 7) count = 7;
    USB_SEND_EPend = USB_SEND_EPst + count - 1;
}

void isn_usbfs_assign_inbuf(uint8_t no, isn_layer_t *reserve_for_layer) {
    if (no == 0) {
        for (; no<7; no++) inep_reservation[no] = reserve_for_layer;
        return;
    }
    if (no < 1) no = 1; else if (no > 7) no = 7;
    inep_reservation[no-1] = reserve_for_layer;
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