"""
Serial communication and protocol structure init test only
"""
from clibisn import *


clib.disable_stdout_buffer()

frame = Frame()
serial = Serial("COM5", frame, baudrate=9600)

msg = Message()
msg_table = Message.Msg * 2
t = msg_table()

t[0] = Message.Msg(0, 0, None, "%T0{Serial dummy comm example}")
t[1] = Message.Msg(0, 0, None, "%!")

frame.init(msg, None, serial, 1000)
msg.init(t, 3, frame, logging=True)

c = 0
while c < 50:

    res = serial.poll()
    print("testPoll: " +str(c) + " len: " + str(res))
    c += 1


print("finished!")
