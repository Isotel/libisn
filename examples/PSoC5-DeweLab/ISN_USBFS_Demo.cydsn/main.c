/** \file
 *  \author Uros Platise <uros@isotel.org>
 */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * (c) Copyright 2019, Isotel, http://isotel.org
 */

#include "project.h"
#include "PSoC/isn_usbfs.h"
#include "isn.h"

//#define TELNET      // Use this option to redirect example2 device over IDM telnet port

#define TIME_ms(t)  ( (t) * 100 )
#define TIME_us(t)  ( (t) / 10  )

isn_user_t isn_user;
isn_redirect_t isn_loopback;
isn_message_t isn_message, isn_message2;
isn_frame_t isn_frame;
isn_usbfs_t isn_usbfs;
isn_dispatch_t isn_dispatch;

int send_data = 0;

/*----------------------------------------------------------*/
/* Transparent User1 Layer sends arb data to telnet port    */
/*----------------------------------------------------------*/

const uint8_t test_data[64] = "1234567890abcdefghijklmnoprstuvzxABCDEFGHIJKLMNOPRSTUZX@#$%^&*()";

int userstream_generate() {
    return isn_write_atleast(&isn_user, test_data, 62, 50);
}

void *userstream_send(const void *arg) {
    isn_write(&isn_user, "Test\n", 5);
    return NULL;
}

/*----------------------------------------------------------*/
/* Message Layer                                            */
/*----------------------------------------------------------*/

typedef struct {
    uint8_t blue;
    uint32_t cnt;
    uint32_t remain;
} __attribute__((packed)) led_t;

static uint64_t serial = 0x1234567890ABCDEF;
static led_t    led    = {1, 10};

void *serial_cb(const void *data) {
    return &serial;
}

void *led_cb(const void *data) {
    if (data) {
        led = *((const led_t *)data);
    }
    else {
        led.blue ^= 1;
    }
    CTRL_LED_Write(~led.blue);
    return &led;
}

void *led_cb2(const void *data) {
    if (data) {
        led = *((const led_t *)data);
    }
    //led.cnt = *SysCounter_COUNTER_LSB_PTR;
    return &led;
}

static isn_msg_table_t isn_msg_table[] = {
    { 0, sizeof(uint64_t), serial_cb, "%T0{Example} V1.2 {#sno}={%<Lx}" },
    { 0, sizeof(led_t),    led_cb,    "LED {:blue}={%hu:Off,On} {:cnt}={%lu} {:rem}={%lu}" },
    ISN_MSG_DESC_END(0)
};

static isn_msg_table_t isn_msg_table2[] = {
    { 0, sizeof(uint64_t), serial_cb, "%T0{Example2} V1.2 {#sno}={%<Lx}" },
    { 0, sizeof(led_t),    led_cb2,    "LED {:blue}={%hu:Off,On} {:cnt}={%lu} {:rem}={%lu}" },
    ISN_MSG_DESC_END(0)
};

/*----------------------------------------------------------*/
/* Ping Handler from IDM                                    */
/*----------------------------------------------------------*/

size_t ping_recv(isn_layer_t *drv, const void *src, size_t size, isn_layer_t *caller) {
    assert(src);
    assert(size > 0);

    if ( *(uint8_t *)src == ISN_PROTO_PING) {
        isn_msg_send(&isn_message,  1, ISN_MSG_PRI_NORMAL);
        isn_msg_send(&isn_message2, 1, ISN_MSG_PRI_NORMAL);

#ifndef TELNET
        led.remain = send_data;
        send_data = led.cnt;
#endif
    }
    return size;
}

/*----------------------------------------------------------*/
/* PSoC Start                                               */
/*----------------------------------------------------------*/

static isn_bindings_t isn_bindings[] = {
    {ISN_PROTO_USER1, &isn_user},   // higher the faster; we give top speed to the transparent link
    {ISN_PROTO_MSG, &isn_message},
#ifndef TELNET
    {ISN_PROTO_FRAME, &isn_frame},
#endif
    {ISN_PROTO_PING, &(isn_receiver_t){ping_recv} },
    {ISN_PROTO_LISTEND, NULL}
};

int main(void)
{
    SysCounter_Start();

    isn_tasklet_entry_t tasklets[8];
    isn_reactor_init(tasklets, ARRAY_SIZE(tasklets), SysCounter_COUNTER_LSB_PTR);

    /* First IDM Device with One Stream Port and dispatch to the Frame Layer for 2nd Device */
    isn_msg_init(&isn_message, isn_msg_table, ARRAY_SIZE(isn_msg_table), &isn_usbfs);

    /* Second IDM Device over Frame Layer */
    isn_msg_init(&isn_message2, isn_msg_table2, ARRAY_SIZE(isn_msg_table2), &isn_frame);
#ifdef TELNET
    isn_frame_init(&isn_frame, ISN_FRAME_MODE_SHORT, &isn_message2, &(isn_receiver_t){ping_recv}, &isn_user, &counter_1kHz, 100 /*ms*/);
#else
    isn_frame_init(&isn_frame, ISN_FRAME_MODE_SHORT, &isn_message2, NULL, &isn_usbfs, SysCounter_COUNTER_LSB_PTR, 10000 /* *10us */);
#endif

    /* We set a loopback driver (echo) whatever we receive on user (telnet) port we return back */
#ifdef TELNET
    isn_user_init(&isn_user, &isn_frame, &isn_usbfs, ISN_PROTO_USER1);
#else
    isn_loopback_init(&isn_loopback);
    isn_user_init(&isn_user, &isn_loopback, &isn_usbfs, ISN_PROTO_USER1);
#endif

    /* Main dispatch from the USBFS */
    isn_dispatch_init(&isn_dispatch, isn_bindings);
    CyGlobalIntEnable;
    isn_usbfs_init(&isn_usbfs, USBFS_DWR_POWER_OPERATION, &isn_dispatch);
#if 0
    isn_usbfs_assign_inbuf(0, &isn_user);
    isn_usbfs_assign_inbuf(1, &isn_message);
    isn_usbfs_assign_inbuf(2, &isn_message2);
#endif

    while(1) {
        isn_usbfs_poll(&isn_usbfs);
        isn_reactor_run();
        //if (send_data > 0) {            
        //    send_data -= isn_write_atleast(&isn_user, test_data, send_data, 1);            
        //}
        send_data -= isn_write_atleast(&isn_user, test_data, send_data, 1);
        if ( !isn_msg_sched(&isn_message) && !isn_msg_sched(&isn_message2) ) {
            asm volatile("wfi");
        }
    }
}
