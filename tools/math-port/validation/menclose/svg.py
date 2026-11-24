"""Strict writer SVG parser: unknown syntax, paint and hierarchy never disappear."""
from fractions import Fraction
import re

from ..common import equal, require
from .geometry import source_geometry
from .path import SPACE, finite_number, key, native_path, transform_ops

PAINT = ['fill', 'stroke', 'opacity', 'fill-rule', 'fill-opacity', 'stroke-opacity', 'stroke-linecap', 'stroke-linejoin', 'transform']
SCHEMA = {
    'path': (['d', 'style', 'stroke-width'], ['d']),
    'line': (['x1', 'y1', 'x2', 'y2', 'stroke-width'], ['x1', 'y1', 'x2', 'y2']),
    'rect': (['x', 'y', 'width', 'height', 'rx', 'stroke-width'], ['x', 'y', 'width', 'height']),
    'ellipse': (['rx', 'ry', 'cx', 'cy', 'stroke-width'], ['rx', 'ry', 'cx', 'cy']),
}


def attributes(source, allowed, context):
    result, offset = {}, 0
    pattern = re.compile(SPACE + r'+([A-Za-z_:][A-Za-z0-9_.:-]*)="([^"<>]*)"')
    while offset < len(source):
        if re.fullmatch(SPACE + '*', source[offset:]):
            break
        found = pattern.match(source, offset)
        require(found is not None, f'{context}: unsupported or malformed attribute near {source[offset:]}')
        name, value = found[1], found[2]
        require(name in allowed, f'{context}: unsupported attribute {name}')
        require(name not in result, f'{context}: duplicate attribute {name}')
        result[name], offset = value, found.end()
    return result


def native_shape(tag, attrs, nested):
    geometry, _ = SCHEMA[tag]
    for name in attrs:
        require(name in geometry + PAINT, f'{tag}: unsupported attribute {name}')
    values = {name: finite_number(attrs[name], tag + ' ' + name)
              for name in geometry if name not in ['d', 'style'] and name in attrs}
    nonnegative = ['width', 'height', 'rx'] if tag == 'rect' else ['rx', 'ry'] if tag == 'ellipse' else []
    for name in nonnegative:
        require(name not in values or values[name] >= 0, f'{tag} {name} must be nonnegative')
    style_width, style_text = None, None
    if 'style' in attrs:
        match = re.fullmatch(SPACE + '*stroke-width' + SPACE + '*:' + SPACE + '*([^;]+?)' + SPACE + '*;?' + SPACE + '*', attrs['style'])
        require(match is not None, 'Path style only supports one stroke-width declaration')
        style_text, style_width = match[1], finite_number(match[1], 'path style stroke-width')
    source_attrs = {name: attrs[name] for name in geometry if name not in ['style', 'stroke-width'] and name in attrs}
    width = style_width if style_width is not None else values.get('stroke-width')
    if tag == 'path' and width is not None:
        source_attrs['style'] = {'stroke-width': style_text if style_text is not None else attrs['stroke-width']}
    elif tag != 'path' and width is not None:
        source_attrs['stroke-width'] = attrs['stroke-width']
    if tag == 'path':
        for name in ['stroke-linecap', 'stroke-linejoin', 'fill']:
            if name in attrs:
                source_attrs[name] = attrs[name]
    normalized = source_geometry([{'tag': tag, 'attrs': source_attrs}])[0]
    cap, join = attrs.get('stroke-linecap'), attrs.get('stroke-linejoin')
    require(cap is None or cap in ['butt', 'round', 'square'], f'Unsupported stroke-linecap {cap}')
    require(join is None or join in ['miter', 'mitre', 'round', 'bevel'], f'Unsupported stroke-linejoin {join}')
    numeric = lambda name, fallback: finite_number(attrs[name], tag + ' ' + name) if name in attrs else fallback
    return {'segments': native_path(attrs['d']) if tag == 'path' else normalized['segments'],
            'transform': nested + transform_ops(attrs.get('transform')),
            'paint': {'fill': attrs.get('fill', 'inherited'), 'stroke': attrs.get('stroke', 'inherited'),
                      'stroke_width': normalized['paint']['stroke_width'], 'opacity': key(numeric('opacity', Fraction(1))),
                      'fill_rule': attrs.get('fill-rule', 'nonzero'),
                      'fill_opacity': key(numeric('fill-opacity', Fraction(1))) if 'fill-opacity' in attrs else None,
                      'stroke_opacity': key(numeric('stroke-opacity', Fraction(1))) if 'stroke-opacity' in attrs else None,
                      'line_cap': cap, 'line_join': 'miter' if join == 'mitre' else join}}


