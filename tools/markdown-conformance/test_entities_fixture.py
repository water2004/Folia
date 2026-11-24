"""Every pinned entity survives runtime transport, including multi-scalar values."""
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest

import generate_pinned_gfm_entities as entities


class EntityFixtureTests(unittest.TestCase):
    def test_complete_runtime_table_matches_independent_upstream_utf8_arrays(self):
        raw = entities.BUNDLED.read_bytes()
        lock = json.loads(Path(entities.__file__).with_name("gfm-sources.lock.json").read_bytes())
        pin = next(row for row in lock["sources"] if row["name"] == "entities.inc")
        self.assertEqual(hashlib.sha256(raw).hexdigest(), pin["sha256"])
        expected = []
        for name, numbers in re.findall(r'\{\(unsigned char\*\)"([^"]+)", \{([^}]+)\}', raw.decode("utf-8")):
            payload = bytes(int(number.strip()) for number in numbers.split(',') if number.strip())
            self.assertEqual(payload[-1], 0)
            expected.append(['&' + name + ';', payload[:-1].decode('utf-8')])
        observed = [list(map(lambda cell: json.loads('"' + cell + '"'), line.split('\t')))
                    for line in entities.OUTPUT.read_bytes().decode('utf-8').split('\n')
                    if line and not line.startswith('#')]
        self.assertEqual(len(expected), 2125)
        self.assertEqual(observed, expected)
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / 'entities.tsv'
            subprocess.run([sys.executable, entities.__file__, '--source', str(entities.BUNDLED),
                            '--output', str(output)], check=True, capture_output=True)
            self.assertEqual(output.read_bytes(), entities.OUTPUT.read_bytes())

    def test_modified_source_is_rejected_before_output_is_written(self):
        with tempfile.TemporaryDirectory() as directory:
            source, output = Path(directory) / 'bad.inc', Path(directory) / 'entities.tsv'
            source.write_bytes(b'corrupted')
            result = subprocess.run([sys.executable, entities.__file__, '--source', str(source),
                                     '--output', str(output)], capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('hash mismatch', result.stderr)
            self.assertFalse(output.exists())


if __name__ == '__main__':
    unittest.main()
