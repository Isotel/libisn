from ctypes import *
import platform

ISN_PROTO_PING  = 0x00
ISN_PROTO_FRAME = 0x80
ISN_PROTO_MSG   = 0x7F
ISN_PROTO_TRANS = 0x7E
ISN_PROTO_TRANL = 0x7D

clib = CDLL("libisn.dll") if 'windows' in platform.system().lower() else CDLL("./libisn.so")


def get_cbptr(callback):
    """
    Get callback function pointer

    Utility function to convert python callback function into a pointer that can be passed to C library
    """
    CB_FUNCTION = CFUNCTYPE(my_void_p, my_void_p)
    cb = CB_FUNCTION(callback)
    return cast(cb, my_void_p)


def get_recvptr(function):
    """
    Get receiver function pointer

    Utility function to convert python receiver function into a pointer that can be passed to C library
    """
    CB_RECV = CFUNCTYPE(c_int, my_void_p, my_void_p, c_int, my_void_p)
    cb = CB_RECV(function)
    return cast(cb, my_void_p)


def io_write_atleast(layer, source, size, minsize):
    clib.isn_write_atleast.restype = c_int
    return clib.isn_write_atleast(layer, source, c_ulonglong(size), c_ulonglong(minsize))


def io_write(layer, source, size):
    return io_write_atleast(layer, source, size, size)


class my_void_p(c_void_p):
    """
    Override to prevent OverflowError being thrown in certain use-cases
    """
    pass


class User:

    def __init__(self):
        clib.isn_user_create.restype = my_void_p
        self.obj = clib.isn_user_create()

    def init(self, child, parent, identifier):
        clib.isn_user_init(self.obj, child.obj, parent.obj, c_uint8(identifier))

    def __del__(self):
        clib.isn_user_drop(self.obj)


class Transport:

    def __init__(self):
        clib.isn_trans_create.restype = my_void_p
        self.obj = clib.isn_trans_create()

    def init(self, child, parent, port):
        clib.isn_trans_init(self.obj, child.obj, parent.obj, c_uint8(port))

    def __del__(self):
        clib.isn_trans_drop(self.obj)


class Dup:

    def __init__(self):
        clib.isn_dup_create.restype = my_void_p
        self.obj = clib.isn_dup_create()

    def init(self, child1, child2):
        clib.isn_trans_init(self.obj, child1.obj, child2.obj)

    def __del__(self):
        clib.isn_dup_drop(self.obj)


class Frame:
    MODE_SHORT = 0
    MODE_COMPACT = 1

    def __init__(self):
        clib.isn_frame_create.restype = my_void_p
        self.obj = clib.isn_frame_create()

    def init(self, child, other, parent, timeout):

        clib.isn_frame_init(self.obj, c_int(1), child.obj if child else None, other.obj if other else None,
                            parent.obj, c_uint(timeout))

    def __del__(self):
        clib.isn_frame_drop(self.obj)


class Dispatch:

    def __init__(self, size):
        clib.isn_dispatch_create.restype = my_void_p
        self.obj = clib.isn_dispatch_create()
        self.size = int(size)
        bindings_table = Bindings * (size + 1)
        self.binds = bindings_table()
        self.last_bind = -1

    def __del__(self):
        clib.isn_dispatch_drop(self.obj)

    def init(self):
        assert self.last_bind + 1 == self.size, "Bind table size not matching, expected %d, found %d" % \
                                                (self.size, self.last_bind + 1)
        self.binds[self.size] = Bindings(Bindings.ISN_PROTO_LISTEND, None)
        bind = byref(self.binds)
        clib.isn_dispatch_init(self.obj, bind)

    def add(self, protocol, driver):
        self.last_bind += 1
        assert self.last_bind < self.size, "Add bind failed, bind %d is out of range" % self.last_bind
        self.binds[self.last_bind] = Bindings(protocol, driver)
        return self.last_bind

    def set(self, bind_num, protocol, driver):
        assert bind_num < self.size, "Set bind failed, bind %d is out of range" % bind_num
        self.binds[bind_num] = Bindings(protocol, driver)
        if bind_num > self.last_bind:
            self.last_bind = bind_num


class Receiver(Structure):
    _fields_ = [("recv", my_void_p)]

    def __init__(self, recv, ptr=False):
        self.recv = recv if ptr else get_recvptr(recv)
        super(Receiver, self).__init__(self.recv)


class Bindings(Structure):
    ISN_PROTO_OTHER = -1
    ISN_PROTO_LISTEND = -2

    _fields_ = [("protocol", c_int),
                ("driver", c_void_p)]

    def __init__(self, protocol, driver):

        self.protocol = c_int(protocol)
        if driver:
            self.driver = driver
        else:
            self.driver = None
        super(Bindings, self).__init__(protocol, driver)


class DriverStats(Structure):

    _fields_ = [
        ('rx_packets', c_uint32),
        ('rx_counter', c_uint32),
        ('rx_errors', c_uint32),
        ('rx_retries', c_uint32),
        ('rx_dropped', c_uint32),
        ('tx_packets', c_uint32),
        ('tx_counter', c_uint32),
        ('tx_dropped', c_uint32),
        ('tx_retries', c_uint32)
    ]
    _pack_ = 1


