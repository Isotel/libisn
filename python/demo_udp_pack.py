from clibisn import *
from ctypes import *


class Pack(Structure):
    _fields_ = [
        ('p1', c_uint8),
        ('p2', c_uint16),
        ('p3', c_uint32),
        ('p4', c_float)
    ]
    _pack_ = 1


class UDPackDemo:
    """
    UDP demo with multi-parameter packed messages
    """
    def __init__(self):
        self.serial = c_ulonglong(0x1234567890ABCDEF)
        self.counter = c_ulong(0)
        self.pval = Pack(0xF, 0x0102, 0x04050607, 0.1234)
        self.msg = None
        self.serial_cb = self.get_serial_cb()
        self.counter_cb = self.get_counter_cb()
        self.packed_cb = self.get_packed_cb()
        self.ping_cb = self.get_ping_cb()

    def get_serial_cb(self):
        return get_cbptr(lambda x: addressof(self.serial))

    def get_packed_cb(self):
        def get_packed(data):
            if data:
                val = cast(data, POINTER(Pack))[0]
                self.pval = Pack(val.p1, val.p2, val.p3, val.p4)
            return addressof(self.pval)
        return get_cbptr(get_packed)

    def get_counter_cb(self):
        def get_counter(data):
            if data:
                num = cast(data, POINTER(c_ulong))
                self.counter.value = num[0]
            return addressof(self.counter)
        return get_cbptr(get_counter)

    def get_ping_cb(self):
        def ping_recv(drv, src, size, caller):
            self.msg.send(1)
            return size
        return get_recvptr(ping_recv)

    def start(self, port=33010):

        self.msg = Message(4)
        self.msg.add("%T0{UDP Example2} V1.1 {#sno}={%<Lx}", sizeof(self.serial), self.serial_cb)
        self.msg.add("Example {:counter}={%lu}", sizeof(self.counter), self.counter_cb)
        self.msg.add("P {:p1}={%hx}{:p2}={%x}{:p3}={%lx}{:p4}={%f}", sizeof(self.pval), self.packed_cb)
        self.msg.add("%!", 0, None)

        ping_rcv = Receiver(self.ping_cb, ptr=True)

        dispatch = Dispatch(2)
        dispatch.add(ISN_PROTO_MSG, self.msg.obj)
        dispatch.add(ISN_PROTO_PING, addressof(ping_rcv))
        dispatch.init()

        s = UDP(port, dispatch, logging=True)
        self.msg.init(s, logging=True)
        print(str(s.obj))

       # s.add_client("localhost", "31000")

        c = 0
        while True:
            res = s.poll()
            print("testPoll: " + str(c) + " len: " + str(res))
            self.msg.sched()
            self.counter.value = clib.isn_clock_update()
            c += 1


def main():
    clib.disable_stdout_buffer()
    d = UDPackDemo()
    d.start()


if __name__ == "__main__":
    main()
