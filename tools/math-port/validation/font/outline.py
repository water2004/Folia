"""Observe every SVG outline segment and the original exact parse failure offset."""
import math
import re


def utf16_units(text):
    data = text.encode('utf-16-le', errors='surrogatepass')
    return [data[index] | data[index + 1] << 8 for index in range(0, len(data), 2)]


def outline_from_fragment(fragment):
    # JS indexes UTF-16 code units; preserve even malformed/non-ASCII offsets.
    text = ''.join(map(chr, utf16_units('M' + fragment + 'Z'))) if fragment else ''
    segments, index, active, x, y, sx, sy = [], 0, '', 0.0, 0.0, 0.0, 0.0
    is_letter = lambda char: bool(re.fullmatch('[A-Za-z]', char))
    is_digit = lambda char: bool(re.fullmatch('[0-9]', char))

    def skip():
        nonlocal index
        while index < len(text) and text[index] in '\t\n\r ,':
            index += 1

    def failure(reason, offset, command):
        return {'segments': segments, 'error': {'byte_offset': offset, 'command': command or None, 'reason': reason}}

    def number(command):
        nonlocal index
        skip()
        offset = index
        if index == len(text) or is_letter(text[index]):
            return None, failure('incomplete_segment', index, command)
        begin = index
        if text[index] in '+-':
            index += 1
        digits = 0
        while index < len(text) and is_digit(text[index]):
            index, digits = index + 1, digits + 1
        if index < len(text) and text[index] == '.':
            index += 1
            while index < len(text) and is_digit(text[index]):
                index, digits = index + 1, digits + 1
        if not digits:
            return None, failure('invalid_number', offset, command)
        if index < len(text) and text[index] in 'eE':
            index += 1
            if index < len(text) and text[index] in '+-':
                index += 1
            start = index
            while index < len(text) and is_digit(text[index]):
                index += 1
            if index == start:
                return None, failure('invalid_number', offset, command)
        value = float(text[begin:index])
        return (value, None) if math.isfinite(value) else (None, failure('invalid_number', offset, command))

    def read_point(command, relative, origin):
        a, error = number(command)
        if error:
            return None, error
        b, error = number(command)
        if error:
            return None, error
        return ([origin[0] + a, origin[1] + b] if relative else [a, b]), None

    point = lambda values: [values[0] / 1000, values[1] / 1000]
    while True:
        skip()
        if index == len(text):
            break
        if is_letter(text[index]):
            active, index = text[index], index + 1
        if not active:
            return failure('missing_command', index, None)
        command, relative = active.upper(), active != active.upper()
        if command == 'Z':
            segments.append({'type': 'Z'})
            x, y, active = sx, sy, ''
            continue
        if command not in ['M', 'L', 'C', 'Q']:
            return failure('unsupported_command', index - 1, active)
        origin = [x, y]
        points = []
        for _ in range(1 if command in ['M', 'L'] else 2 if command == 'Q' else 3):
            value, error = read_point(active, relative, origin)
            if error:
                return error
            points.append(value)
        x, y = points[-1]
        if command in ['M', 'L']:
            segments.append({'type': command, 'to': point(points[0])})
            if command == 'M':
                sx, sy, active = x, y, 'l' if relative else 'L'
        elif command == 'Q':
            segments.append({'type': 'Q', 'control': point(points[0]), 'to': point(points[1])})
        else:
            segments.append({'type': 'C', 'control1': point(points[0]), 'control2': point(points[1]), 'to': point(points[2])})
    return {'segments': segments, 'error': None}
