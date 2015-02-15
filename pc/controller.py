import serial
from cmdmessenger import CmdMessenger, CmdMessengerHandler
import threading
import binascii
import cmd

class Command:
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

electrodes = [
    "top",
    "left",
    "bottom",
    "right",
    "center",
]

gestures = [
    "unknown",
    "tap",
    "double_tap",
    "swipe_up",
    "swipe_down",
    "swipe_left",
    "swipe_right",
]

ack_condition = threading.Condition()
ack_msg = None

class Handler(CmdMessengerHandler):
    @CmdMessengerHandler.handler(cmdid=Command.MSG)
    def handle_debug(self, msg):
        print("debug: {}".format(msg.read_str()))

    @CmdMessengerHandler.handler(cmdid=Command.ACK)
    def handle_ack(self, msg):
        global ack_msg
        ack_condition.acquire()
        ack_msg = msg
        ack_condition.notify()
        ack_condition.release()

    @CmdMessengerHandler.handler(cmdid=Command.TOUCH_EVENT)
    def handle_touch_event(self, msg):
        nodeid = msg.read_int8()
        gesture = gestures[msg.read_int8()]
        electrode = electrodes[msg.read_int8()]
        repeat = msg.read_int8()
        print('[{}] gesture: {}, electrode: {}, repeat: {}'.format(nodeid,
            gesture, electrode, repeat))

    @CmdMessengerHandler.handler(cmdid=Command.STATUS_EVENT)
    def handle_status_event(self, msg):
        nodeid = msg.read_int8()
        vcc = msg.read_int32()
        count = msg.read_int16()
        print("[{}] status: vcc {}, count {}".format(nodeid, vcc, count))

    @CmdMessengerHandler.handler(cmdid=Command.DUMP_SETTINGS)
    def handle_dump_settings(self, msg):
        nodeid = msg.read_int8()
        print("[{}] Dump settings:".format(nodeid))
        settings = msg.read_bytes()
        settings = str(binascii.hexlify(settings), 'ascii')
        settings = [settings[i:i+2] for i in range(0, len(settings), 2)]
        for i in range(0, len(settings), 8):
            print('{:#04x}: {}'.format(i, ' '.join(settings[i:i+8])))

    @CmdMessengerHandler.handler(cmdid=Command.GET_I2C)
    def handle_get_i2c(self, msg):
        nodeid = msg.read_int8()
        address = msg.read_int8()
        register = msg.read_int8()
        value = msg.read_int8()
        print("[{}] i2c {:#04x}, register {:#04x} : {:#04x}".format(
            nodeid, address, register, value))


class SerialInputThread(threading.Thread):
    def __init__(self, msg):
        super(SerialInputThread, self).__init__()
        self.msg = msg
        self.running = False

    def run(self):
        print("Starting input thread...")
        self.running = True
        while self.running:
            self.msg.read()
        print("Closing input thread...")

    def wait_for_ack(self, timeout=None):
        global ack_msg
        ack_condition.acquire()
        ack_msg = None
        ack_condition.wait(timeout)
        ret = ack_msg
        ack_condition.release()
        return ack_msg


