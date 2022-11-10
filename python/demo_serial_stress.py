
from clibisn import *
from ctypes import *
import time


class SerialStressDemo:
    """
    Serial Communication/query stress test

    Connects to serial ports and queries for arguments of message 0 and 1 in a loop, prints communication statistics
    """
    def __init__(self):
        self.counter = c_ulong(0)
        self.msg = None
        self.running = False

    def get_ping_cb(self):
        def ping_recv(drv, src, size, caller):
            self.msg.send(1)
            return size

        return get_recvptr(ping_recv)

    def millis(self):
        return int(round(time.time() * 1000))

    def print_stats(self, driver):
        """
        Print driver stats
        :param driver:
        :return:
        """
        clib.isn_serial_driver_get_stats.restype = my_void_p
        data = clib.isn_serial_driver_get_stats(driver.obj)
        val = cast(data, POINTER(DriverStats))[0]
        print("{} {} RX: {}, errors: {}, RX retries: {}, RX dropped {} TX: {}, TX retries: {}, Tx dropped: {}"
              .format(time.ctime(time.time()), self.millis(), val.rx_counter, val.rx_errors, val.rx_retries, val.rx_dropped,
                      val.tx_counter, val.tx_retries, val.tx_dropped))

    def start(self, port, baudrate=115200, duration=-1, stat_interval=-1, dump=False):
        """
        Start test
        :param port: Serial port
        :param baudrate: Baud rate
        :param duration: duration in [min]
        :param stat_interval:  periodic comm stat dump in [s], -1 disabled
        :param dump: if True, dump trace from serial driver
        :return:
        """

        self.msg = Message(2)
        self.msg.add("%T0{Serial Stress Demo} V1.1 {#sno}={%<Lx}", 0, None)
        self.msg.add("%!", 0, None)

        ping_rcv = Receiver(self.get_ping_cb(), ptr=True)
        frame = Frame()
        dispatch = Dispatch(2)
        dispatch.add(ISN_PROTO_MSG, self.msg.obj)
        dispatch.add(ISN_PROTO_PING, addressof(ping_rcv))

        dispatch.init()
        serial = Serial(port, frame, baudrate, logging=dump)

        frame.init(self.msg, None, serial, 1000)
        self.msg.init(frame, logging=True)

        end_time = -1 if duration <= 0 else time.time() + duration * 60
        dump_stat_interval = -1 if stat_interval <= 0 else stat_interval
        next_stat_dump = time.time() + dump_stat_interval
        self.running = True
        while self.running:
            try:

                self.msg.send(0, priority=Message.MSG_PRI_QUERY_ARGS)
                self.msg.send(1, priority=Message.MSG_PRI_QUERY_ARGS)
                self.msg.sched()
                self.counter.value = clib.isn_clock_update()
                serial.poll(timeout=1)
                now = time.time()
                if dump_stat_interval > 0 and now >= next_stat_dump:
                    self.print_stats(serial)
                    next_stat_dump = now + dump_stat_interval
                if 0 < end_time <= now:
                    self.running = False

            except KeyboardInterrupt or Exception as e:
                self.print_stats(serial)
                self.running = False
                raise e

        print("Finished")
        self.print_stats(serial)

    def stop(self):
        self.running = False


def main():
    clib.disable_stdout_buffer()
    d = SerialStressDemo()
    d.start("COM6", baudrate=115200, duration=10, stat_interval=60, dump=False)


if __name__ == "__main__":
    main()