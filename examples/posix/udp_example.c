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

#include <isn_msg.h>
#include <isn_dispatch.h>
#include <posix/isn_udp.h>

#define POLL_TIMEOUT_MS       5

isn_message_t isn_message;
isn_dispatch_t isn_dispatch;

/*--------------------------------------------------------------------*/
/* ISN Messages                                                       */
/*--------------------------------------------------------------------*/

static uint64_t serial = 0x1234567890ABCDEF;

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

static void* serial_cb(const void* UNUSED(data)) {
    return &serial;
}

static void* counter_cb(const void* data) {
    counter.x++;
    if (data) {
        counter = *(const counter_t*) data;
    }
    return &counter;
}

// Triggers every second by IDM unless this device is sending other data
size_t ping_recv(isn_layer_t* drv, const void* src, size_t size, isn_driver_t* caller) {
    isn_msg_sendby(&isn_message, counter_cb, ISN_MSG_PRI_NORMAL);
    return size;
}

static isn_msg_table_t isn_msg_table[] = {
    { 0, sizeof(uint64_t), serial_cb, "%T0{UDP Example} V1.0 {#sno}={%<Lx}" },
    { 0, sizeof(counter_t), counter_cb, "Example {:counter}={%lu}" },
    ISN_MSG_DESC_END(0)
};

static isn_bindings_t isn_bindings[] = {
    { ISN_PROTO_MSG,  &isn_message },
    { ISN_PROTO_PING, &(isn_receiver_t) { ping_recv }},
    { ISN_PROTO_LISTEND, NULL }
};

/*--------------------------------------------------------------------*/
/* Main                                                               */
/*--------------------------------------------------------------------*/

#ifdef __CLION_IDE__
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wmissing-noreturn"
#endif
int main(int argc, char* argv[]) {
    uint16_t serverport = ISN_UDP_DEFAULT_SERVERPORT;
    int opt;
    while ((opt = getopt(argc, argv, "hp:")) != -1) {
        switch (opt) {
            case 'p':
                serverport = atoi(optarg);
                break;
            default:
                fprintf(stdout, "usage: %s [-p port]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    isn_udp_driver_setlogging(ISN_LOGGER_LOG_LEVEL_DEBUG);
    isn_udp_driver_t* isn_udp_driver = isn_udp_driver_create(serverport, &isn_dispatch, 1);
    if (!isn_udp_driver) {
        fprintf(stderr, "unable to initialize UDP driver: %s, exiting\n", strerror(-errno));
        exit(1);
    }
    isn_dispatch_init(&isn_dispatch, isn_bindings);
    isn_msg_init(&isn_message, isn_msg_table, ARRAY_SIZE(isn_msg_table), isn_udp_driver);

    // add client and send at least one, the first message, to establish connection
    // Number of other clients may in addition connect to this udp server to the given port
    isn_udp_driver_addclient(isn_udp_driver, "255.255.255.255", "33005");
    isn_msg_sendby(&isn_message, counter_cb, ISN_MSG_PRI_NORMAL);

    while (1) {
        isn_udp_driver_poll(isn_udp_driver, POLL_TIMEOUT_MS);
        isn_msg_sched(&isn_message);
    }
}
#ifdef __CLION_IDE__
#  pragma clang diagnostic pop
#endif
