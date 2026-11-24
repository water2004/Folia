"""Source glyph tuples, visible SMP remaps and UTF-16 resource identities."""
import json
import math

from ..common import MISSING, field, require, truthy
from .outline import utf16_units


def integer(value):
    return isinstance(value, (float, int)) and not isinstance(value, bool) and math.isfinite(value) and value == int(value)


def slot(value, index):
    return value[index] if isinstance(value, list) and index < len(value) else MISSING


def or_default(value, default):
    return value if truthy(value) else default


def coalesce(value, default):
    return default if value is None or value is MISSING else value


def number(value):
    if isinstance(value, str):
        text = value.strip()
        if not text:
            return 0.0
        if text.lower().startswith(('0x', '0o', '0b')):
            return float(int(text, 0))
    return float(value)


def text_number(value):
    return str(int(value)) if integer(value) else str(value)


def content_string(value):
    if value is MISSING:
        return None
    if isinstance(value, str):
        return value
    if isinstance(value, list):
        return ','.join('' if item is None else text_number(item) if isinstance(item, (int, float)) else str(item) for item in value)
    require(False, 'Unsupported Source composition recipe: ' + repr(value))


def composition(value):
    if value is MISSING:
        return []
    result = []
    for item in value if isinstance(value, list) else [value]:
        if isinstance(item, str):
            units, index = utf16_units(item), 0
            while index < len(units):
                cp = units[index]
                index += 1
                if 0xD800 <= cp <= 0xDBFF and index < len(units) and 0xDC00 <= units[index] <= 0xDFFF:
                    cp = 0x10000 + ((cp - 0xD800) << 10) + units[index] - 0xDC00
                    index += 1
                result.append(cp)
        elif integer(item) and 0 <= item <= 0x10FFFF:
            result.append(item)
        else:
            require(False, 'Unsupported Source composition element: ' + repr(item))
    return result


def source_glyph(row):
    options = or_default(slot(row, 3), {})
    recipe = field(options, 'c')
    return {'metrics': [coalesce(slot(row, index), 0) for index in range(3)] +
                       [coalesce(field(options, name), 0) for name in ['ic', 'sk', 'dx', 'oc']],
            'path_defined': isinstance(options, dict) and 'p' in options,
            'path_data': options['p'] if isinstance(field(options, 'p'), str) else '',
            'content_data': content_string(recipe), 'composition': composition(recipe)}


def effective_remaps(variant, visible):
    return {cp: {'variant': variant, 'codepoint': field(slot(row, 3), 'smp')}
            for cp, row in (visible or {}).items() if integer(field(slot(row, 3), 'smp'))}


def resource_key(value):
    # Match JSON.stringify([variant, codepoint]), including numeric spelling.
    return json.dumps([int(item) if integer(item) else item for item in value], ensure_ascii=False, separators=(',', ':'))