def parse_svg(svg):
    shapes, stack, coordinate_seen, root_closed, offset = [], [], False, False, 0
    pattern = re.compile(r'<(/)?([A-Za-z][A-Za-z0-9:-]*)([^<>]*?)(/?)>')
    while offset < len(svg):
        if re.fullmatch(SPACE, svg[offset]):
            offset += 1
            continue
        match = pattern.match(svg, offset)
        require(match is not None, f'Unparsed SVG content at {offset}: {svg[offset:offset + 50]}')
        close, name, attr_source, self_close = match.groups()
        offset = match.end()
        if close:
            require(not self_close, f'Closing {name} tag cannot be self-closing')
            top = stack.pop() if stack else None
            require(top is not None and top['name'] == name, f'Unexpected closing SVG tag {name}')
            if name == 'svg':
                require(not stack and coordinate_seen, 'SVG root closed at an invalid depth')
                root_closed = True
            continue
        require(not root_closed, 'Markup follows the SVG root')
        if name == 'svg':
            require(not stack and not self_close, 'SVG must have one non-self-closing outer root')
            required = ['style', 'xmlns', 'width', 'height', 'role', 'focusable', 'viewBox']
            attrs = attributes(attr_source, required + ['xmlns:xlink'], 'svg root')
            for field in required:
                require(field in attrs, 'SVG root is missing ' + field)
            require(attrs['xmlns'] == 'http://www.w3.org/2000/svg' and attrs['role'] == 'img' and attrs['focusable'] == 'false',
                    'SVG root namespace or accessibility attributes differ from writer contract')
            require('xmlns:xlink' not in attrs or attrs['xmlns:xlink'] == 'http://www.w3.org/1999/xlink', 'Unsupported xlink namespace')
            stack.append({'name': 'svg', 'attrs': attrs})
        elif name == 'g':
            require(not self_close and stack and stack[-1]['name'] in ['g', 'svg'], 'Groups must be non-self-closing children of SVG/groups')
            if len(stack) == 1:
                require(not coordinate_seen, 'More than one top-level coordinate group')
                attrs = attributes(attr_source, ['stroke', 'fill', 'stroke-width', 'transform'], 'root coordinate group')
                equal({'stroke': 'currentColor', 'fill': 'currentColor', 'stroke-width': '0', 'transform': 'scale(1,-1)'}, attrs,
                      'Root coordinate group must be exact unique MathJax y-flip/currentColor group')
                coordinate_seen = True
                stack.append({'name': 'g', 'coordinateRoot': True, 'transforms': []})
            else:
                require(coordinate_seen and any(row.get('coordinateRoot') for row in stack), 'Nested group is outside coordinate group')
                attrs = attributes(attr_source, ['data-mml-node', 'transform'], 'nested group')
                require(attrs.get('data-mml-node') == 'mrow', 'Unexpected writer group node label')
                stack.append({'name': 'g', 'coordinateRoot': False, 'transforms': transform_ops(attrs.get('transform'))})
        elif name in SCHEMA:
            require(self_close and len(stack) >= 3 and stack[-1]['name'] == 'g' and any(row.get('coordinateRoot') for row in stack),
                    name + ' elements must be self-closing descendants of coordinate group')
            geometry, required = SCHEMA[name]
            attrs = attributes(attr_source, geometry + PAINT, name)
            for field in required:
                require(field in attrs, name + ' is missing ' + field)
            nested = [transform for row in stack if row['name'] == 'g' and not row['coordinateRoot'] for transform in row['transforms']]
            shapes.append(native_shape(name, attrs, nested))
        else:
            require(False, f'Unsupported SVG element <{name}>')
    require(root_closed and not stack, 'SVG root is unclosed')
    require(bool(shapes), 'SVG contains no geometry paths')
    return shapes
