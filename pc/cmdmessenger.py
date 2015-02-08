import struct
import re

class CmdMessengerWriter(object):
    def __init__(self, stream, field_sep, cmd_sep, escape_sep, cmdid):
        self.stream = stream
        self.field_sep = field_sep
        self.cmd_sep = cmd_sep
        self.escape_sep = escape_sep
        self.cmdid = int(cmdid)

    def __enter__(self):
        self.start()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        if not exc_type:
            self.stop()

    def start(self):
        self.stream.write(str(self.cmdid).encode('utf-8'))

    def stop(self):
        self.stream.write(self.cmd_sep)

    def _escape(self, arg):
        it = iter(arg)
        out = bytearray()
        special = [ord(self.field_sep), ord(self.cmd_sep), ord(self.escape_sep)]
        for b in it:
            if b in special:
                out.append(ord(self.escape_sep))
            out.append(b)
        return out

    def _send_field(self, arg):
        self.stream.write(self.field_sep)
        self.stream.write(arg)

    def _send_unescaped_field(self, arg):
        self._send_field(str(arg).encode('utf-8'))

    def send_bool(self, arg):
        self.send_int16(arg)

    def send_int8(self, arg):
        self._send_unescaped_field(int(arg))

    def send_int16(self, arg):
        self._send_unescaped_field(int(arg))

    def send_int32(self, arg):
        self._send_unescaped_field(int(arg))

    def send_char(self, arg):
        self._send_field(bytes([ord(chr(arg))]))

    def send_float(self, arg):
        self._send_unescaped_field(float(arg))

    def send_double(self, arg):
        self._send_unescaped_field(float(arg))

    def send_str(self, arg):
        self._send_field(self._escape(str.encode(arg)))

    def send_bytes(self, arg):
        self._send_field(self._escape(arg))


class CmdMessengerReader(object):
    def __init__(self, cmd, field_sep, cmd_sep, escape_sep):
        self.cmd = cmd.copy()
        self.field_sep = field_sep
        self.cmd_sep = cmd_sep
        self.escape_sep = escape_sep
        self.cursor = iter(self.cmd)
        self.cmdid = self.read_int16()

    def _next(self, escaped=False):
        arg = bytearray()
        for b in self.cursor:
            if b == ord(self.field_sep) or b == ord(self.cmd_sep):
                return arg
            if escaped and b == ord(self.escape_sep):
                b = next(self.cursor)
            arg.append(b)
        return arg

    def read_bool(self):
        return (self.read_int16() != 0)

    def read_int8(self):
        return int(self._next())

    def read_int16(self):
        return int(self._next())

    def read_int32(self):
        return int(self._next())

    def read_char(self):
        return ord(self._next().decode())

    def read_float(self):
        return float(self._next())

    def read_double(self):
        return float(self._next())

    def read_str(self):
        return self._next(escaped=True).decode('utf-8')

    def read_bytes(self):
        return self._next(escaped=True)


class CmdMessenger(object):
    cmd_callbacks = {}

    def __init__(self, stream, field=b',', cmd=b';', escape=b'/'):
        self.stream = stream
        self.field_sep = field
        self.cmd_sep = cmd
        self.escape_sep = escape
        self.input_buffer = bytearray()
        self.message_available = False

    @classmethod
    def callback(cls, cmdid):
        def decorator(f):
            cls.cmd_callbacks[cmdid] = f
            return f
        return decorator

    def writer(self, cmdid):
        return CmdMessengerWriter(self.stream, self.field_sep, self.cmd_sep,
                                  self.escape_sep, cmdid)

    def read(self):
        while True:
            c = self.stream.read(1)
            if not c:
                break
            if len(self.input_buffer):
                prev = self.input_buffer[-1]
            else:
                prev = None
            self.input_buffer.append(ord(c))
            if ord(c) == ord(self.cmd_sep) and prev != ord(self.escape_sep):
                break
        # there may be more than one message in the buffer, handle them all
        cmd = bytearray()
        for b in self.input_buffer:
            if b == ord(self.cmd_sep) and cmd[-1] != ord(self.escape_sep):
                self._handle_msg(cmd)
                cmd.clear()
            else:
                cmd.append(b)
        self.input_buffer = cmd

    def _handle_msg(self, cmd):
        reader = CmdMessengerReader(cmd, self.field_sep, self.cmd_sep,
                                    self.escape_sep)
        if reader.cmdid in self.cmd_callbacks:
            self.cmd_callbacks[reader.cmdid](reader)
        else:
            print("callback not found for: {}".format(reader.cmdid))
