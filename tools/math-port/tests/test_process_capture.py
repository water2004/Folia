from pathlib import Path
import json
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from validation.processes import execute


class ProcessCaptureTests(unittest.TestCase):
    def test_original_multibyte_corpus_at_every_byte_boundary(self):
        # Preserve every split from process-output.test.cjs, including invisible
        # multiplication, four-byte scalars, CJK and the bytewise observation.
        expected = {'text': 'aµ⁢𝒙中文🙂z', 'nested': {'linebreakmultchar': '⁢'}}
        text = json.dumps(expected, ensure_ascii=False, separators=(',', ':')) + '\n'
        payload = text.encode('utf-8')
        for split in range(len(payload) + 1):
            with self.subTest(split=split):
                script = f'import os; os.write(1,{payload[:split]!r}); os.write(1,{payload[split:]!r})'
                record, stdout, _ = execute(sys.executable, ['-c', script])
                self.assertEqual(record['exitCode'], 0)
                self.assertEqual(stdout, payload)
                self.assertEqual(stdout.decode('utf-8'), text)
                self.assertEqual(json.loads(stdout), expected)
        script = f'import os\nfor byte in {payload!r}: os.write(1,bytes([byte]))'
        record, stdout, _ = execute(sys.executable, ['-c', script])
        self.assertEqual(record['exitCode'], 0)
        self.assertEqual(stdout.decode('utf-8'), text)

    def test_byte_chunks_preserve_utf8_and_stderr(self):
        script = "import os; os.write(1,bytes([0xe4])); os.write(1,bytes([0xb8,0xad,0])); os.write(2,b'error\\x00tail')"
        record, stdout, stderr = execute(sys.executable, ['-c', script])
        self.assertEqual(record['exitCode'], 0)
        self.assertEqual(stdout, '中'.encode() + b'\0')
        self.assertEqual(stderr, b'error\0tail')
        self.assertEqual(stdout.decode('utf-8'), '中\0')

    def test_invalid_utf8_remains_available_as_evidence(self):
        for byte in [0xb5, 0xff]:
            record, stdout, _ = execute(sys.executable, ['-c', f'import os; os.write(1,bytes([{byte}]))'])
            self.assertEqual(record['exitCode'], 0)
            self.assertEqual(stdout, bytes([byte]))
            with self.assertRaises(UnicodeDecodeError):
                stdout.decode('utf-8')

    def test_timeout_and_process_failure_do_not_pass(self):
        record, _, _ = execute(sys.executable, ['-c', 'import time; time.sleep(10)'], timeout=0.05)
        self.assertTrue(record['timedOut'])
        self.assertNotEqual(record['exitCode'], 0)
        record, _, stderr = execute(sys.executable, ['-c', "import sys; sys.stderr.write('failed'); sys.exit(7)"])
        self.assertEqual(record['exitCode'], 7)
        self.assertEqual(stderr, b'failed')


if __name__ == '__main__':
    unittest.main()
