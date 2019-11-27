/** \file
 *  \brief Example of UDP Server
 *  \author Uros Platise <uros@isotel.eu>
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _MSC_VER
#include <unistd.h>
#else

#include <posix/getopt.h>

#endif

#include <isn_dispatch.h>
#include <isn_msg.h>
#include <isn_frame.h>
#include <posix/isn_serial_driver.h>

#define POLL_TIMEOUT_MS 1000

isn_message_t isn_message;
isn_dispatch_t isn_dispatch;

/*--------------------------------------------------------------------*/
/* ISN Messages                                                       */
/*--------------------------------------------------------------------*/

static uint64_t serial = 0;

#ifdef _MSC_VER
#pragma pack(push, 1)
#endif
typedef struct {
    int32_t x;
}
#ifndef _MSC_VER
__attribute__((packed))
#else
#pragma pack(pop)
#endif
    counter_t;

counter_t counter = { 0 };

static void* serial_cb(const void* data) {
    if (data) {
        serial = *(const uint64_t*) data;
        printf("Received serial: %llx\n", serial);
        return NULL; // we do not return values as we ask the peer for it
    }
    return &serial;
}

static void* counter_cb(const void* data) {
    counter.x++;
    if (data) {
        counter = *(const counter_t*) data;
        printf("Received counter: %x\n", counter.x);
        return NULL;
    }
    return &counter;
}

// Triggers every second by IDM unless this device is sending other data
const void* ping_recv(isn_layer_t* drv, const void* src, size_t size,
                      isn_driver_t* caller) {
    isn_msg_sendby(&isn_message, counter_cb, ISN_MSG_PRI_NORMAL);
    return src;
}

static isn_msg_table_t isn_msg_table[] = {
    { 0, sizeof(uint64_t), serial_cb, "%T0{Serial Example} V1.0 {#sno}={%<Lx}" },
    { 0, sizeof(counter_t), counter_cb, "Example {:counter}={%lu}" },
    ISN_MSG_DESC_END(0)};

static isn_bindings_t isn_bindings[] = {
    { ISN_PROTO_MSG,  &isn_message },
    { ISN_PROTO_PING, &(isn_receiver_t) { ping_recv }},
    { ISN_PROTO_LISTEND, NULL }};

/*--------------------------------------------------------------------*/
/* Main                                                               */
/*--------------------------------------------------------------------*/

#ifdef __CLION_IDE__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
#endif
int main(int argc, char* argv[]) {
    const char* serial_port = NULL;
    int opt;
    while ((opt = getopt(argc, argv, "hp:")) != -1) {
        switch (opt) {
            case 'p':
                serial_port = optarg;
                break;
            default:
                fprintf(stdout, "usage: %s [-p port]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }
    if (serial_port == NULL) {
        fprintf(stdout, "usage: %s [-p port]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    volatile int32_t counter_1kHz;
    isn_frame_t isn_uart_frame;

    isn_serial_driver_setlogging(ISN_LOGGER_LOG_LEVEL_TRACE);

    // serial port setup with framing as a child
    isn_serial_driver_t* isn_serial_driver = isn_serial_driver_create(serial_port, NULL, &isn_uart_frame);
    if (!isn_serial_driver) {
        fprintf(stderr, "unable to initialize serial driver: %s, exiting\n",
                strerror(-errno));
        exit(1);
    }

    // initialize dispatching
    isn_dispatch_init(&isn_dispatch, isn_bindings);

    // initialize message table
    isn_msg_init(&isn_message, isn_msg_table, ARRAY_SIZE(isn_msg_table), &isn_uart_frame);

    // initialize framing with serial driver as a parent
    isn_frame_init(&isn_uart_frame, ISN_FRAME_MODE_COMPACT, &isn_message, NULL, isn_serial_driver,
                   &counter_1kHz, 100 /*ms*/);

    while (1) {
        isn_serial_driver_poll(isn_serial_driver, POLL_TIMEOUT_MS);
        isn_msg_sendqby(&isn_message, serial_cb, ISN_MSG_PRI_NORMAL, 0);
        isn_msg_sched(&isn_message);
    }
}
#ifdef __CLION_IDE__
#pragma clang diagnostic pop
#endif
