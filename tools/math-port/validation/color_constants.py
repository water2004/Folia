"""Compare all native color constants with the existing independent Source table."""

import re

from .common import ROOT, equal, read


SOURCE = ROOT / "tests/math/fixtures/tex-source/color-source-expected.json"
NATIVE = ROOT / "src/math/input/tex/color/constants.ixx"


def check(native_text=None, expected=None):
    expected = read(SOURCE)["constants"] if expected is None else expected
    native_text = NATIVE.read_text(encoding="utf-8") if native_text is None else native_text
    equal(68, len(expected), "Pinned Source color count")
    equal(68, len({name for name, _ in expected}), "Pinned Source unique colors")
    # Preserve the original exact ordered native table observation contract.
    rows = [list(row) for row in re.findall(r'\{"([A-Za-z]+)","(#[A-F0-9]+)"\}', native_text)]
    equal(expected, rows, "Native immutable color table order and values")
    return {"colors": len(rows), "source": str(SOURCE.relative_to(ROOT)), "native": str(NATIVE.relative_to(ROOT))}
