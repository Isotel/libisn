#include "project.h"
#include "PSoC/isn_usbuart.h"
#include "isn_frame.h"
#include "isn_msg.h"

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

int isn_user_getsendbuf(uint8_t **buf, size_t size) {
    int osize = isn_frame.getsendbuf(buf, size+1);
    if (buf) {
        if (*buf) (*buf)++; // add protocol header at the front
    }
    return osize-1;
}

void isn_user_free(const uint8_t *buf) {
    if (buf) isn_frame.free( buf-1 );
}

int isn_user_send(uint8_t *buf, size_t size) {
    *(--buf) = ISN_PROTO_USER1;
    isn_frame.send(buf, size+1);
    return 0;
}

const uint8_t * isn_user_recv(const uint8_t *buf, size_t size, isn_layer_t *caller) {
    uint8_t *obuf = NULL;
    if (caller->getsendbuf(&obuf, size) == size) {
        memcpy(obuf, buf, size);
        caller->send(obuf, size);
    }
    else {
        caller->free(obuf);
    }
    return buf;
}

isn_layer_t isn_user = {
    isn_user_getsendbuf,
    isn_user_send,
    isn_user_recv,
    isn_user_free
};

void user1_streamout() {
    if (trigger > 1000) {
        trigger = 0;        
        uint8_t *obuf;
        if (isn_user.getsendbuf(&obuf, 5) == 5) {
            memcpy(obuf, "User\n", 5);
            isn_user.send(obuf, 5);
        }
        else {
            isn_user.free(obuf);
        }
    }
}


/*----------------------------------------------------------*/
/* Terminal I/O, Simple Echo                                */
/*----------------------------------------------------------*/

const uint8_t * isn_terminal_recv(const uint8_t *buf, size_t size, isn_layer_t *caller) {
    uint8_t *obuf = NULL;

    if (size==1 && *buf==ISN_PROTO_PING) {
        if ( caller->getsendbuf(&obuf, 4)==4 ) {
            isn_msg_send(1,1);
            memcpy(obuf, "ping", 4);
            caller->send(obuf, 4);
        }
        else {
            caller->free(obuf);
        }
    }
    else {
        if ( caller->getsendbuf(&obuf, size)==size ) {
            memcpy(obuf, buf, size);
            caller->send(obuf, size);
        }
        else {
            caller->free(obuf);
        }
    }
    return buf;
}

isn_layer_t isn_terminal = {
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
    isn_msg_init(isn_msg_table, ISN_MSG_TABLE_SIZE(isn_msg_table), &isn_frame);

    static isn_bindings_t isn_bindings[] = {
        {ISN_PROTO_USER1, &isn_user},
        {ISN_PROTO_MSG, &isn_message},
        {ISN_PROTO_OTHERWISE, &isn_terminal}
    };
    isn_frame_init(ISN_FRAME_MODE_COMPACT, isn_bindings, &isn_usbuart, &counter_1kHz, 100 /*ms*/);
    CySysTickStart();
    CySysTickSetCallback(0, systick_1kHz);
    PWM_LEDB_Start();
    PWM_HS_Start();
    CyGlobalIntEnable;

    isn_usbuart_init(USBUART_3V_OPERATION, &isn_frame);

    while(1) {
        isn_usbuart_poll();
        user1_streamout();
        if ( !isn_msg_sched() ) {
            asm volatile("wfi");
        }
    }
}
