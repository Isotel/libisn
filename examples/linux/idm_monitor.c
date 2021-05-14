/** \file
 *  \brief Example of Xilinx I2C Thermometers exposed to UDP
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>

#include <isn_msg.h>
#include <isn_dispatch.h>
#include <posix/isn_udp.h>

#define POLL_TIMEOUT_QUICK_MS       1
#define POLL_TIMEOUT_SLEEP_MS       1000

isn_message_t isn_message;
isn_dispatch_t isn_dispatch;

struct {
    int16_t ps_main, ps_remote, pl;
} xlnx_temp_struct;

struct {
    int16_t ip1, fpga, board;
} tmp100_temp_struct;

static uint64_t serial = 0x1234567890ABCDEF;

static const char *xlnx_ps_main_temperature_sensor = "/sys/bus/iio/devices/iio:device0/in_temp0_ps_temp_raw";
static const char *xlnx_ps_remote_temperature_sensor = "/sys/bus/iio/devices/iio:device0/in_temp1_remote_temp_raw";
static const char *xlnx_pl_temperature_sensor = "/sys/bus/iio/devices/iio:device0/in_temp2_pl_temp_raw";

typedef struct {
    const char *i2c_device;
    uint8_t address;
} tmp100_devices_t;

tmp100_devices_t tmp100_ip1 = { .i2c_device = "/dev/i2c-1", .address = 0x48 };
tmp100_devices_t tmp100_fpga = { .i2c_device = "/dev/i2c-1", .address = 0x49 };
tmp100_devices_t tmp100_board = { .i2c_device = "/dev/i2c-1", .address = 0x4a };


/*--------------------------------------------------------------------*/
/* I2C Thermometers                                                   */
/*--------------------------------------------------------------------*/

static int read_sysfs_file(const char *file_name, void *buf, size_t sz) {
    int fd = open(file_name, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "unable to open %s: %s\n", file_name, strerror(errno));
        return -1;
    }
    int c = read(fd, buf, sz - 1);
    if (c > 0) {
        ((char *) buf)[c] = '\0';
    }
    close(fd);
    return c;
}

static int setup_tmp100(const tmp100_devices_t *dev) {
    const uint8_t setup_12bit_mode = 0x60;
    int ret = -1;
    int fd = open(dev->i2c_device, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "unable to open %s: %s\n", dev->i2c_device, strerror(errno));
        return -1;
    }
    unsigned char buf[2] = { 0x1, setup_12bit_mode };
    struct i2c_msg messages[] = {
            { .addr = dev->address, .flags = 0, .len = sizeof(buf), .buf = buf }
    };
    struct i2c_rdwr_ioctl_data packets = { .msgs = messages, .nmsgs = sizeof(messages) / sizeof(messages[0]) };
    if (ioctl(fd, I2C_RDWR, &packets) < 0) {
        fprintf(stderr, "unable to issue ioctl to %s: %s\n", dev->i2c_device, strerror(errno));
        goto exit;
    }
    ret = 0;
exit:
    close(fd);
    return ret;
}

static double get_tmp100_temperature(const tmp100_devices_t *dev) {
    double ret = 0.0;
    int fd = open(dev->i2c_device, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "unable to open %s: %s\n", dev->i2c_device, strerror(errno));
        return -1;
    }
    unsigned char addr_buf[] = { 0 };
    unsigned char value_buf[2];
    struct i2c_msg messages[] = {
            { .addr = dev->address, .flags = 0, .len = sizeof(addr_buf), .buf = addr_buf },
            { .addr = dev->address, .flags = I2C_M_RD, .len = sizeof(value_buf), .buf = value_buf }
    };
    struct i2c_rdwr_ioctl_data packets = { .msgs = messages, .nmsgs = sizeof(messages) / sizeof(messages[0]) };

    if (ioctl(fd, I2C_RDWR, &packets) < 0) {
        fprintf(stderr, "unable to issue ioctl to %s: %s\n", dev->i2c_device, strerror(errno));
        goto exit;
    }
    ret = (value_buf[0] << 8 | value_buf[1]) >> 4;
    ret = ret * 0.0625;
    ret = ret > 128.0 ? ret - 256.0 : ret;
    //printf("R:%2x: %02x%02x\n", dev->address, value_buf[0], value_buf[1]);
exit:
    close(fd);
    return ret;
}

static double get_xilinx_temperature(const char *file_name) {
    double ret = 0.0;
    char buf[64];
    if (read_sysfs_file(file_name, buf, sizeof(buf)) > 0) {
        ret = atof(buf);
        ret = ret * 509.314 / 65536.0 - 280.23;
    }
    return ret;
}

