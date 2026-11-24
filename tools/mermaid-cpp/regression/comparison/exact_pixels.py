"""Exact transparent RGBA comparison used by Canvas and Math fragment gates."""

import numpy as np

from .corpus import require


def exact_pixels(actual, expected):
    require(len(actual) == len(expected) and len(actual) % 4 == 0, "RGBA byte count mismatch")
    a, e = (np.frombuffer(data, dtype=np.uint8).astype(np.int16).reshape(-1, 4)
            for data in (actual, expected))
    delta = np.abs(a - e)
    return {"differentPixels": int(np.count_nonzero(np.any(delta, axis=1))),
            "maximumChannelDifference": int(delta.max(initial=0)),
            "absoluteChannelDifference": int(delta.sum()), "totalPixels": len(a)}
