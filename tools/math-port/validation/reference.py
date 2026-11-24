"""Structured math reference comparison with the original narrow tolerances."""
import math
import re

from .common import js_type, same_scalar, sorted_keys

_GEOMETRY = re.compile(
    r'^\$\.layout_runs\[\d+\]\.wrappers\[\d+\]\.(bbox|outer_bbox|line_bboxes\[\d+\])\.'
    r'(w|h|d|scale|rscale|L|R|ic|oc|sk|dx)$|^\$\.placements\[\d+\]\.(x|y|dx|rscale)$')


def geometry(path):
    return _GEOMETRY.fullmatch(path) is not None


def compare(expected, actual, *, limit=20):
    differences = []

    def fail(path, reason):
        if len(differences) < limit:
            differences.append({'path': path, 'reason': reason})

    def walk(left, right, path):
        if len(differences) >= limit:
            return
        if js_type(left) == js_type(right) == 'number' and geometry(path):
            if (not math.isfinite(left) or not math.isfinite(right)
                    or abs(left - right) > 1e-10 + 1e-10 * max(abs(left), abs(right))):
                fail(path, 'geometry differs beyond absolute 1e-10 + relative 1e-10')
            return
        if left is None or right is None or not isinstance(left, (dict, list)) or not isinstance(right, (dict, list)):
            if not same_scalar(left, right):
                fail(path, 'exact value differs')
            return
        if isinstance(left, list) != isinstance(right, list):
            fail(path, 'array/object kind differs')
            return
        if isinstance(left, list):
            if len(left) != len(right):
                fail(path, 'array length differs')
            for index, (a, b) in enumerate(zip(left, right)):
                walk(a, b, f'{path}[{index}]')
        else:
            if sorted_keys(left) != sorted_keys(right):
                fail(path, 'own property names differ')
            for key in sorted_keys(left):
                if key in right:
                    walk(left[key], right[key], path + '.' + key)

    walk(expected, actual, '$')
    return differences
