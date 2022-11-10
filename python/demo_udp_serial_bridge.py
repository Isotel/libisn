"""
Serial <-> UDP bridge demo
"""
from clibisn import *


def start(serial_port, udp_port=33010):

    redirect_fw2udp = Redirect()
    redirect_fw2serial = Redirect()

    frame = Frame()
    udp = UDP(udp_port,  redirect_fw2serial, logging=True)

    serial = Serial(serial_port, frame, baudrate=9600)
    frame.init(redirect_fw2udp, None, serial, 1000)

    redirect_fw2udp.init(udp)
    redirect_fw2serial.init(frame)

    c = 0
    while True:

        r_udp = udp.poll()
        print("udp poll: " + str(c) + " len: " + str(r_udp))
        r_ser = serial.poll()
        print("serial poll: " + str(c) + " len: " + str(r_ser))
        c += 1


clib.disable_stdout_buffer()
start("COM5")
