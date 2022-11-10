/** \file
 *  \brief ISN USBUART Driver for PSoC4 and PSoC5 Implementation
 *  \author Uros Platise <uros@isotel.org>
 *  \see isn_usbuart.h
 */
/**
 * \ingroup GR_ISN_PSoC
 * \addtogroup GR_ISN_PSoC_USBUART
 *
 * # Tested
 *
 *  - Families: PSoC4, PSoC5
 *
 * \cond Implementation
 */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * (c) Copyright 2019, Isotel, http://isotel.org
 */

#include <string.h>
#include "project.h"
#include "config.h"
#include "isn_clock.h"
#include "PSoC/isn_usbuart.h"

#if (CY_USB_DEV_VERSION_MAJOR == 1)
#  define PSoC6_PDL_API
#  define USBUART_COM_PORT    (0U)
#endif

/**\{ */

#define USBUART_TIMEOUT    ISN_CLOCK_ms(100)

/**
 * Allocate buffer if buf is given, or just query for availability if buf is NULL
 *
 * \returns desired or limited (max) size in the case desired size is too big
 */
static int isn_usbuart_getsendbuf(isn_layer_t *drv, void **dest, size_t size, const isn_layer_t *caller) {
    isn_usbuart_t *obj = (isn_usbuart_t *)drv;
    if (!obj->buf_locked &&
#ifdef PSoC6_PDL_API
        Cy_USB_Dev_CDC_IsReady(USBUART_COM_PORT, &USBUART_cdcContext)
#else
        USBUART_CDCIsReady()
#endif
            ) {
        if (dest) {
            obj->buf_locked = 1;
            *dest = obj->txbuf;
        }
        return (size > USBUART_TXBUF_SIZE) ? USBUART_TXBUF_SIZE : size;
    }
    if (dest) {
        *dest = NULL;
    }
    return -1;
}

static void isn_usbuart_free(isn_layer_t *drv, const void *ptr) {
    isn_usbuart_t *obj = (isn_usbuart_t *)drv;
    if (ptr == obj->txbuf) {
        obj->buf_locked = 0;             // we only support one buffer so we may free
    }
}

static int isn_usbuart_send(isn_layer_t *drv, void *dest, size_t size) {
    isn_usbuart_t *obj = (isn_usbuart_t *)drv;
    assert(size <= USBUART_TXBUF_SIZE);
    if (size) {
#ifdef PSoC6_PDL_API
        ASSERT_UNTIL( Cy_USB_Dev_CDC_IsReady(USBUART_COM_PORT, &USBUART_cdcContext), USBUART_TIMEOUT );
        Cy_USB_Dev_CDC_PutData(USBUART_COM_PORT, dest, size, &USBUART_cdcContext);
#else
        ASSERT_UNTIL( USBUART_CDCIsReady(), USBUART_TIMEOUT );
        USBUART_PutData(dest, size);
#endif
        if (size == 64) {   /// terminate the last packet with zero packet, and \todo if other packets are not already in the queue
#ifdef PSoC6_PDL_API
            ASSERT_UNTIL( Cy_USB_Dev_CDC_IsReady(USBUART_COM_PORT, &USBUART_cdcContext), USBUART_TIMEOUT );
            Cy_USB_Dev_CDC_PutData(USBUART_COM_PORT, NULL, 0, &USBUART_cdcContext);
#else
            ASSERT_UNTIL( USBUART_CDCIsReady(), USBUART_TIMEOUT );
            USBUART_PutData(NULL, 0);
#endif
        }
        obj->drv.stats.tx_counter += size;
        obj->drv.stats.tx_packets++;
    }
    isn_usbuart_free(drv, dest);    // free buffer, however need to block use of buffer until sent out
    return size;
}

