import threading
import serial
from cmdmessenger import CmdMessenger, CmdMessengerHandler
from enum import Enum

class Command(Enum):
    '''Command IDs'''
    msg             = 0
    ack             = 1
    touch_event     = 2
    status_event    = 3
    dump_settings   = 4
    reset           = 5
    set_byte        = 6
    get_i2c         = 7
    set_i2c         = 8
    status_request  = 9

class Electrode(Enum):
    '''Electrode names'''
    top     = 0
    left    = 1
    bottom  = 2
    right   = 3
    center  = 4

class Gesture(Enum):
    '''Gesture names'''
    unknown     = 0
    tap         = 1
    double_tap  = 2
    swipe_up    = 3
    swipe_down  = 4
    swipe_left  = 5
    swipe_right = 6


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

    @CmdMessengerHandler.handler(cmdid=Command.ack)
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
        with self.messenger.writer(cmdid=Command.reset) as w:
            w.send_char(int(nodeid))
            w.send_bool(hard)
        return self.input_thread.wait_for_ack(self.ack_timeout)

    def status(self, nodeid):
        '''Request status from switch'''
        with self.messenger.writer(cmdid=Command.status_request) as w:
            w.send_int16(nodeid)
        return self.input_thread.wait_for_ack(self.ack_timeout)

    def dump(self, nodeid):
        '''Request a memory dump of switch settings'''
        with self.messenger.writer(cmdid=Command.dump_settings) as w:
            w.send_char(nodeid)
        return self.input_thread.wait_for_ack(self.ack_timeout)

    def setbyte(self, nodeid, offset, value):
        '''Sets a configuration byte'''
        with self.messenger.writer(cmdid=Command.set_byte) as w:
            w.send_int8(nodeid)
            w.send_int8(int(offset, 0))
            w.send_int8(int(value, 0))
        return self.input_thread.wait_for_ack(self.ack_timeout)

    def geti2c(self, nodeid, address, register):
        '''Gets an I2C register value'''
        with self.messenger.writer(cmdid=Command.get_i2c) as w:
            w.send_int8(nodeid)
            w.send_int8(int(address, 0))
            w.send_int8(int(register, 0))
        return self.input_thread.wait_for_ack(self.ack_timeout)

    def seti2c(self, nodeid, address, register, value):
        '''Sets an I2C register value, given address, register, value:
           seti2c 0x5A 0x20 0x12'''
        with self.messenger.writer(cmdid=Command.set_i2c) as w:
            w.send_int8(nodeid)
            w.send_int8(int(address, 0))
            w.send_int8(int(register, 0))
            w.send_int8(int(value, 0))
        return self.input_thread.wait_for_ack(self.ack_timeout)
