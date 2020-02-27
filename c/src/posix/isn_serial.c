#include <isn.h>
#include <posix/isn_serial.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#ifdef _WIN32
#include <windows.h>
#include <fileapi.h>
#include <handleapi.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#endif

#define MAXIMUM_PACKET_SIZE 64

struct isn_serial_driver_s {
    isn_driver_t drv;
    isn_driver_t* child_driver;
    uint8_t tx_buf[MAXIMUM_PACKET_SIZE];
    int buf_locked;
#ifdef _WIN32
    HANDLE port_handle;
#else
    int fd;
#endif
    isn_serial_driver_params_t params;
};

isn_serial_driver_params_t isn_serial_driver_default_params = {
    .baud_rate = 115200,
    .data_bits = 8,
    .flow_control = ISN_FLOW_CONTROL_NONE,
    .parity = ISN_PARITY_NONE,
    .stop_bits = 1,
    .write_timeout_ms = 1000,
};

#ifndef _WIN32
typedef enum {
    MODE_SET,
    MODE_CLEAR,
} modify_mode_t;

static speed_t baud_rate_value_to_enum(int baud_rate);
static int modify_file_flags(int fd, int flags, modify_mode_t mode);
#endif

static isn_logger_level_t isn_logger_level = ISN_LOGGER_LOG_LEVEL_FATAL;

static char* hex_dump(const unsigned char* buffer, size_t size) {
    static char strbuf[MAXIMUM_PACKET_SIZE * 3];
    int offset = 0;
    for(size_t i = 0; i < size; ++i) {
        offset += snprintf(strbuf + offset, sizeof(strbuf) - offset, "%02X ", buffer[i]);
    }
    strbuf[offset ? offset - 1 : offset] = '\0';
    return strbuf;
}