/*--------------------------------------------------------------------*/
/* ISN Messages                                                       */
/*--------------------------------------------------------------------*/

static void *serial_cb(const void *UNUSED(data)) {
    return &serial;
}

static void *xlnx_temp_cb(const void *UNUSED(data)) {
    xlnx_temp_struct.ps_main = get_xilinx_temperature(xlnx_ps_main_temperature_sensor) * 100.0;
    xlnx_temp_struct.ps_remote = get_xilinx_temperature(xlnx_ps_remote_temperature_sensor) * 100.0;
    xlnx_temp_struct.pl = get_xilinx_temperature(xlnx_pl_temperature_sensor) * 100.0;
    return &xlnx_temp_struct;
}

static void *tmp100_temp_cb(const void *UNUSED(data)) {
    tmp100_temp_struct.ip1 = get_tmp100_temperature(&tmp100_ip1) * 100.0;
    tmp100_temp_struct.fpga = get_tmp100_temperature(&tmp100_fpga) * 100.0;
    tmp100_temp_struct.board = get_tmp100_temperature(&tmp100_board) * 100.0;
    return &tmp100_temp_struct;
}

// Triggers every second by IDM unless this device is sending other data
const void * ping_recv(isn_layer_t *drv, const void *src, size_t size, isn_driver_t *caller) {
    isn_msg_send(&isn_message, 1, 1);
    isn_msg_send(&isn_message, 6, 1);
    return src;
}

static isn_msg_table_t isn_msg_table[] = {
    { 0, sizeof(uint64_t), serial_cb, "%T0{SiriusXHS} V1.0 {#sno}={%<Lx}" },
    /*1*/
    { 0, sizeof(xlnx_temp_struct), xlnx_temp_cb, "%T1d{Temperature} %T2{Xilinx} xilinx{" },
    { 0, 0, NULL, "+{:PSmain}={%<j/100}[oC]" },
    { 0, 0, NULL, "+{:PSremote}={%<j/100}[oC]" },
    { 0, 0, NULL, "+{:PL}={%<j/100}[oC]" },
    { 0, 0, NULL, "+}" },
    /*6*/
    { 0, sizeof(tmp100_temp_struct), tmp100_temp_cb, "%T2{TMP100} tmp100{" },
    { 0, 0, NULL, "+{:IP1}={%<j/100}[oC]" },
    { 0, 0, NULL, "+{:FPGA}={%<j/100}[oC]" },
    { 0, 0, NULL, "+{:Board}={%<j/100}[oC]" },
    { 0, 0, NULL, "+}" },
    ISN_MSG_DESC_END(0)
};

static isn_bindings_t isn_bindings[] = {
    {ISN_PROTO_MSG, &isn_message},
    {ISN_PROTO_PING, &(isn_receiver_t){ping_recv} },
    {ISN_PROTO_LISTEND, NULL}
};

/*--------------------------------------------------------------------*/
/* Main                                                               */
/*--------------------------------------------------------------------*/

static void usage(const char *prog) {
    fprintf(stdout, "usage: %s [-p port]\n", prog);
}

#ifdef __CLION_IDE__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
#endif
int main(int argc, char *argv[]) {
    uint16_t port = ISN_UDP_DEFAULT_SERVERPORT;
    int opt;
    while ((opt = getopt(argc, argv, "hp:vd")) != -1) {
        switch (opt) {
            case 'p':
                port = atoi(optarg);
                break;
            case 'v':
                isn_udp_driver_setlogging(ISN_LOGGER_LOG_LEVEL_INFO);
                break;
            case 'd':
                isn_udp_driver_setlogging(ISN_LOGGER_LOG_LEVEL_DEBUG);
                break;
            case 'h':
                usage(argv[0]);
                exit(EXIT_SUCCESS);
            default:
                usage(argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    setup_tmp100(&tmp100_ip1);
    setup_tmp100(&tmp100_fpga);
    setup_tmp100(&tmp100_board);

    isn_udp_driver_t *isn_udp_driver = isn_udp_driver_create(port, &isn_dispatch, 1);
    if (!isn_udp_driver) {
        fprintf(stderr, "unable to initialize UDP driver: %s, exiting\n", strerror(-errno));
        exit(1);
    }
    isn_dispatch_init(&isn_dispatch, isn_bindings);
    isn_msg_init(&isn_message, isn_msg_table, ARRAY_SIZE(isn_msg_table), isn_udp_driver);

    while(1) {
        isn_udp_driver_poll(isn_udp_driver, 
            isn_msg_sched(&isn_message) ? POLL_TIMEOUT_QUICK_MS : POLL_TIMEOUT_SLEEP_MS);
    }
}
#ifdef __CLION_IDE__
#pragma clang diagnostic pop
#endif
