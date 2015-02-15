from cmdmessenger import CmdMessenger
import io
import unittest

class TestCmdMessenger(unittest.TestCase):

    def setUp(self):
        self.stream = io.BytesIO()
        self.cmd = CmdMessenger(self.stream)

    def test_bool(self):
        with self.cmd.writer(1) as w:
            w.send_bool(True)
            w.send_bool(False)
        self.stream.seek(0)
        def verify(cmd):
            self.assertEqual(cmd.read_bool(), True)
            self.assertEqual(cmd.read_bool(), False)
        self.cmd.register(1, verify)
        self.cmd.read()

    def test_int(self):
        values = [1, ord(','), ord(';'), ord('\\')]
        with self.cmd.writer(1) as w:
            for val in values:
                w.send_int16(val)
        self.stream.seek(0)
        def verify(cmd):
            for val in values:
                self.assertEqual(cmd.read_int16(), val,
                        msg="val: {}".format(val))
        self.cmd.register(1, verify)
        self.cmd.read()

    def test_char(self):
        with self.cmd.writer(1) as w:
            w.send_char(10)
        self.stream.seek(0)
        def verify(cmd):
            self.assertEqual(cmd.read_char(), 10)
        self.cmd.register(1, verify)
        self.cmd.read()

    def test_float(self):
        with self.cmd.writer(1) as w:
            w.send_float(1.23456)
        self.stream.seek(0)
        def verify(cmd):
            self.assertEqual(cmd.read_float(), 1.23456)
        self.cmd.register(1, verify)
        self.cmd.read()

    def test_str(self):
        with self.cmd.writer(1) as w:
            w.send_str('this is a test string')
        self.stream.seek(0)
        def verify(cmd):
            self.assertEqual(cmd.read_str(), 'this is a test string')
        self.cmd.register(1, verify)
        self.cmd.read()

    def test_bytes(self):
        with self.cmd.writer(1) as w:
            w.send_bytes(b'this is a byte string')
        self.stream.seek(0)
        def verify(cmd):
            self.assertEqual(cmd.read_bytes(), b'this is a byte string')
        self.cmd.register(1, verify)
        self.cmd.read()

    def test_escape_chars(self):
        with self.cmd.writer(1) as w:
            w.send_str('this, string needs\\ to be; escaped')
        self.stream.seek(0)
        def verify(cmd):
            self.assertEqual(cmd.read_str(),
                             'this, string needs\\ to be; escaped')
        self.cmd.register(1, verify)
        self.cmd.read()

    def test_mixed_fields(self):
        with self.cmd.writer(1) as w:
            w.send_int16(123)
            w.send_str('string,')
            w.send_int32(321)
        self.stream.seek(0)
        def verify(cmd):
            self.assertEqual(cmd.read_int16(), 123)
            self.assertEqual(cmd.read_str(), 'string,')
            self.assertEqual(cmd.read_int32(), 321)
        self.cmd.register(1, verify)
        self.cmd.read()

    def test_multiple_commands(self):
        with self.cmd.writer(1) as w:
            w.send_int16(123)
        with self.cmd.writer(1) as w:
            w.send_int16(321)
        self.stream.seek(0)
        def verify(cmd):
            if verify.idx == 0:
                self.assertEqual(cmd.read_int16(), 123)
            else:
                self.assertEqual(cmd.read_int16(), 321)
            verify.idx += 1
        verify.idx = 0
        self.cmd.register(1, verify)
        self.cmd.read()

if __name__ == '__main__':
    unittest.main()