static int get_send_buf(isn_layer_t* drv, void** buf, size_t size, isn_layer_t* caller) {
    isn_serial_driver_t* const driver = (isn_serial_driver_t*) drv;

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

static void free_send_buf(isn_layer_t* drv, const void* buf) {
    isn_serial_driver_t* const driver = (isn_serial_driver_t*) drv;
    if (buf == driver->tx_buf) {
        driver->buf_locked = 0; // we only support one buffer so we may free
    }
}

static int send_buf(isn_layer_t* drv, void* buf, size_t sz) {
    isn_serial_driver_t* const driver = (isn_serial_driver_t*) drv;
    LOG_TRACE(isn_logger_level, "sending %zd bytes [%s]", sz, hex_dump(buf, sz));
#ifdef _WIN32
    DWORD bytes_written;
    if (WriteFile(driver->port_handle, buf, sz, &bytes_written, NULL) == 0) {
        LOG_ERROR(isn_logger_level, "unable to write to serial port [%lu]", GetLastError())
        return -1;
    }
    if (bytes_written != sz) {
        LOG_ERROR(isn_logger_level, "wrote only %ld bytes of %lld bytes", bytes_written, sz)
        return -1;
    }
#else
    if (modify_file_flags(driver->fd, O_NONBLOCK, MODE_SET) == -1) {
        return -1;
    }

    ssize_t ret;
    int bytes_written;

    if ((ret = write(driver->fd, buf, sz)) == -1) {
        if (errno != EAGAIN) {
            LOG_ERROR(isn_logger_level, "unable to write to serial port [%s]",
                      strerror(errno))
            return -1;
        }
        struct timespec delay = {
            .tv_sec = driver->params.write_timeout_ms / 1000,
            .tv_nsec = driver->params.write_timeout_ms % 1000 * 1000000,
        };

        nanosleep(&delay, NULL);
        if ((ret = write(driver->fd, buf, sz)) == -1) {
            LOG_ERROR(isn_logger_level, "unable to write to serial port [%s]",
                      strerror(errno))
            return -1;
        }
    }
    bytes_written = ret;
    if(ret != sz) {
        LOG_ERROR(isn_logger_level, "unable to write to serial port %d != %d [%s]",
                  (int) ret, (int) sz, strerror(errno))
    }

#endif
    free_send_buf(driver, buf);
    return bytes_written;
}

static int isn_serial_driver_init(isn_serial_driver_t* driver, const char* port,
                                  const isn_serial_driver_params_t* params,
                                  isn_layer_t* child) {
#ifdef _WIN32
    char buf[128];
    if (snprintf(buf, sizeof(buf), "\\\\.\\%s", port) == -1) {
        return -1;
    }
    driver->port_handle =
        CreateFileA(buf,
                    GENERIC_READ | GENERIC_WRITE,
                    0, // share mode none
                    NULL, // security attributes
                    OPEN_EXISTING,
                    FILE_ATTRIBUTE_NORMAL,
                    0);
    if (driver->port_handle == INVALID_HANDLE_VALUE) {
        LOG_FATAL(isn_logger_level, "unable to open serial port %s [%lu]", port, GetLastError())
        return -1;
    }
    DCB dcb;
    if (GetCommState(driver->port_handle, &dcb) == 0) {
        LOG_FATAL(isn_logger_level, "unable to get state of the  serial port %s [%lu]", port, GetLastError())
        return -1;
    }

    dcb.BaudRate = params->baud_rate;

    assert(params->data_bits >= 5 && params->data_bits <= 8);
    dcb.ByteSize = params->data_bits;

    switch (params->flow_control) {
        case ISN_FLOW_CONTROL_NONE:
            dcb.fOutxCtsFlow = 0;
            dcb.fRtsControl = 0;
            dcb.fOutX = 0;
            dcb.fInX = 0;
            break;
        case ISN_FLOW_CONTROL_SOFTWARE:
            dcb.fOutxCtsFlow = 0;
            dcb.fRtsControl = 0;
            dcb.fOutX = 1;
            dcb.fInX = 1;
            break;
        case ISN_FLOW_CONTROL_HARDWARE:
            dcb.fOutxCtsFlow = 1;
            dcb.fRtsControl = 1;
            dcb.fOutX = 0;
            dcb.fInX = 0;
            break;
    }

    switch (params->parity) {
        case ISN_PARITY_NONE:
            dcb.Parity = NOPARITY;
            break;
        case ISN_PARITY_ODD:
            dcb.Parity = ODDPARITY;
            break;
        case ISN_PARITY_EVEN:
            dcb.Parity = EVENPARITY;
            break;
    }
    assert(params->stop_bits == 1 || params->stop_bits == 2);
    dcb.StopBits = params->stop_bits;
    if (SetCommState(driver->port_handle, &dcb) == 0) {
        LOG_FATAL(isn_logger_level, "unable to set state of the serial port %s [%lu]", port, GetLastError())
        return -1;
    }
    COMMTIMEOUTS timeouts = {
        .WriteTotalTimeoutConstant = params->write_timeout_ms,
    };
    if (SetCommTimeouts(driver->port_handle, &timeouts) == 0) {
        LOG_FATAL(isn_logger_level, "unable to set timeouts of the serial port %s [%lu]", port, GetLastError())
        return -1;
    }
#else
    if ((driver->fd = open(port, O_RDWR | O_NOCTTY)) == -1) {
        LOG_FATAL(isn_logger_level, "unable to open serial port %s [%s]", port, strerror(errno))
        return -1;
    }
    if (ioctl(driver->fd, TIOCEXCL) == -1) {
        LOG_FATAL(isn_logger_level, "unable to set exclusive port access ioctl(TIOCEXCL) [%s]", strerror(errno));
        return -1;
    }
    struct termios tios = {
        .c_cflag = CS8 | CREAD | CLOCAL,
    };
    const speed_t speed = baud_rate_value_to_enum(params->baud_rate);
    if(speed == 0) {
        LOG_FATAL(isn_logger_level, "specified speed %d is invalid", params->baud_rate);
        return -1;
    }
    cfsetispeed(&tios, speed);
    cfsetospeed(&tios, speed);

    if (tcflush(driver->fd, TCIOFLUSH) == -1) {
        LOG_FATAL(isn_logger_level, "unable to flush serial port buffer [%s]", strerror(errno));
        return -1;
    }
    if (tcsetattr(driver->fd, TCSANOW, &tios) == -1) {
        LOG_FATAL(isn_logger_level, "unable to set serial port parameters [%s]", strerror(errno));
        return -1;
    }

#endif
    driver->params = *params;
    memset(&driver->drv, 0, sizeof(driver->drv));
    driver->drv.getsendbuf = get_send_buf;
    driver->drv.send = send_buf;
    driver->drv.free = free_send_buf;
    driver->child_driver = child;

    return 0;
}

isn_serial_driver_t* isn_serial_driver_create(const char* port, const isn_serial_driver_params_t* params,
                                              isn_layer_t* child) {
    isn_serial_driver_t* driver = malloc(sizeof(isn_serial_driver_t));
    memset(driver, 0, sizeof(*driver));
    return isn_serial_driver_init(driver,
                                  port,
                                  params ? params : &isn_serial_driver_default_params, child) >= 0 ? driver
                                                                                                   : NULL;
}

int isn_serial_driver_poll(isn_serial_driver_t* driver, time_ms_t timeout) {
#ifdef _WIN32
    COMMTIMEOUTS timeouts = {
        .ReadTotalTimeoutConstant = timeout,
        .WriteTotalTimeoutConstant = driver->params.write_timeout_ms,
    };
    if (SetCommTimeouts(driver->port_handle, &timeouts) == 0) {
        LOG_FATAL(isn_logger_level, "unable to set timeouts of the serial port [%lu]", GetLastError())
        return -1;
    }
    char buf[MAXIMUM_PACKET_SIZE];
    DWORD bytes_read;
    if (ReadFile(driver->port_handle, buf, sizeof(buf), &bytes_read, NULL) == 0) {
        LOG_ERROR(isn_logger_level, "unable to read from the serial port [%lu]", GetLastError())
        return -1;
    }
#else
    fd_set read_fds;
    struct timeval tv = {
        .tv_sec = timeout / 1000,
        .tv_usec = timeout % 1000,
    };
    char buf[MAXIMUM_PACKET_SIZE];
    FD_ZERO(&read_fds);

    if (modify_file_flags(driver->fd, O_NONBLOCK, MODE_CLEAR) == -1) {
        return -1;
    }

    int bytes_read = 0;
    for (; bytes_read < sizeof(buf);) {
        int ret;
        FD_SET(driver->fd, &read_fds);

        if ((ret = select(driver->fd + 1, &read_fds, NULL, NULL, &tv)) == -1) {
            if (errno == EINTR || errno == EAGAIN) { continue; }
            LOG_ERROR(isn_logger_level, "select failed [%s]", strerror(errno));
            return -1;
        }

        if (ret == 0) {
            break;
        }

        if ((ret = read(driver->fd, buf + bytes_read, sizeof(buf) - bytes_read)) == -1) {
            LOG_ERROR(isn_logger_level, "read failed [%s]", strerror(errno));
            return -1;
        }
        bytes_read += ret;
    }
#endif
    if (bytes_read) {
        LOG_TRACE(isn_logger_level, "read %lu bytes [%s]", (long) bytes_read, hex_dump((unsigned char*) buf, bytes_read))
        driver->child_driver->recv(driver->child_driver, buf, bytes_read, &driver->drv);
    }

    return bytes_read;
}

void isn_serial_driver_free(isn_serial_driver_t* driver) {
#ifdef _WIN32
    if(driver) {
        CloseHandle(driver->port_handle);
    }
#else
    if(driver) {
        close(driver->fd);
    }
#endif
    free(driver);
}

void isn_serial_driver_setlogging(isn_logger_level_t level) {
    isn_logger_level = level;
}

#ifndef _WIN32
static speed_t baud_rate_value_to_enum(int baud_rate) {
    speed_t ret;
#define TRANSLATE(SPEED) \
    case SPEED: \
        ret = B ## SPEED; \
        break;

    switch (baud_rate) {
        TRANSLATE(50)
        TRANSLATE(75)
        TRANSLATE(110)
        TRANSLATE(134)
        TRANSLATE(150)
        TRANSLATE(200)
        TRANSLATE(300)
        TRANSLATE(600)
        TRANSLATE(1200)
        TRANSLATE(1800)
        TRANSLATE(2400)
        TRANSLATE(4800)
        TRANSLATE(9600)
        TRANSLATE(19200)
        TRANSLATE(38400)
        TRANSLATE(57600)
        TRANSLATE(115200)
        TRANSLATE(230400)
        TRANSLATE(460800)
        TRANSLATE(500000)
        TRANSLATE(576000)
        TRANSLATE(921600)
        TRANSLATE(1000000)
        TRANSLATE(1152000)
        TRANSLATE(1500000)
        TRANSLATE(2000000)
        TRANSLATE(2500000)
        TRANSLATE(3000000)
        TRANSLATE(3500000)
        TRANSLATE(4000000)
        default:
            ret = 0;
    }
    return ret;
#undef TRANSLATE
}

static int modify_file_flags(int fd, int flags, modify_mode_t mode) {
    int file_flags = fcntl(fd, F_GETFL);
    if (file_flags == -1) {
        LOG_FATAL(isn_logger_level, "unable to modify_file_flags: fcntl(F_GETFL) [%s]", strerror(errno));
        return -1;
    }

    switch (mode) {
        case MODE_SET:
            file_flags |= flags;
            break;
        case MODE_CLEAR:
            file_flags &= ~flags;
            break;
    }

    if (fcntl(fd, F_SETFL, file_flags) == -1) {
        LOG_FATAL(isn_logger_level, "unable to modify_file_flags: fcntl(F_GETFL) [%s]", strerror(errno));
        return -1;
    }

    return 0;
}
#endif
