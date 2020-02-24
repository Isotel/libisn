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

#include <isn.h>
#include <posix/isn_serial.h>
#include <posix/isn_udp.h>

#define POLL_TIMEOUT_MS 10

volatile uint32_t counter1k = 0;    // This should increment at rate of 1 kHz, maybe merge somehow with POLL TIMEOUT


/*--------------------------------------------------------------------*/
/* Main                                                               */
/*--------------------------------------------------------------------*/

#ifdef __CLION_IDE__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
#endif
int main(int argc, char* argv[]) {
    isn_frame_t isn_frame;
    isn_redirect_t isn_fw2udp, isn_fw2serial;

    uint16_t serverport = ISN_UDP_DEFAULT_SERVERPORT;
    const char* serial_port = NULL;
    int opt;

    while ((opt = getopt(argc, argv, "hp:s:")) != -1) {
        switch (opt) {
            case 's':
                serial_port = optarg;
                break;
            case 'p':
                serverport = atoi(optarg);
                break;
            default:
                fprintf(stdout, "usage: %s [-p port] [-s serial]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    isn_udp_driver_setlogging(ISN_LOGGER_LOG_LEVEL_DEBUG);
    isn_udp_driver_t* isn_udp_driver = isn_udp_driver_create(serverport, &isn_fw2serial, 0);
    if (!isn_udp_driver) {
        fprintf(stderr, "unable to initialize UDP driver: %s, exiting\n",
                strerror(-errno));
        exit(1);
    }

    isn_serial_driver_t* isn_serial_driver = isn_serial_driver_create(serial_port, NULL, &isn_frame);
    if (!isn_serial_driver) {
        fprintf(stderr, "unable to initialize serial driver: %s, exiting\n",
                strerror(-errno));
        exit(1);
    }

    isn_frame_init(&isn_frame, ISN_FRAME_MODE_COMPACT, &isn_fw2udp, NULL, isn_serial_driver, &counter1k, 100);

    isn_redirect_init(&isn_fw2udp, isn_udp_driver);
    isn_redirect_init(&isn_fw2serial, &isn_frame);

    while (1) {
        isn_udp_driver_poll(isn_udp_driver, POLL_TIMEOUT_MS);
        isn_serial_driver_poll(isn_serial_driver, POLL_TIMEOUT_MS);
    }
}
#ifdef __CLION_IDE__
#pragma clang diagnostic pop
#endif
