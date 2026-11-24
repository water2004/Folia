"""Compare the 96 captured browser/native font observations without a JS runtime.

The retained ceilings describe this corpus, not universal DirectWrite/Chromium
equivalence. Both input order and the actual captured measurements are checked.
"""

import math

from .corpus import require


def compare_text_probe(reference, native):
    expected, rows = reference.rstrip().splitlines(), native.rstrip().splitlines()
    require(len(rows) == len(expected) == 96, "Text probe requires all 96 browser/native observations")
    summary = {"cases": len(rows), "advanceCeiling": .0625, "boundsCeiling": 2.5,
               "maxAdvanceError": 0, "maxBoundsError": 0}
    for index, row in enumerate(rows):
        fields = row.split("\t")
        require(len(fields) == 8, f"Text observation {index}: expected eight fields")
        require("\t".join(fields[:5]) == expected[index], f"Text observation {index}: input/order mismatch")
        values = [float(value) for value in fields[3:]]
        require(all(math.isfinite(value) and value >= 0 for value in values),
                f"Text observation {index}: invalid measurement")
        browser_bounds, browser_advance, advance, ink, bounds = values
        advance_error, bounds_error = abs(browser_advance - advance), abs(browser_bounds - bounds)
        summary["maxAdvanceError"] = max(summary["maxAdvanceError"], advance_error)
        summary["maxBoundsError"] = max(summary["maxBoundsError"], bounds_error)
        require(advance_error <= summary["advanceCeiling"], f"Advance regression at row {index}: {advance_error}")
        require(bounds_error <= summary["boundsCeiling"], f"Bounds regression at row {index}: {bounds_error}")
        require(bounds >= advance, f"Text observation {index}: bounds smaller than advance")
    return summary
