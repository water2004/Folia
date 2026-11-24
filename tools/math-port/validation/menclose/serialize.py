"""Serialize exact geometry for independent comparator corruption probes."""
from fractions import Fraction

from ..common import require
from .geometry import source_geometry


def decimal_text(value):
    value = Fraction(value)
    numerator, denominator, twos, fives = value.numerator, value.denominator, 0, 0
    while denominator % 2 == 0:
        denominator //= 2
        twos += 1
    while denominator % 5 == 0:
        denominator //= 5
        fives += 1
    require(denominator == 1, 'All test geometry should have finite base-ten expansions')
    scale = max(twos, fives)
    numerator *= 2 ** (scale - twos) * 5 ** (scale - fives)
    digits = str(abs(numerator)).rjust(scale + 1, '0')
    result = (digits[:-scale] + '.' + digits[-scale:].rstrip('0')).rstrip('.') if scale else digits
    return ('-' if numerator < 0 else '') + result


def scaled(value):
    return decimal_text(Fraction(value) * 1000)


def serialize_path(segments):
    result = []
    for command, *values in segments:
        if command == 'Z':
            result.append('Z')
        elif command in ['M', 'L']:
            result.append(command + ' ' + scaled(values[0]) + ' ' + scaled(values[1]))
        elif command == 'A':
            result.append(' '.join(['A', scaled(values[0]), scaled(values[1]), decimal_text(values[2]),
                                    str(int(values[3])), str(int(values[4])), scaled(values[5]), scaled(values[6])]))
        else:
            require(False, 'Unexpected Source segment ' + command)
    return ' '.join(result)


def serialize_transform(operations):
    return ' '.join('translate(' + scaled(op[1]) + ',' + scaled(op[2]) + ')' if op[0] == 'T' else
                    'rotate(' + decimal_text(op[1]) + ',' + scaled(op[2]) + ',' + scaled(op[3]) + ')' for op in operations)


def serialize_shape(shape):
    attrs = {'d': serialize_path(shape['segments'])}
    transform = serialize_transform(shape['transform']) if shape['transform'] else None
    paint = shape['paint']
    for name in ['fill', 'stroke']:
        if paint[name] != 'inherited':
            attrs[name] = paint[name]
    if paint['stroke_width'] != '0/1':
        attrs['stroke-width'] = scaled(paint['stroke_width'])
    if paint['opacity'] != '1/1':
        attrs['opacity'] = decimal_text(paint['opacity'])
    if paint['fill_rule'] != 'nonzero':
        attrs['fill-rule'] = paint['fill_rule']
    for name in ['fill_opacity', 'stroke_opacity']:
        if paint[name] is not None:
            attrs[name.replace('_', '-')] = decimal_text(paint[name])
    for name, attr in [('line_cap', 'stroke-linecap'), ('line_join', 'stroke-linejoin')]:
        if paint[name] is not None:
            attrs[attr] = paint[name]
    markup = '<path ' + ' '.join(f'{name}="{value}"' for name, value in attrs.items()) + ' />'
    return '<g data-mml-node="mrow"' + (f' transform="{transform}"' if transform else '') + '>' + markup + '</g>'


def render(row):
    inner = '<g data-mml-node="mrow">' + ''.join(map(serialize_shape, source_geometry(row['source_svg']))) + '</g>'
    return ('<svg style="vertical-align: -1.25ex;" xmlns="http://www.w3.org/2000/svg" width="7.5ex" height="3.75ex" '
            'role="img" focusable="false" viewBox="0 -1000 3000 1500">'
            '<g stroke="currentColor" fill="currentColor" stroke-width="0" transform="scale(1,-1)">' + inner + '</g></svg>')


def dump(rows):
    return '\n'.join(row['case'] + '\t' + row['notation'] + '\t' + render(row) for row in rows) + '\n'
