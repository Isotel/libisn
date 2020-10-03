/** \file
 *  \author Uros Platise <uros@isotel.org>
 *  \brief Simple Demo showing the use of Message Layer framed with Compact Frame over standard USB UART phy
 */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * (c) Copyright 2019, Isotel, http://isotel.org
 */

#include "project.h"
#include "PSoC/isn_usbuart.h"
#include "isn_frame.h"
#include "isn_msg.h"

isn_message_t isn_message;
isn_frame_t isn_frame;
isn_usbuart_t isn_usbuart;
volatile uint32_t counter_1kHz = 0;

/*----------------------------------------------------------*/
/* Message Layer                                            */
/*----------------------------------------------------------*/

typedef struct {
    uint8_t blue;
} __attribute__((packed)) led_t;

static uint64_t serial = 0x12345678;
static led_t    led    = {1};

void *serial_cb(const void *data) {
    return &serial;
}

void *led_cb(const void *data) {
    if (data) {
        led = *((const led_t *)data);
    }
    return &led;
}

static isn_msg_table_t isn_msg_table[] = {
    { 0, sizeof(uint64_t), serial_cb, "%T0{Frame-Message Example} V1.0 {#sno}={%<lx}" },
    { 0, sizeof(led_t),    led_cb,    "LED {:blue}={%hu:Off,On}" },
    ISN_MSG_DESC_END(0)
};

/*----------------------------------------------------------*/
/* Terminal I/O, Simple Echo and Ping Handler               */
/*----------------------------------------------------------*/

const void * terminal_recv(isn_layer_t *drv, const void *src, size_t size, isn_driver_t *caller) {
    void *obuf = NULL;
    const uint8_t *buf = src;

    if (size==1 && *buf==ISN_PROTO_PING) {
        isn_msg_send(&isn_message, 1,1);

#if BUG_HUNTED_TEXTwFRAME_SHOULDWORK
        if ( caller->getsendbuf(caller, &obuf, 4)==4 ) {
            memcpy(obuf, "ping", 4);
            caller->send(caller, obuf, 4);
        }
        else {
            caller->free(caller, obuf);
        }
#endif
    }
    else {
        if ( caller->getsendbuf(caller, &obuf, size)==size ) {
            memcpy(obuf, buf, size);
            caller->send(caller, obuf, size);
        }
        else {
            caller->free(caller, obuf);
        }
    }
    return buf;
}

/*----------------------------------------------------------*/
/* PSoC Start                                               */
/*----------------------------------------------------------*/

static void systick_1kHz(void) {
    counter_1kHz++;
}

int main(void)
{
    PWM_LEDB_Start();
    CySysTickStart();
    CySysTickSetCallback(0, systick_1kHz);  // Note that this is high-pri NMI, you may want to use other lower priority sources

    isn_msg_init(&isn_message, isn_msg_table, ARRAY_SIZE(isn_msg_table), &isn_frame);
    isn_frame_init(&isn_frame, ISN_FRAME_MODE_COMPACT, &isn_message, NULL, &isn_usbuart, &counter_1kHz, 100 /*ms*/);
    CyGlobalIntEnable;
    isn_usbuart_init(&isn_usbuart, USBUART_3V_OPERATION, &isn_frame);

    while(1) {
        if (isn_usbuart_poll(&isn_usbuart) > 0) {
            // we received something from host, host is attached
            // one could also wait for ping as in this demo, see terminal_recv()
        }
        if ( !isn_msg_sched(&isn_message) ) {
            asm volatile("wfi");
        }
    }
}
