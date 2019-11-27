#include <isn.h>
#include <posix/isn_serial_driver.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#ifdef _WIN32

#include <windows.h>
#include <fileapi.h>
#include <handleapi.h>

#endif

#define MAXIMUM_PACKET_SIZE 64

struct isn_serial_driver_s {
    isn_driver_t drv;
    isn_driver_t* child_driver;
    uint8_t tx_buf[MAXIMUM_PACKET_SIZE];
    int buf_locked;
#ifdef _WIN32
    HANDLE port_handle;
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

static int get_send_buf(isn_layer_t* drv, void** buf, size_t size) {
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
    LOG_TRACE(isn_logger_level, "sending %lld bytes [%s]", sz, hex_dump(buf, sz));
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
#endif
    free_send_buf(driver, buf);
    return 0;
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
    if (bytes_read) {
        LOG_TRACE(isn_logger_level, "read %lu bytes [%s]", bytes_read, hex_dump((unsigned char*) buf, bytes_read))
        driver->child_driver->recv(driver->child_driver, buf, bytes_read, &driver->drv);
    }
    return bytes_read;
#else
    return 0;
#endif
}

void isn_serial_driver_free(isn_serial_driver_t* driver) {
#ifdef _WIN32
    if(driver) {
        CloseHandle(driver->port_handle);
    }
#endif
    free(driver);
}

void isn_serial_driver_setlogging(isn_logger_level_t level) {
    isn_logger_level = level;
}
