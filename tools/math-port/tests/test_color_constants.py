"""Retain the former JS native color-table gate, including every ordered row."""

from copy import deepcopy
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from validation.color_constants import NATIVE, SOURCE, check
from validation.common import ComparisonError, read


class ColorConstantChecks(unittest.TestCase):
    def test_all_68_native_constants_match_independent_source(self):
        self.assertEqual(check()["colors"], 68)

    def test_any_changed_constant_fails(self):
        text = NATIVE.read_text(encoding="utf-8")
        for name, color in read(SOURCE)["constants"]:
            with self.subTest(name=name):
                changed = text.replace('{"' + name + '","' + color + '"}', '{"' + name + '","#000001"}')
                with self.assertRaises(ComparisonError):
                    check(changed)

    def test_missing_extra_and_reordered_rows_fail(self):
        rows = read(SOURCE)["constants"]
        for changed in (rows[:-1], rows + rows[:1], [rows[1], rows[0], *rows[2:]]):
            text = ",\n".join('{"' + name + '","' + value + '"}' for name, value in changed)
            with self.assertRaises(ComparisonError):
                check(text)

    def test_damaged_source_table_is_rejected(self):
        expected = deepcopy(read(SOURCE)["constants"])
        expected[1] = expected[0]
        with self.assertRaises(ComparisonError):
            check(expected=expected)


if __name__ == "__main__":
    unittest.main()