class ControllerShell(cmd.Cmd):
    intro = 'switch controller shell.  Type help or ? to list commands.\n'
    prompt = 'hub> '
    connected = False

    def emptyline(self):
        return

    def do_connect(self, args):
        'Connect to a serial port'
        if self.connected:
            print('Already connected to {}'.format(self.connection.name))
            return
        port, args = args.partition(' ')[::2]
        baud, args = args.partition(' ')[::2]
        timeout, args = args.partition(' ')[::2]
        port = port or '/dev/ttyUSB0'
        baud = int(baud or 115200)
        timeout = float(timeout or 1.0)
        print("Connecting to {} at {} baud, timeout {}".format(port, baud, timeout))
        try:
            self.connection = serial.Serial(port, baudrate=baud, timeout=timeout)
        except serial.serialutil.SerialException as e:
            print(e)
            return
        self.msg = CmdMessenger(self.connection)
        Handler(self.msg)
        self.input_thread = SerialInputThread(self.msg)
        self.input_thread.start()
        self.connected = True
        self.nodeid = None

    def do_disconnect(self, args):
        'Disconnect from serial port'
        if not self.connected:
            print('Not connected')
            return
        print("Disconnecting...")
        self.input_thread.running = False
        self.connected = False
        self.connection.close()

    def do_node(self, args):
        'Sets the current nodeid: node 2'
        try:
            self.nodeid = int(args)
            self.prompt = 'node:{}> '.format(self.nodeid)
        except:
            print("Invalid node id: {}".format(args))

    def _reset_command(self, nodeid, hard=False):
        with self.msg.writer(cmdid=Command.RESET) as w:
            w.send_char(int(nodeid))
            w.send_bool(hard)
        msg = self.input_thread.wait_for_ack(1.0)
        if msg:
            print('Tap switch {} to reset.'.format(nodeid))
        else:
            print('No ACK received')

    def do_reset(self, args):
        'Reset the current switch'
        if not self.nodeid:
            print('Must select a node first')
            return
        self._reset_command(self.nodeid)

    def do_hardreset(self, args):
        'Reset the current switch and its settings'
        if not self.nodeid:
            print('Must select a node first')
            return
        self._reset_command(self.nodeid, hard=True)

    def do_status(self, args):
        'Request status from switch'
        if not self.nodeid:
            print('Must select a node first')
            return
        with self.msg.writer(cmdid=Command.STATUS_REQUEST) as w:
            w.send_int16(self.nodeid)
        msg = self.input_thread.wait_for_ack(1.0)
        if msg:
            print('Tap switch {} to get status.'.format(self.nodeid))
        else:
            print('No ACK received')

    def do_dump(self, args):
        'Dump switch settings'
        if not self.nodeid:
            print('Must select a node first')
            return
        with self.msg.writer(cmdid=Command.DUMP_SETTINGS) as w:
            w.send_char(self.nodeid)
        msg = self.input_thread.wait_for_ack(1.0)
        if msg:
            print('Tap switch {} to dump settings.'.format(self.nodeid))
        else:
            print('No ACK received')

    def do_setbyte(self, args):
        'Sets a configuration byte, given offset, value: setbyte 2 2'
        offset, args = args.partition(' ')[::2]
        value, args = args.partition(' ')[::2]
        if not self.nodeid or not offset or not value:
            print('Missing required argument')
            return
        with self.msg.writer(cmdid=Command.SET_BYTE) as w:
            w.send_int8(self.nodeid)
            w.send_int8(int(offset, 0))
            w.send_int8(int(value, 0))
        msg = self.input_thread.wait_for_ack(1.0)
        if msg:
            print('Tap switch {} to set configuration.'.format(self.nodeid))
            n = msg.read_int8()
            o = msg.read_int8()
            v = msg.read_int8()
            print('nodeid:{} offset:{:#04x} value:{:#04x}'.format(n,o,v))
        else:
            print('No ACK received')

    def do_geti2c(self, args):
        '''Gets an I2C register value, given address, register: geti2c 0x5A 0x20'''
        address, args = args.partition(' ')[::2]
        register, args = args.partition(' ')[::2]
        if not self.nodeid or not address or not register:
            print('Missing required argument')
            return
        with self.msg.writer(cmdid=Command.GET_I2C) as w:
            w.send_int8(self.nodeid)
            w.send_int8(int(address, 0))
            w.send_int8(int(register, 0))
        msg = self.input_thread.wait_for_ack(1.0)
        if msg:
            print('Tap switch {} to get register.'.format(self.nodeid))
            n = msg.read_int8()
            a = msg.read_int8()
            r = msg.read_int8()
            print('nodeid:{} address:{:#04x} register:{:#04x}'.format(n,a,r))
        else:
            print('No ACK received')

    def do_seti2c(self, args):
        '''Sets an I2C register value, given address, register, value:
           seti2c 0x5A 0x20 0x12'''
        address, args = args.partition(' ')[::2]
        register, args = args.partition(' ')[::2]
        value, args = args.partition(' ')[::2]
        if not self.nodeid or not address or not register or not value:
            print('Missing required argument')
            return
        with self.msg.writer(cmdid=Command.SET_I2C) as w:
            w.send_int8(self.nodeid)
            w.send_int8(int(address, 0))
            w.send_int8(int(register, 0))
            w.send_int8(int(value, 0))
        msg = self.input_thread.wait_for_ack(1.0)
        if msg:
            print('Tap switch {} to set register.'.format(self.nodeid))
            n = msg.read_int8()
            a = msg.read_int8()
            r = msg.read_int8()
            v = msg.read_int8()
            print('nodeid:{} address:{:#04x} register:{:#04x} value:{:#04x}'.format(
                n,a,r,v))
        else:
            print('No ACK received')

    def do_exit(self, args):
        'Exits the shell'
        if self.connected:
            self.connection.close()
            self.input_thread.running = False
        return True

if __name__ == '__main__':
    c = ControllerShell()
    try:
        c.cmdloop()
    except KeyboardInterrupt:
        print()
    finally:
        if c.connected:
            c.input_thread.running = False
