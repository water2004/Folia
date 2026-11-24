from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from validation.common import ROOT
from validation.menclose.selftest import run_selftests


class MencloseComparatorTests(unittest.TestCase):
    def test_all_original_geometry_and_cli_corruption_probes(self):
        with tempfile.TemporaryDirectory(prefix='folia-menclose-') as directory:
            report = run_selftests(ROOT, directory)
        self.assertEqual(report['equivalent-baseline'], 'accepted expected baseline')
        self.assertEqual(report['positional-cli'], 'accepted positional baseline')
        self.assertEqual(sum(value == 'rejected as expected' for value in report.values()), 13)


if __name__ == '__main__':
    unittest.main()
