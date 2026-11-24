"""The preservation gate checks duplicate registrations as well as missing names."""
from collections import Counter
import unittest

import check_inventory


class InventoryTests(unittest.TestCase):
    def test_all_original_registrations_remain(self):
        report = check_inventory.check()
        self.assertTrue(report["success"], "\n".join(report["failures"]))

    def test_missing_duplicate_is_detected_but_new_tests_are_allowed(self):
        baseline = [{"name": "original", "count": 2}]
        self.assertEqual(check_inventory.check_names(Counter(original=2, additional=3), baseline), [])
        self.assertEqual(len(check_inventory.check_names(Counter(original=1), baseline)), 1)
        self.assertEqual(len(check_inventory.check_names(Counter(), baseline)), 1)


if __name__ == "__main__":
    unittest.main()
