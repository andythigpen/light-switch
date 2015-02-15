import binascii
import cmd
from cmdmessenger import CmdMessengerHandler
from hub import LightSwitchHub, Command, LightSwitchHubTimeout


class Handler(CmdMessengerHandler):
    @CmdMessengerHandler.handler(cmdid=Command.MSG)
    def handle_debug(self, msg):
        print("hub: {}".format(msg.read_str()))

    @CmdMessengerHandler.handler(cmdid=Command.TOUCH_EVENT)
    def handle_touch_event(self, msg):
        nodeid = msg.read_int8()
        gesture = Command.GESTURES[msg.read_int8()]
        electrode = Command.ELECTRODES[msg.read_int8()]
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


class ControllerShell(cmd.Cmd):
    intro = 'switch controller shell.  Type help or ? to list commands.\n'
    prompt = 'hub> '
    connected = False

    def emptyline(self):
        return

    def do_connect(self, args):
        'Connect to a serial port'
        if hasattr(self, 'hub') and self.hub.connected:
            print('Already connected to {}'.format(self.hub.connection.name))
            return
        port, args = args.partition(' ')[::2]
        baud, args = args.partition(' ')[::2]
        timeout, args = args.partition(' ')[::2]
        port = port or '/dev/ttyUSB0'
        baud = int(baud or 115200)
        timeout = float(timeout or 1.0)

        print("Connecting to {} at {} baud, timeout {}".format(port, baud, timeout))
        self.hub = LightSwitchHub(handlers=Handler())
        self.hub.connect(port, baud, timeout)
        self.nodeid = None

    def do_disconnect(self, args):
        'Disconnect from serial port'
        if not hasattr(self, 'hub') or not self.hub.connected:
            print('Not connected')
            return
        print("Disconnecting...")
        self.hub.disconnect()

    def do_node(self, args):
        'Sets the current nodeid: node 2'
        try:
            self.nodeid = int(args)
            self.prompt = 'node:{}> '.format(self.nodeid)
        except:
            print("Invalid node id: {}".format(args))

    def do_reset(self, args):
        'Reset the current switch'
        if not self.nodeid:
            print('Must select a node first')
            return
        try:
            msg = self.hub.reset(self.nodeid)
            print('Tap switch {} to reset.'.format(self.nodeid))
        except LightSwitchHubTimeout:
            print('No ACK received')

    def do_hardreset(self, args):
        'Reset the current switch and its settings'
        if not self.nodeid:
            print('Must select a node first')
            return
        try:
            msg = self.hub.reset(self.nodeid, hard=True)
            print('Tap switch {} to reset.'.format(self.nodeid))
        except LightSwitchHubTimeout:
            print('No ACK received')

    def do_status(self, args):
        'Request status from switch'
        if not self.nodeid:
            print('Must select a node first')
            return
        try:
            msg = self.hub.status(self.nodeid)
            print('Tap switch {} to get status.'.format(self.nodeid))
        except LightSwitchHubTimeout:
            print('No ACK received')

    def do_dump(self, args):
        'Dump switch settings'
        if not self.nodeid:
            print('Must select a node first')
            return
        try:
            msg = self.hub.dump(self.nodeid)
            print('Tap switch {} to dump settings.'.format(self.nodeid))
        except LightSwitchHubTimeout:
            print('No ACK received')

    def do_setbyte(self, args):
        'Sets a configuration byte, given offset, value: setbyte 2 2'
        offset, args = args.partition(' ')[::2]
        value, args = args.partition(' ')[::2]
        if not self.nodeid or not offset or not value:
            print('Missing required argument')
            return
        try:
            msg = self.hub.dump(self.nodeid, offset, value)
            print('Tap switch {} to set configuration.'.format(self.nodeid))
            n = msg.read_int8()
            o = msg.read_int8()
            v = msg.read_int8()
            print('nodeid:{} offset:{:#04x} value:{:#04x}'.format(n,o,v))
        except LightSwitchHubTimeout:
            print('No ACK received')

    def do_geti2c(self, args):
        '''Gets an I2C register value, given address, register: geti2c 0x5A 0x20'''
        address, args = args.partition(' ')[::2]
        register, args = args.partition(' ')[::2]
        if not self.nodeid or not address or not register:
            print('Missing required argument')
            return
        try:
            msg = self.hub.geti2c(self.nodeid, address, register)
            print('Tap switch {} to get register.'.format(self.nodeid))
            n = msg.read_int8()
            a = msg.read_int8()
            r = msg.read_int8()
            print('nodeid:{} address:{:#04x} register:{:#04x}'.format(n,a,r))
        except LightSwitchHubTimeout:
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
        try:
            msg = self.hub.seti2c(self.nodeid, address, register, value)
            print('Tap switch {} to set register.'.format(self.nodeid))
            n = msg.read_int8()
            a = msg.read_int8()
            r = msg.read_int8()
            v = msg.read_int8()
            print('nodeid:{} address:{:#04x} register:{:#04x} value:{:#04x}'.format(
                n,a,r,v))
        except LightSwitchHubTimeout:
            print('No ACK received')

    def do_exit(self, args):
        'Exits the shell'
        if hasattr(self, 'hub') and self.hub.connected:
            self.hub.disconnect()
        return True

def main():
    c = ControllerShell()
    try:
        c.cmdloop()
    except KeyboardInterrupt:
        print()
    finally:
        if hasattr(c, 'hub') and c.hub.connected:
            c.hub.disconnect()

if __name__ == '__main__':
    main()
