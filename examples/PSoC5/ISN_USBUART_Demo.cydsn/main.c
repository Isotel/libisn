#include "project.h"
#include "PSoC/isn_usbuart.h"
#include "isn_frame.h"
#include "isn_msg.h"

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


/*----------------------------------------------------------*/
/* Transparent I/O User Layer                               */
/*----------------------------------------------------------*/

int isn_user_getsendbuf(isn_layer_t *drv, uint8_t **buf, size_t size) {
    int osize = isn_frame.drv.getsendbuf(&isn_frame, buf, size+1);
    if (buf) {
        if (*buf) (*buf)++; // add protocol header at the front
    }
    return osize-1;
}

void isn_user_free(isn_layer_t *drv, const uint8_t *buf) {
    if (buf) isn_frame.drv.free(&isn_frame, buf-1);
}

int isn_user_send(isn_layer_t *drv, uint8_t *buf, size_t size) {
    *(--buf) = ISN_PROTO_USER1;
    isn_frame.drv.send(&isn_frame, buf, size+1);
    return 0;
}

const uint8_t * isn_user_recv(isn_layer_t *drv, const uint8_t *buf, size_t size, isn_driver_t *caller) {
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

isn_driver_t isn_user = {
    isn_user_getsendbuf,
    isn_user_send,
    isn_user_recv,
    isn_user_free
};

void user1_streamout() {
    if (trigger > 1000) {
        trigger = 0;        
        uint8_t *obuf;
        if (isn_user.getsendbuf(NULL, &obuf, 5) == 5) {
            memcpy(obuf, "User\n", 5);
            isn_user.send(NULL, obuf, 5);
        }
        else {
            isn_user.free(NULL, obuf);
        }
    }
}


/*----------------------------------------------------------*/
/* Terminal I/O, Simple Echo                                */
/*----------------------------------------------------------*/

const uint8_t * isn_terminal_recv(isn_layer_t *drv, const uint8_t *buf, size_t size, isn_driver_t *caller) {
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

isn_driver_t isn_terminal = {
    NULL,
    NULL,
    isn_terminal_recv,
    NULL
};


/*----------------------------------------------------------*/
/* PSoC Start                                               */
/*----------------------------------------------------------*/

int main(void)
{
    static isn_msg_table_t isn_msg_table[] = {
        { 0, sizeof(uint64_t), serial_cb, "%T0{Example} V1.0 {#sno}={%<Lx}" },
        { 0, sizeof(led_t),    led_cb,    "LED {:blue}={%hu:Off,On}" },
        ISN_MSG_DESC_END(0)
    };
    isn_msg_init(&isn_message, isn_msg_table, SIZEOF(isn_msg_table), &isn_frame);

    static isn_bindings_t isn_bindings[] = {
        {ISN_PROTO_USER1, &isn_user},
        {ISN_PROTO_MSG, &isn_message},
        {ISN_PROTO_OTHERWISE, &isn_terminal}
    };
    isn_frame_init(&isn_frame, ISN_FRAME_MODE_COMPACT, isn_bindings, &isn_usbuart, &counter_1kHz, 100 /*ms*/);
    CySysTickStart();
    CySysTickSetCallback(0, systick_1kHz);
    PWM_LEDB_Start();
    PWM_HS_Start();
    CyGlobalIntEnable;

    isn_usbuart_init(&isn_usbuart, USBUART_3V_OPERATION, &isn_frame);

    while(1) {
        isn_usbuart_poll(&isn_usbuart);
        user1_streamout();
        if ( !isn_msg_sched(&isn_message) ) {
            asm volatile("wfi");
        }
    }
}
