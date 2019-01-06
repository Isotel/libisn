/** \file
 *  \author Uros Platise <uros@isotel.eu>
 *  \see ...
 */

#include "project.h"
#include "PSoC/isn_usbuart.h"
#include "isn_frame.h"
#include "isn_msg.h"
#include "isn_user.h"

isn_user_t isn_user;
isn_message_t isn_message;
isn_frame_t isn_frame;
isn_usbuart_t isn_usbuart;

volatile uint32_t counter_1kHz = 0;
volatile uint32_t trigger = 0;

static void systick_1kHz(void) {
    counter_1kHz++; 
    trigger++;
}

/*----------------------------------------------------------*/
/* Message Layer                                            */
/*----------------------------------------------------------*/

typedef struct {
    uint8_t blue;
} __attribute__((packed)) led_t;

static uint64_t serial = 0x1234567890ABCDEF;
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
    { 0, sizeof(uint64_t), serial_cb, "%T0{Example} V1.0 {#sno}={%<Lx}" },
    { 0, sizeof(led_t),    led_cb,    "LED {:blue}={%hu:Off,On}" },
    ISN_MSG_DESC_END(0)
};

/*----------------------------------------------------------*/
/* Transparent I/O User Layer                               */
/*----------------------------------------------------------*/

const uint8_t * userstream_recv(isn_layer_t *drv, const uint8_t *buf, size_t size, isn_driver_t *caller) {    
    uint8_t *obuf = NULL;
    if (caller->getsendbuf(caller, &obuf, size) == size) {
        memcpy(obuf, buf, size);
        caller->send(caller, obuf, size);
    }
    else {
        caller->free(caller, obuf);
    }
    return buf;
}

void userstream_generate() {
    if (trigger > 1000) {
        trigger = 0;        
        uint8_t *obuf;
        if (isn_user.drv.getsendbuf(&isn_user, &obuf, 5) == 5) {
            memcpy(obuf, "User\n", 5);
            isn_user.drv.send(&isn_user, obuf, 5);
        }
        else {
            isn_user.drv.free(&isn_user, obuf);
        }
    }
}

/*----------------------------------------------------------*/
/* Terminal I/O, Simple Echo and Ping Handler               */
/*----------------------------------------------------------*/

const uint8_t * terminal_recv(isn_layer_t *drv, const uint8_t *buf, size_t size, isn_driver_t *caller) {
    uint8_t *obuf = NULL;

    if (size==1 && *buf==ISN_PROTO_PING) {
        if ( caller->getsendbuf(caller, &obuf, 4)==4 ) {
            isn_msg_send(&isn_message, 1,1);
            memcpy(obuf, "ping", 4);
            caller->send(caller, obuf, 4);
        }
        else {
            caller->free(caller, obuf);
        }
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

static isn_bindings_t isn_bindings[] = {
    {ISN_PROTO_USER1, &(isn_receiver_t){userstream_recv} },
    {ISN_PROTO_MSG, &isn_message},
    {ISN_PROTO_OTHERWISE, &(isn_receiver_t){terminal_recv} }
};

int main(void)
{
    isn_msg_init(&isn_message, isn_msg_table, SIZEOF(isn_msg_table), &isn_frame);
    isn_user_init(&isn_user, &userstream, &isn_frame, ISN_PROTO_USER1);

    isn_frame_init(&isn_frame, ISN_FRAME_MODE_COMPACT, isn_bindings, &isn_usbuart, &counter_1kHz, 100 /*ms*/);
    CySysTickStart();
    CySysTickSetCallback(0, systick_1kHz);
    PWM_LEDB_Start();
    PWM_HS_Start();
    CyGlobalIntEnable;
    isn_usbuart_init(&isn_usbuart, USBUART_3V_OPERATION, &isn_frame);

    while(1) {
        isn_usbuart_poll(&isn_usbuart);
        userstream_generate();
        if ( !isn_msg_sched(&isn_message) ) {
            asm volatile("wfi");
        }
    }
}