size_t isn_usbuart_poll(isn_usbuart_t *obj) {
    size_t size = 0;
#ifdef PSoC6_PDL_API
    if (Cy_USB_Dev_CDC_IsDataReady(USBUART_COM_PORT, &USBUART_cdcContext)) {
        size = Cy_USB_Dev_CDC_GetCount(USBUART_COM_PORT, &USBUART_cdcContext);
#else
    if (USBUART_DataIsReady()) {
        size = USBUART_GetCount();
#endif
        if ( (size + obj->rx_size) > USBUART_RXBUF_SIZE ) size = USBUART_RXBUF_SIZE - obj->rx_size;
        if ( size ) {
#ifdef PSoC6_PDL_API
            Cy_USB_Dev_CDC_GetData(USBUART_COM_PORT, &obj->rxbuf[obj->rx_size], size, &USBUART_cdcContext);
#else
            USBUART_GetData(&obj->rxbuf[obj->rx_size], size);
#endif
            obj->rx_size += size;
            obj->drv.stats.rx_counter += size;
        }
        else obj->drv.stats.rx_dropped++; // It hasn't been really dropped yet
    }
    if (obj->rx_size) {
        size = obj->child_driver->recv(obj->child_driver, obj->rxbuf, obj->rx_size, obj);
        if (size) obj->drv.stats.rx_packets++;
        if (size < obj->rx_size) {
            obj->drv.stats.rx_retries++;    // Packet could not be fully accepted, retry next time
            memmove(obj->rxbuf, &obj->rxbuf[size], obj->rx_size - size);
            obj->rx_size -= size;
        }
        else obj->rx_size = 0;  // handles case if recv() returns size higher than rx_size
    }
    return size;
}

#ifdef PSoC6_PDL_API

static void USBUART_IsrHigh(void) {
    Cy_USBFS_Dev_Drv_Interrupt(USBUART_HW,
                               Cy_USBFS_Dev_Drv_GetInterruptCauseHi(USBUART_HW),
                               &USBUART_drvContext);
}

static void USBUART_IsrMedium(void) {
    Cy_USBFS_Dev_Drv_Interrupt(USBUART_HW,
                               Cy_USBFS_Dev_Drv_GetInterruptCauseMed(USBUART_HW),
                               &USBUART_drvContext);
}

static void USBUART_IsrLow(void) {
    Cy_USBFS_Dev_Drv_Interrupt(USBUART_HW,
                               Cy_USBFS_Dev_Drv_GetInterruptCauseLo(USBUART_HW),
                               &USBUART_drvContext);
}

# if (CY_CPU_CORTEX_M0P)
const cy_stc_sysint_t UsbDevIntrHigh =
{
    .intrSrc = USBUART_HighPriorityInterrupt__INTC_CORTEXM0P_MUX,
    .cm0pSrc = (IRQn_Type) usb_interrupt_hi_IRQn,
    .intrPriority = 1U,
};
const cy_stc_sysint_t UsbDevIntrMedium =
{
    .intrSrc = USBUART_MediumPriorityInterrupt__INTC_CORTEXM0P_MUX,
    .cm0pSrc = (IRQn_Type) usb_interrupt_med_IRQn,
    .intrPriority = 2U,
};
const cy_stc_sysint_t UsbDevIntrLow =
{
    .intrSrc = USBUART_LowPriorityInterrupt__INTC_CORTEXM0P_MUX,
    .cm0pSrc = (IRQn_Type) usb_interrupt_lo_IRQn,
    .intrPriority = 3U,
};
# else
const cy_stc_sysint_t UsbDevIntrHigh =
{
    .intrSrc = (IRQn_Type) usb_interrupt_hi_IRQn,
    .intrPriority = 1U,
};
const cy_stc_sysint_t UsbDevIntrMedium =
{
    .intrSrc = (IRQn_Type) usb_interrupt_med_IRQn,
    .intrPriority = 2U,
};
const cy_stc_sysint_t UsbDevIntrLow =
{
    .intrSrc = (IRQn_Type) usb_interrupt_lo_IRQn,
    .intrPriority = 3U,
};

# endif
#endif

void isn_usbuart_init(isn_usbuart_t *obj, int mode, isn_layer_t* child) {
    ASSERT(obj);
    ASSERT(child);
    memset(&obj->drv, 0, sizeof(obj->drv));
    obj->drv.getsendbuf = isn_usbuart_getsendbuf;
    obj->drv.send = isn_usbuart_send;
    obj->drv.recv = NULL;
    obj->drv.free = isn_usbuart_free;
    obj->child_driver = child;
    obj->buf_locked = 0;
    obj->rx_size = 0;

#ifdef PSoC6_PDL_API
    if (CY_USB_DEV_SUCCESS != Cy_USB_Dev_Init(USBUART_HW, &USBUART_drvConfig, &USBUART_drvContext,
                       &USBUART_devices[0], &USBUART_devConfig, &USBUART_devContext)) {
        return;
    }
    if (CY_USB_DEV_SUCCESS != Cy_USB_Dev_CDC_Init(&USBUART_cdcConfig, &USBUART_cdcContext, &USBUART_devContext)) {
        return;
    }

    Cy_SysInt_Init(&UsbDevIntrHigh,   &USBUART_IsrHigh);
    Cy_SysInt_Init(&UsbDevIntrMedium, &USBUART_IsrMedium);
    Cy_SysInt_Init(&UsbDevIntrLow,    &USBUART_IsrLow);

    NVIC_EnableIRQ(UsbDevIntrHigh.intrSrc);
    NVIC_EnableIRQ(UsbDevIntrMedium.intrSrc);
    NVIC_EnableIRQ(UsbDevIntrLow.intrSrc);
#else
    USBUART_Start(0, mode);
    while(0 == USBUART_GetConfiguration());
    USBUART_CDC_Init();
#endif
}

int isn_usbuart_start(int wait_ms) {
#ifdef PSoC6_PDL_API
    return Cy_USB_Dev_Connect(wait_ms > 0 ? true : false, wait_ms, &USBUART_devContext) == CY_USB_DEV_SUCCESS ? 1 : 0;
#else
#  warning isn_usbuart_start() not implemented, currently isn_usbuart_init() also performs the connection
    return 0;
#endif
}

int isn_usbuart_stop() {
#ifdef PSoC6_PDL_API
    Cy_USB_Dev_Disconnect(&USBUART_devContext);
    return 0;
#else
#  warning isn_usbuart_start() not implemented, currently isn_usbuart_init() also performs the connection
    return 0;
#endif
}

/** \} \endcond */
