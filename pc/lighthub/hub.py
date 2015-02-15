import threading
import serial
from cmdmessenger import CmdMessenger, CmdMessengerHandler

class Command:
    # Command IDs
    MSG             = 0
    ACK             = 1
    TOUCH_EVENT     = 2
    STATUS_EVENT    = 3
    DUMP_SETTINGS   = 4
    RESET           = 5
    SET_BYTE        = 6
    GET_I2C         = 7
    SET_I2C         = 8
    STATUS_REQUEST  = 9

    # Electrode ordered list
    ELECTRODES = [
        "top",
        "left",
        "bottom",
        "right",
        "center",
    ]

    # Gesture ordered list
    GESTURES = [
        "unknown",
        "tap",
        "double_tap",
        "swipe_up",
        "swipe_down",
        "swipe_left",
        "swipe_right",
    ]


class LightSwitchHubTimeout(Exception):
    pass


class SerialInputThread(threading.Thread):
    def __init__(self, messenger):
        super(SerialInputThread, self).__init__()
        self.messenger = messenger
        self.messenger.register_object(self)
        self.ack_condition = threading.Condition()
        self.running = False

    def run(self):
        self.running = True
        while self.running:
            self.messenger.read()

    @CmdMessengerHandler.handler(cmdid=Command.ACK)
    def handle_ack(self, msg):
        self.ack_condition.acquire()
        self.ack_msg = msg
        self.ack_condition.notify()
        self.ack_condition.release()

    def wait_for_ack(self, timeout=None):
        self.ack_condition.acquire()
        self.ack_msg = None
        self.ack_condition.wait(timeout)
        ret = self.ack_msg
        self.ack_condition.release()
        if not self.ack_msg:
            raise LightSwitchHubTimeout()
        return self.ack_msg

class LightSwitchHub(object):
    def __init__(self, handlers=None):
        self.connected = False
        self.connection = None
        self.ack_timeout = 1.0
        self.handlers = handlers

    def connect(self, port, baud, timeout):
        '''Connect to hub over serial port.'''
        self.connection = serial.Serial(port, baudrate=baud, timeout=timeout)
        self.connected = True
        self.messenger = CmdMessenger(self.connection)
        if self.handlers:
            self.messenger.register_object(self.handlers)
        self.input_thread = SerialInputThread(self.messenger)
        self.input_thread.start()

    def disconnect(self):
        '''Disconnect from hub'''
        if not self.connected:
            return
        self.input_thread.running = False
        self.connected = False
        self.connection.close()
        self.input_thread.join()

    def reset(self, nodeid, hard=False):
        '''Reset a switch'''
        with self.messenger.writer(cmdid=Command.RESET) as w:
            w.send_char(int(nodeid))
            w.send_bool(hard)
        return self.input_thread.wait_for_ack(self.ack_timeout)

    def status(self, nodeid):
        '''Request status from switch'''
        with self.messenger.writer(cmdid=Command.STATUS_REQUEST) as w:
            w.send_int16(nodeid)
        return self.input_thread.wait_for_ack(self.ack_timeout)

    def dump(self, nodeid):
        '''Request a memory dump of switch settings'''
        with self.messenger.writer(cmdid=Command.DUMP_SETTINGS) as w:
            w.send_char(nodeid)
        return self.input_thread.wait_for_ack(self.ack_timeout)

    def setbyte(self, nodeid, offset, value):
        '''Sets a configuration byte'''
        with self.messenger.writer(cmdid=Command.SET_BYTE) as w:
            w.send_int8(nodeid)
            w.send_int8(int(offset, 0))
            w.send_int8(int(value, 0))
        return self.input_thread.wait_for_ack(self.ack_timeout)

    def geti2c(self, nodeid, address, register):
        '''Gets an I2C register value'''
        with self.messenger.writer(cmdid=Command.GET_I2C) as w:
            w.send_int8(nodeid)
            w.send_int8(int(address, 0))
            w.send_int8(int(register, 0))
        return self.input_thread.wait_for_ack(self.ack_timeout)

    def seti2c(self, nodeid, address, register, value):
        '''Sets an I2C register value, given address, register, value:
           seti2c 0x5A 0x20 0x12'''
        with self.messenger.writer(cmdid=Command.SET_I2C) as w:
            w.send_int8(nodeid)
            w.send_int8(int(address, 0))
            w.send_int8(int(register, 0))
            w.send_int8(int(value, 0))
        return self.input_thread.wait_for_ack(self.ack_timeout)
