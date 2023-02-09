from clibisn import *
from ctypes import *


class UDPDemo:
    """
    Simple UDP demo
    """
    def __init__(self):
        self.serial = c_ulonglong(0x1234567890ABCDEF)
        self.counter = c_ulong(0)
        self.msg = None
        self.serial_cb = self.get_serial_cb()
        self.counter_cb = self.get_counter_cb()
        self.ping_cb = self.get_ping_cb()

    def get_serial_cb(self):
        return get_cbptr(lambda x: addressof(self.serial))

    def get_counter_cb(self):
        def get_counter(data):
            if data:
                num = cast(data, POINTER(c_ulong))
                self.counter.value = num[0]
            else:
                self.counter.value += 1
            return addressof(self.counter)
        return get_cbptr(get_counter)

    def get_ping_cb(self):
        def ping_recv(drv, src, size, caller):
            self.msg.send(1)
            return size
        return get_recvptr(ping_recv)

    def start(self, port=33010):

        self.msg = Message(3)

        self.msg.add("%T0{UDP Example} V1.0 {#sno}={%<Lx}", sizeof(self.serial), self.serial_cb)
        self.msg.add("Example {:counter}={%lu}", sizeof(self.counter), self.counter_cb)
        self.msg.add("%!", 0, None)
        ping_rcv = Receiver(self.ping_cb, ptr=True)

        dispatch = Dispatch(2)
        dispatch.add(ISN_PROTO_MSG, self.msg.obj)
        dispatch.add(ISN_PROTO_PING, addressof(ping_rcv))
        dispatch.init()

        s = UDP(port, dispatch, logging=True)
        self.msg.init(s, logging=True)
        print(str(s.obj))

        c = 0
        while True:
            res = s.poll()
            print("testPoll: " + str(c) + " len: " + str(res))
            self.msg.sched()
            clib.isn_clock_update()
            c += 1


def main():
    clib.disable_stdout_buffer()
    d = UDPDemo()
    d.start()


if __name__ == "__main__":
    main()

