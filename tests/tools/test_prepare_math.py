"""Source preparation preserves the exact pins on hosts with different EOLs."""
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("prepare_math_source", ROOT / "tools/math-port/prepare-source.py")
prepare = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(prepare)


class MathSourcePreparationTests(unittest.TestCase):
    def test_only_hash_matching_line_endings_are_accepted(self):
        expected = b"source\r\nbytes\r\n"
        digest = hashlib.sha256(expected).hexdigest()
        self.assertEqual(prepare.pinned_bytes(expected, digest, "fixture.ts"), expected)
        self.assertEqual(prepare.pinned_bytes(expected.replace(b'\r\n', b'\n'), digest, "fixture.ts"), expected)
        with self.assertRaisesRegex(ValueError, "source mismatch"):
            prepare.pinned_bytes(b"different source\n", digest, "fixture.ts")

    def test_restoration_is_local_and_checks_all_inputs_before_writing(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            package = root / "tools/mathjax-bundle/node_modules/@mathjax/src/ts/util"
            destination = root / ".reference/MathJax-src/ts/util/Entities.ts"
            package.mkdir(parents=True)
            raw = b"locked source\r\n"
            (package / "Entities.ts").write_bytes(raw.replace(b'\r\n', b'\n'))
            lock_path = root / "docs/math-port/evidence/entities-source-lock.json"
            lock_path.parent.mkdir(parents=True)
            entries = {"Entities.ts": hashlib.sha256(raw).hexdigest()}
            lock_path.write_text(json.dumps({"source": {"ts": dict(entries, missing="0" * 64)}}))
            with self.assertRaisesRegex(ValueError, "Missing MathJax package"):
                prepare.restore(root)
            self.assertFalse(destination.exists())
            lock_path.write_text(json.dumps({"source": {"ts": entries}}))
            self.assertEqual(prepare.restore(root), 1)
            self.assertEqual(destination.read_bytes(), raw)
            self.assertEqual(prepare.restore(root, check=True), 1)
            destination.write_bytes(b"corrupt")
            with self.assertRaisesRegex(ValueError, "source mismatch"):
                prepare.restore(root)
            self.assertEqual(destination.read_bytes(), b"corrupt")


if __name__ == "__main__":
    unittest.main()
