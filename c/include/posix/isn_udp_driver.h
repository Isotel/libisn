#ifndef ISN_UDP_DRIVER_H
#define ISN_UDP_DRIVER_H

#include "time_utils.h"
#include "logger.h"

typedef struct isn_udp_driver_t isn_udp_driver_t;

int isn_udp_driver_poll(isn_udp_driver_t *driver, time_ms_t timeout);

isn_udp_driver_t *isn_udp_driver_alloc();
void isn_udp_driver_free(isn_udp_driver_t *driver);
int isn_udp_driver_init(isn_udp_driver_t *driver, uint16_t port, isn_layer_t *child);

void set_udp_driver_logging_level(logger_level_t level);

#endif //ISN_UDP_DRIVER_H
