/** \file
 *  \brief Example of UDP Server
 *  \author Uros Platise <uros@isotel.org>
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
    isn_serial_driver_params_t serial_parameters = isn_serial_driver_default_params;

    uint16_t server_port = ISN_UDP_DEFAULT_SERVERPORT;
    const char* serial_port = NULL;
    int trace_serial_port = 0;
    int opt;

    while ((opt = getopt(argc, argv, "hp:s:l:t")) != -1) {
        switch (opt) {
            case 's':
                serial_port = optarg;
                break;
            case 'p':
                server_port = atoi(optarg);
                break;
            case 'l': {
                char parity;
                int cnt = sscanf(optarg, "%d:%1d:%1[NEO]:%1d",
                                 &serial_parameters.baud_rate, &serial_parameters.data_bits,
                                 &parity, &serial_parameters.stop_bits);
                if (cnt != 4) {
                    fprintf(stderr, "invalid serial parameters specified: [%s]\n", optarg);
                    exit(EXIT_FAILURE);
                }
                if (parity == 'N') {
                    serial_parameters.parity = ISN_PARITY_NONE;
                } else if (parity == 'E') {
                    serial_parameters.parity = ISN_PARITY_EVEN;
                } else if (parity == 'O') {
                    serial_parameters.parity = ISN_PARITY_ODD;
                }
                break;
            }
            case 't':
                trace_serial_port = 1;
                break;

            default:
                fprintf(stdout, "usage: %s [-t trace serial calls] [-p port default: %d] -s serial\n"
                                "[-l serial port parameters speed:len:parity{NEO}:stop_bits, default: 115200:8:N:1]\n",
                        argv[0], server_port);
                exit(EXIT_FAILURE);
        }
    }
    if(serial_port == NULL) {
        fprintf(stderr, "serial port must be specified\n");
        exit(EXIT_FAILURE);
    }

    isn_udp_driver_setlogging(ISN_LOGGER_LOG_LEVEL_DEBUG);
    isn_udp_driver_t* isn_udp_driver = isn_udp_driver_create(server_port, &isn_fw2serial, 0);
    if (!isn_udp_driver) {
        fprintf(stderr, "unable to initialize UDP driver: %s, exiting\n",
                strerror(-errno));
        exit(1);
    }

    isn_serial_driver_setlogging(trace_serial_port ? ISN_LOGGER_LOG_LEVEL_TRACE : ISN_LOGGER_LOG_LEVEL_DEBUG);
    isn_serial_driver_t* isn_serial_driver = isn_serial_driver_create(serial_port, &serial_parameters, &isn_frame);
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
