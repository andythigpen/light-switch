import serial
from cmdmessenger import CmdMessenger
import binascii

s = serial.Serial('/dev/ttyUSB0', baudrate=115200, timeout=1)
cmd = CmdMessenger(s)

#TODO: add command line options to send reconfigure packet

@CmdMessenger.callback(cmdid=0)
def handle_debug(cmd):
    print(cmd.read_str())

@CmdMessenger.callback(cmdid=1)
def handle_touch_event(cmd):
    nodeid = cmd.read_int8()
    gesture = cmd.read_int8()
    electrode = cmd.read_int8()
    repeat = cmd.read_int8()
    print('[{}] gesture: {}, electrode: {} repeat: {}'.format(nodeid,
        gesture, electrode, repeat))

@CmdMessenger.callback(cmdid=2)
def handle_status_event(cmd):
    nodeid = cmd.read_int8()
    vcc = cmd.read_int8()
    print("[{}] status: vcc {}".format(nodeid, vcc))

@CmdMessenger.callback(cmdid=3)
def handle_dump_settings(cmd):
    nodeid = cmd.read_int8()
    print("[{}] Dump settings:".format(nodeid))
    settings = cmd.read_bytes()
    settings = str(binascii.hexlify(settings), 'ascii')
    print(':'.join(settings[i:i+2] for i in range(0, len(settings), 2)))

while True:
    cmd.read()
