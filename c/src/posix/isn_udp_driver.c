#include <isn.h>
#include <posix/isn_udp_driver.h>
#include <time_utils.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef int socket_length_type;
#else

#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

typedef socklen_t socket_length_type;
typedef int SOCKET;

#define INVALID_SOCKET (-1)
#endif

#include <stdlib.h>
#include <stdio.h>

#define MAXIMUM_PACKET_SIZE     64
#define MAXIMUM_CLIENTS         32
#define CLIENT_TIMEOUT_MS       5000

static logger_level_t logger_level;

typedef struct {
    struct sockaddr s_addr;
    time_ms_t last_access;
    socket_length_type s_addr_len;
} udp_client_t;

typedef struct {
    int active_clients;
    udp_client_t clients[MAXIMUM_CLIENTS];
} udp_clients_t;

static void udp_clients_init(udp_clients_t *cls) {
    memset(cls->clients, 0, sizeof(cls->clients));
}

static void udp_clients_insert(udp_clients_t *cls, time_ms_t tm,
                               const struct sockaddr *sa, socket_length_type sa_len) {
    for (unsigned i = 0; i < sizeof(cls->clients) / sizeof(cls->clients[0]); ++i) {
        udp_client_t *const uc = &cls->clients[i];
        if (uc->s_addr_len == 0) {
            uc->s_addr_len = sa_len;
            uc->last_access = tm;
            memcpy(&uc->s_addr, sa, sa_len);
            ++cls->active_clients;
            LOG_INFO(logger_level, "client connected %s", inet_ntoa(((struct sockaddr_in *) sa)->sin_addr))
            break;
        }
    }
}

static void udp_client_remove(udp_clients_t *cls, udp_client_t *uc) {
    LOG_INFO(logger_level, "client disconnected %s",
             inet_ntoa(((struct sockaddr_in *) &uc->s_addr)->sin_addr))
    uc->s_addr_len = 0;
    --cls->active_clients;
}

static void udp_clients_update(udp_clients_t *cls, const struct sockaddr *sa, socket_length_type sa_len) {
    const time_ms_t tm = monotonic_ms_time();
    if (tm < 0) {
        return;
    }
    int new_client = 1;

    for (unsigned i = 0; i < sizeof(cls->clients) / sizeof(cls->clients[0]); ++i) {
        udp_client_t *const uc = &cls->clients[i];

        if (uc->s_addr_len) {
            if (tm - uc->last_access > CLIENT_TIMEOUT_MS) {
                udp_client_remove(cls, uc);
            } else if (uc->s_addr_len == sa_len && memcmp(&uc->s_addr, sa, sa_len) == 0) {
                uc->last_access = tm;
                new_client = 0;
            }
        }
    }
    if (new_client) {
        udp_clients_insert(cls, tm, sa, sa_len);
    }
}

static void udp_clients_send(udp_clients_t *cls, SOCKET socket, const void *buf, size_t sz) {
    const time_ms_t tm = monotonic_ms_time();
    if (tm < 0 || cls->active_clients == 0) {
        return;
    }

    for (unsigned i = 0; i < sizeof(cls->clients) / sizeof(cls->clients[0]); ++i) {
        udp_client_t *const uc = &cls->clients[i];
        if (uc->s_addr_len) {
            if (tm - uc->last_access > CLIENT_TIMEOUT_MS) {
                udp_client_remove(cls, uc);
            } else {
                sendto(socket, (const char *) buf, sz, 0, &uc->s_addr, uc->s_addr_len);
            }
        }
    }
}

struct isn_udp_driver_t {
    isn_driver_t drv;
    isn_driver_t *child_driver;
    udp_clients_t clients;
    SOCKET sock;
    uint8_t tx_buf[MAXIMUM_PACKET_SIZE];
    int buf_locked;
};

/**/

static int wsa_startup() {
#ifdef _WIN32
    WORD w_version_requested = MAKEWORD(2, 2);
    WORD wsa_data;
    return WSAStartup(w_version_requested, &wsa_data);
#else
    return 0;
#endif
}