class Redirect:

    def __init__(self):
        clib.isn_redirect_create.restype = my_void_p
        self.obj = clib.isn_redirect_create()

    def __del__(self):
        clib.isn_redirect_drop(self.obj)

    def init(self, target):
        clib.isn_redirect_init(self.obj, target.obj)


class Message:
    DEFAULT_PRIORITY = 0
    MSG_PRI_DESCRIPTION = 31
    MSG_PRI_DESCRIPTIONLOW = 30
    MSG_PRI_QUERY_ARGS = 27
    MSG_PRI_QUERY_WAIT = 26
    MGG_PRI_UPDATE_ARGS = 25

    class Msg(Structure):
        _fields_ = [("priority", c_ubyte),
                    ("size", c_ubyte),
                    ("handler", my_void_p),
                    ("desc", c_char_p)]

        def __init__(self, desc, size, handler, ptr=True, priority=0):
            desc = c_char_p(desc.encode('utf-8'))

            self.priority = c_ubyte(priority)
            self.size = c_ubyte(size)

            if handler:
                self.handler = handler if ptr else get_cbptr(handler)
            else:
                self.handler = None
            self.desc = desc
            super(Message.Msg, self).__init__(priority, size, self.handler, desc)

        def get_handler(self):
            return self.handler

    def __init__(self, size):
        clib.isn_msg_create.restype = my_void_p
        self.obj = clib.isn_msg_create()
        self.size = int(size)
        msg_table = Message.Msg * size
        self.messages = msg_table()
        self.last_msg = -1

    def add(self, desc, size, handler, ptr=True, priority=DEFAULT_PRIORITY):
        self.last_msg += 1

        assert self.last_msg < self.size, "Add message failed, message %d is out of range" % self.last_msg

        self.messages[self.last_msg] = Message.Msg(desc, size, handler, ptr=ptr, priority=priority)
        return self.last_msg

    def set(self, msg_num, desc, size, handler, ptr=True, priority=DEFAULT_PRIORITY):
        assert msg_num < self.size, "Set message failed, message %d is out of range" % msg_num

        self.messages[msg_num] = Message.Msg(desc, size, handler, ptr=ptr, priority=priority)
        if msg_num > self.last_msg:
            self.last_msg = msg_num
        return msg_num

    def init(self, parent, logging=False):
        assert self.last_msg + 1 == self.size, "Message table size not matching, expected %d, found %d" % \
                                                (self.size, self.last_msg + 1)
        if logging:
            clib.isn_msg_setlogging(c_int(-6))
        msg_point = byref(self.messages)
        clib.isn_msg_init(self.obj, msg_point, c_uint8(self.size), parent.obj)
        return None

    def send(self, msg_num, priority=4):
        clib.isn_msg_send(self.obj, c_uint8(msg_num), priority)

    def sendqby(self, handler, priority=4):
        clib.isn_msg_sendqby(self.obj, handler, priority, 1)

    def sched(self):
        clib.isn_msg_sched(self.obj)

    def __del__(self):
        clib.isn_msg_drop(self.obj)


class Serial:
    PARITY_NONE = 0
    PARITY_ODD = 1
    PARITY_EVEN = 2

    class Params(Structure):
        _fields_ = [("baud_rate", c_int),
                    ("data_bits", c_int),
                    ("flow_control", c_int),
                    ("parity", c_int),
                    ("stop_bits", c_int),
                    ("write_timeout_ms", c_int)]

    """
    parity, 0 = none, 1 = odd, 2 = even
    """

    def __init__(self, port, child, baudrate=115200, databits=8, stopbits=1, parity=PARITY_NONE, logging=False):
        param = Serial.Params()
        param.baud_rate = baudrate
        param.flow_control = 0
        param.parity = parity
        param.data_bits = databits
        param.stop_bits = stopbits
        if logging:
            clib.isn_serial_driver_setlogging(c_int(-6))
        clib.isn_serial_driver_create.restype = my_void_p
        self.obj = clib.isn_serial_driver_create(bytes(port, "utf-8"), byref(param), child.obj)

    def __del__(self):
        clib.isn_serial_driver_free(self.obj)

    def poll(self, timeout=1000):
        clib.isn_serial_driver_poll.argtypes = (c_void_p, c_long)
        return clib.isn_serial_driver_poll(self.obj, c_long(timeout))


class UDP:

    def __init__(self, server_port, child, broadcast=0, logging=False):
        clib.isn_udp_driver_create.restype = my_void_p
        if logging:
            clib.isn_udp_driver_setlogging(c_int(-6))

        self.obj = clib.isn_udp_driver_create(c_ushort(server_port), child.obj, c_int(broadcast))

    def __del__(self):
        clib.isn_udp_driver_free(self.obj)

    def poll(self):
        clib.isn_udp_driver_poll.argtypes = (c_void_p, c_long)
        return clib.isn_udp_driver_poll(self.obj, c_long(1000))

    def add_client(self, host, port):
        chost = c_char_p(host.encode('utf-8'))
        cport = c_char_p(port.encode('utf-8'))

        clib.isn_udp_driver_addclient.argtypes = (c_void_p, c_char_p, c_char_p)
        return clib.isn_udp_driver_addclient(self.obj, chost, cport)
