"""Canonical line/rect/ellipse/path geometry with every paint field retained."""
from fractions import Fraction

from ..common import require
from .path import decimal, key, source_path, transform_ops

SOURCE_ATTRIBUTES = {
    'line': ['x1', 'y1', 'x2', 'y2', 'stroke-width'],
    'rect': ['x', 'y', 'width', 'height', 'rx', 'fill', 'stroke-width'],
    'ellipse': ['rx', 'ry', 'cx', 'cy', 'fill', 'stroke-width'],
    'path': ['d', 'style', 'stroke-linecap', 'stroke-linejoin', 'fill', 'transform'],
}


def source_geometry(elements):
    result = []
    for element in elements:
        tag, attrs = element['tag'], element['attrs']
        require(tag in SOURCE_ATTRIBUTES, f'Unsupported Source shape {tag}')
        for name in attrs:
            require(name in SOURCE_ATTRIBUTES[tag], f'Source {tag} has unmodeled attribute {name}')
        if attrs.get('style'):
            for name in attrs['style']:
                require(name == 'stroke-width', f'Source style has unmodeled property {name}')
        value = lambda name, fallback=Fraction(0): decimal(str(attrs[name]), 'Source ' + name) if name in attrs else fallback
        scaled = lambda number: key(number / 1000)
        width = Fraction(0)
        if tag == 'line':
            segments = [['M', scaled(value('x1')), scaled(value('y1'))], ['L', scaled(value('x2')), scaled(value('y2'))]]
            width = value('stroke-width') / 1000
        elif tag == 'rect':
            x, y, w, h, rx = [value(name) for name in ['x', 'y', 'width', 'height', 'rx']]
            x1, y1 = x + w, y + h
            radius = min(rx, w / 2, h / 2) if rx > 0 else Fraction(0)
            width = value('stroke-width') / 1000
            if not radius:
                segments = [['M', scaled(x), scaled(y)], ['L', scaled(x1), scaled(y)],
                            ['L', scaled(x1), scaled(y1)], ['L', scaled(x), scaled(y1)], ['Z']]
            else:
                r = scaled(radius)
                segments = [['M', scaled(x + radius), scaled(y)], ['L', scaled(x1 - radius), scaled(y)],
                            ['A', r, r, '0/1', False, True, scaled(x1), scaled(y + radius)],
                            ['L', scaled(x1), scaled(y1 - radius)],
                            ['A', r, r, '0/1', False, True, scaled(x1 - radius), scaled(y1)],
                            ['L', scaled(x + radius), scaled(y1)],
                            ['A', r, r, '0/1', False, True, scaled(x), scaled(y1 - radius)],
                            ['L', scaled(x), scaled(y + radius)],
                            ['A', r, r, '0/1', False, True, scaled(x + radius), scaled(y)], ['Z']]
        elif tag == 'ellipse':
            rx, ry, cx, cy = [value(name) for name in ['rx', 'ry', 'cx', 'cy']]
            width = value('stroke-width') / 1000
            segments = [['M', scaled(cx + rx), scaled(cy)],
                        ['A', scaled(rx), scaled(ry), '0/1', False, True, scaled(cx - rx), scaled(cy)],
                        ['A', scaled(rx), scaled(ry), '0/1', False, True, scaled(cx + rx), scaled(cy)], ['Z']]
        else:
            segments = source_path(attrs['d'])
            if 'style' in attrs and attrs['style'] is not None:
                width = decimal(str(attrs['style']['stroke-width'])) / 1000
        join = attrs.get('stroke-linejoin')
        paint = {'fill': attrs.get('fill', 'inherited'), 'stroke': 'inherited', 'stroke_width': key(width),
                 'opacity': '1/1', 'fill_rule': 'nonzero', 'fill_opacity': None, 'stroke_opacity': None,
                 'line_cap': attrs.get('stroke-linecap'), 'line_join': 'miter' if join == 'mitre' else join}
        result.append({'segments': segments, 'transform': transform_ops(attrs.get('transform')), 'paint': paint})
    return result
