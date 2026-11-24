"""Strict local RGBA comparison with the original spatial/rounding budgets."""

from __future__ import annotations

import math

import numpy as np

from .corpus import require


RASTER_SCALE = 2
CHANNEL_TOLERANCE = 2


def compare_pixels(*, actual, expected, scale, tolerance, colorTolerance):
    width, height = max(actual["width"], expected["width"]), max(actual["height"], expected["height"])
    radius = tolerance * scale
    require(width > 0 and height > 0 and scale > 0 and radius >= 0, "Invalid raster dimensions/tolerance")
    offsets = [(x, y) for y in range(-math.ceil(radius), math.ceil(radius) + 1)
               for x in range(-math.ceil(radius), math.ceil(radius) + 1) if x * x + y * y <= radius * radius]
    offsets.sort(key=lambda p: p[0] * p[0] + p[1] * p[1])

    def compose(image):
        pixels = (np.frombuffer(image["data"], dtype=np.uint8) if isinstance(image["data"], bytes)
                  else np.asarray(image["data"], dtype=np.uint8)).reshape(image["height"], image["width"], 4)
        output = np.zeros((height, width, 4), dtype=np.int16)
        output[:, :, :3] = 255
        alpha = pixels[:, :, 3:4].astype(float) / 255
        # JS Math.round uses floor(x + .5) for these nonnegative values.
        output[:image["height"], :image["width"], :3] = np.floor(pixels[:, :, :3] * alpha + 255 * (1 - alpha) + .5)
        output[:image["height"], :image["width"], 3] = pixels[:, :, 3]
        return output

    a, e, examples = compose(actual), compose(expected), []

    def check(source, target, direction):
        matched = np.max(np.abs(source - target), axis=2) <= colorTolerance
        matched |= source[:, :, 3] == 0
        for dx, dy in offsets:
            if matched.all():
                break
            x0, x1, y0, y1 = max(0, -dx), min(width, width - dx), max(0, -dy), min(height, height - dy)
            if x1 <= x0 or y1 <= y0:
                continue
            matched[y0:y1, x0:x1] |= np.max(np.abs(
                source[y0:y1, x0:x1] - target[y0 + dy:y1 + dy, x0 + dx:x1 + dx]), axis=2) <= colorTolerance
        failed = np.argwhere(~matched)
        for y, x in failed[:max(0, 32 - len(examples))]:
            examples.append({"direction": direction, "x": int(x) / scale, "y": int(y) / scale})
        return len(failed)

    missing, extra = check(e, a, "missing"), check(a, e, "extra")

    def integral(data, background):
        ink = (255 - np.min(data[:, :, :3], axis=2) if background == "white" else
               np.max(data[:, :, :3], axis=2) - 255 + data[:, :, 3]).astype(float) / 255
        return np.pad(ink.cumsum(axis=1).cumsum(axis=0), ((1, 0), (1, 0)))

    def area(sums, x0, y0, x1, y1):
        x0, y0, x1, y1 = max(0, x0), max(0, y0), min(width, x1), min(height, y1)
        if x1 <= x0 or y1 <= y0:
            return 0
        return sums[y1, x1] - sums[y0, x1] - sums[y1, x0] + sums[y0, x0]

    failed_tiles, tile, halo = 0, 16 * scale, math.ceil(radius)
    require(int(tile) == tile, "Raster scale must give integral tile dimensions")
    tile = int(tile)
    for background in ("white", "black"):
        a_ink, e_ink = integral(a, background), integral(e, background)
        for y in range(0, height, tile):
            for x in range(0, width, tile):
                inner, outer = (x + halo, y + halo, x + tile - halo, y + tile - halo), (x - halo, y - halo, x + tile + halo, y + tile + halo)
                rounding = tile * tile * colorTolerance / 255
                if area(e_ink, *inner) > area(a_ink, *outer) + rounding or area(a_ink, *inner) > area(e_ink, *outer) + rounding:
                    failed_tiles += 1
                    if len(examples) < 32:
                        examples.append({"direction": "local-ink-" + background, "x": x / scale, "y": y / scale})
    return {"ok": missing == extra == failed_tiles == 0, "missingPixels": missing, "extraPixels": extra,
            "failedTiles": failed_tiles, "examples": examples, "width": width, "height": height}
