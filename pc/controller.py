import serial
from cmdmessenger import CmdMessenger
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

@CmdMessenger.callback(cmdid=Command.MSG)
def handle_debug(msg):
    print("debug: {}".format(msg.read_str()))

@CmdMessenger.callback(cmdid=Command.ACK)
def handle_ack(msg):
    global ack_msg
    ack_condition.acquire()
    ack_msg = msg
    ack_condition.notify()
    ack_condition.release()

@CmdMessenger.callback(cmdid=Command.TOUCH_EVENT)
def handle_touch_event(msg):
    nodeid = msg.read_int8()
    gesture = gestures[msg.read_int8()]
    electrode = electrodes[msg.read_int8()]
    repeat = msg.read_int8()
    print('[{}] gesture: {}, electrode: {}, repeat: {}'.format(nodeid,
        gesture, electrode, repeat))

@CmdMessenger.callback(cmdid=Command.STATUS_EVENT)
def handle_status_event(msg):
    nodeid = msg.read_int8()
    vcc = msg.read_int8()
    print("[{}] status: vcc {}".format(nodeid, vcc))

@CmdMessenger.callback(cmdid=Command.DUMP_SETTINGS)
def handle_dump_settings(msg):
    nodeid = msg.read_int8()
    print("[{}] Dump settings:".format(nodeid))
    settings = msg.read_bytes()
    settings = str(binascii.hexlify(settings), 'ascii')
    settings = [settings[i:i+2] for i in range(0, len(settings), 2)]
    for i in range(0, len(settings), 8):
        print('{:#04x}: {}'.format(i, ' '.join(settings[i:i+8])))


class SerialInputThread(threading.Thread):
    def __init__(self, msg):
        super(SerialInputThread, self).__init__()
        self.msg = msg
        self.running = False

    def run(self):
        self.running = True
        while self.running:
            self.msg.read()

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
    prompt = 'command> '
    connected = False

    def emptyline(self):
        return

    def do_connect(self, args):
        'Connect to a serial port'
        if self.connected:
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
        print("Starting input thread...")
        self.input_thread = SerialInputThread(self.msg)
        self.input_thread.start()
        self.connected = True

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
        'Reset a switch given a node id: reset 2'
        nodeid, args = args.partition(' ')[::2]
        if not nodeid:
            print('Missing required nodeid')
            return
        self._reset_command(nodeid)

    def do_hardreset(self, args):
        'Reset a switch and its settings given a node id: hardreset 2'
        nodeid, args = args.partition(' ')[::2]
        if not nodeid:
            print('Missing required nodeid')
            return
        self._reset_command(nodeid, hard=True)

    def do_dump(self, args):
        'Dump switch settings given a node id: dump 2'
        nodeid, args = args.partition(' ')[::2]
        if not nodeid:
            print('Missing required nodeid')
            return
        with self.msg.writer(cmdid=Command.DUMP_SETTINGS) as w:
            w.send_char(int(nodeid))
        msg = self.input_thread.wait_for_ack(1.0)
        if msg:
            print('Tap switch {} to dump settings.'.format(nodeid))
        else:
            print('No ACK received')

    def do_setbyte(self, args):
        'Sets a configuration byte, given nodeid, offset, value: setbyte 2 2 2'
        nodeid, args = args.partition(' ')[::2]
        offset, args = args.partition(' ')[::2]
        value, args = args.partition(' ')[::2]
        if not nodeid or not offset or not value:
            print('Missing required argument')
            return
        with self.msg.writer(cmdid=Command.SET_BYTE) as w:
            w.send_int8(int(nodeid, 0))
            w.send_int8(int(offset, 0))
            w.send_int8(int(value, 0))
        msg = self.input_thread.wait_for_ack(1.0)
        if msg:
            print('Tap switch {} to set configuration.'.format(nodeid))
            n = msg.read_int8()
            o = msg.read_int8()
            v = msg.read_int8()
            print('nodeid:{} offset:{:#04x} value:{:#04x}'.format(n,o,v))
        else:
            print('No ACK received')

    def do_geti2c(self, args):
        '''Gets an I2C register value, given nodeid, address, register:
           geti2c 2 0x5A 0x20'''
        nodeid, args = args.partition(' ')[::2]
        address, args = args.partition(' ')[::2]
        register, args = args.partition(' ')[::2]
        if not nodeid or not address or not register:
            print('Missing required argument')
            return
        with self.msg.writer(cmdid=Command.GET_I2C) as w:
            w.send_int8(int(nodeid, 0))
            w.send_int8(int(address, 0))
            w.send_int8(int(register, 0))
        print('Tap switch {} to get register.'.format(nodeid))
        msg = self.input_thread.wait_for_ack(10.0)
        if msg:
            n = msg.read_int8()
            a = msg.read_int8()
            r = msg.read_int8()
            v = msg.read_int8()
            print('nodeid:{} address:{:#04x} register:{:#04x} value:{:#04x}'.format(
                n,a,r,v))
        else:
            print('No ACK received')

    def do_seti2c(self, args):
        '''Sets an I2C register value, given nodeid, address, register, value:
           geti2c 2 0x5A 0x20 0x12'''
        nodeid, args = args.partition(' ')[::2]
        address, args = args.partition(' ')[::2]
        register, args = args.partition(' ')[::2]
        value, args = args.partition(' ')[::2]
        if not nodeid or not address or not register or not value:
            print('Missing required argument')
            return
        with self.msg.writer(cmdid=Command.SET_I2C) as w:
            w.send_int8(int(nodeid, 0))
            w.send_int8(int(address, 0))
            w.send_int8(int(register, 0))
            w.send_int8(int(value, 0))
        msg = self.input_thread.wait_for_ack(10.0)
        if msg:
            print('Tap switch {} to set register.'.format(nodeid))
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