static int get_send_buf(isn_layer_t *drv, void **buf, size_t size) {
    isn_udp_driver_t *const driver = (isn_udp_driver_t *) drv;

    if (!driver->buf_locked) {
        if (buf) {
            driver->buf_locked = 1;
            *buf = driver->tx_buf;
        }
        return (size > MAXIMUM_PACKET_SIZE) ? MAXIMUM_PACKET_SIZE : size;
    }
    if (buf) {
        *buf = NULL;
    }
    return -1;
}

static void free_send_buf(isn_layer_t *drv, const void *buf) {
    isn_udp_driver_t *const driver = (isn_udp_driver_t *) drv;
    if (buf == driver->tx_buf) {
        driver->buf_locked = 0;             // we only support one buffer so we may free
    }
}

static int send_buf(isn_layer_t *drv, void *buf, size_t sz) {
    isn_udp_driver_t *const driver = (isn_udp_driver_t *) drv;
    udp_clients_send(&driver->clients, driver->sock, buf, sz);
    free_send_buf(driver, buf);
    return 0;
}

/*
 * public interface
 */

int isn_udp_driver_poll(isn_udp_driver_t *driver, time_ms_t timeout) {
    fd_set read_fds;
    struct timeval select_timeout = { timeout / 1000, timeout % 1000 * 1000 };

    FD_ZERO(&read_fds);
    FD_SET(driver->sock, &read_fds);

#ifdef _WIN32
    int ret = select(0, &read_fds, NULL, NULL, &select_timeout);
#else
    int ret = select(driver->sock + 1, &read_fds, NULL, NULL, &select_timeout);
#endif

    if (ret != 0 && FD_ISSET(driver->sock, &read_fds)) {
        char buf[MAXIMUM_PACKET_SIZE];
        struct sockaddr client_addr;
        socket_length_type sa_len = sizeof(struct sockaddr_in);

        const ssize_t sz = recvfrom(driver->sock, buf, sizeof(buf), 0,
                                    &client_addr,
                                    &sa_len);
        if (sz > 0) {
            udp_clients_update(&driver->clients, &client_addr, sa_len);
            driver->child_driver->recv(driver->child_driver, buf, sz, &driver->drv);
        }
    }

    return driver->clients.active_clients;
}

isn_udp_driver_t *isn_udp_driver_alloc() {
    isn_udp_driver_t* ret = malloc(sizeof(isn_udp_driver_t));
    return memset(ret, 0, sizeof(*ret));
}

#ifdef __CLION_IDE__
#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"
#endif
void isn_udp_driver_free(isn_udp_driver_t *driver) {
    free(driver);
}
#ifdef __CLION_IDE__
#pragma clang diagnostic pop
#endif

int isn_udp_driver_init(isn_udp_driver_t *driver, uint16_t port, isn_layer_t *child) {
    if (wsa_startup() != 0) {
        return -EINVAL;
    }

    LOG_INFO(logger_level, "starting udp driver, port: %u, maximum clients: %zu",
            port, sizeof(driver->clients.clients) / sizeof(driver->clients.clients[0]))

    driver->sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (driver->sock == INVALID_SOCKET) {
        return -EINVAL;
    }

    struct sockaddr_in s_addr;
    s_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    s_addr.sin_family = AF_INET;
    s_addr.sin_port = htons(port);

    if (setsockopt(driver->sock, SOL_SOCKET, SO_REUSEADDR, (const char *) &(int) { 1 }, sizeof(int)) == -1) {
        return -errno;
    }

    if (bind(driver->sock, (struct sockaddr *) (&s_addr), sizeof(struct sockaddr_in)) == -1) {
        return -errno;
    }

    memset(&driver->drv, 0, sizeof(driver->drv));
    driver->drv.getsendbuf = get_send_buf;
    driver->drv.send = send_buf;
    driver->drv.free = free_send_buf;
    driver->child_driver = child;

    udp_clients_init(&driver->clients);

    return 0;
}

void set_udp_driver_logging_level(logger_level_t level) {
    logger_level = level;
}

