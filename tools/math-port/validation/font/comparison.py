"""Complete font catalog comparison with exact values and bounded diagnostics."""
import json
import math

from ..common import MISSING, field, js_type, keys, read, emit, require, shown, sorted_keys, truthy
from .glyph import (coalesce, effective_remaps, integer, number, or_default, resource_key, slot, source_glyph)
from .outline import outline_from_fragment, utf16_units


class FontComparison:
    def __init__(self):
        self.compared = 0
        self.mismatch_count = 0
        self.failures = []

    def fail(self, path, actual, expected):
        self.mismatch_count += 1
        if len(self.failures) < 100:
            row = {'path': path}
            if actual is not MISSING:
                row['actual'] = actual
            if expected is not MISSING:
                row['expected'] = expected
            self.failures.append(row)

    def eq(self, actual, expected, path):
        self.compared += 1
        if js_type(expected) == 'number':
            if js_type(actual) != 'number' or not math.isfinite(actual) or actual != expected:
                self.fail(path, actual, expected)
        elif expected is None or not isinstance(expected, (list, dict)):
            if js_type(actual) != js_type(expected) or actual != expected:
                self.fail(path, actual, expected)
        elif isinstance(expected, list):
            if not isinstance(actual, list) or len(actual) != len(expected):
                self.fail(path, actual, expected)
                return
            for index, value in enumerate(expected):
                self.eq(actual[index], value, f'{path}[{index}]')
        else:
            if not isinstance(actual, dict):
                self.fail(path, actual, expected)
                return
            expected_keys, actual_keys = sorted_keys(expected), sorted_keys(actual)
            if actual_keys != expected_keys:
                self.fail(path + ' keys', actual_keys, expected_keys)
            for key in expected_keys:
                if key in actual:
                    self.eq(actual[key], expected[key], path + '.' + key)

    def compare(self, source, native):
        eq, fail = self.eq, self.fail
        native_aliases = {resource_key(left): right for left, right in native.get('svg_aliases', [])}

        def lookup(key):
            name = resource_key(key)
            direct = native['svg_resources'].get(name)
            if truthy(direct):
                return direct
            target = native_aliases.get(name)
            return native['svg_resources'].get(resource_key(target)) if target is not None else None

        if native.get('face') != 'MathJaxNewcm':
            fail('face', field(native, 'face'), 'MathJaxNewcm')
        for native_key, source_key in [('source_profile_glyph_count', 'newcm_variant_glyph_keys'),
                                       ('source_dynamic_module_count', 'dynamic_modules'),
                                       ('source_provider_trace_events', 'provider_trace_events')]:
            eq(field(native['source_profile'], native_key), field(source['sourceProfile'], source_key), 'source_profile.' + native_key)
        font = source['newcm']
        for name, key in [('parameters', 'effectiveParameters'), ('size_variants', 'effectiveSizeVariants'), ('stretch_variants', 'effectiveStretchVariants')]:
            eq(field(native, name), field(font, key), name)
        for name, key in [('operator_remap', 'mo'), ('accent_remap', 'accent'), ('number_remap', 'mn')]:
            encoded = {cp: utf16_units(text) for cp, text in font['effectiveRemaps'].get(key, {}).items()}
            eq(field(native, name), encoded, name)
        for cp, delimiter in font['effectiveDelimiters'].items():
            actual = native['delimiters'].get(cp)
            if not truthy(actual):
                fail('delimiters.' + cp, None, delimiter)
                continue
            expected = {'direction': 2 if delimiter.get('dir') == 'v' else 1 if delimiter.get('dir') == 'h' else 0}
            expected.update({name: or_default(field(delimiter, name), []) for name in ['sizes', 'variants', 'schar', 'stretch', 'stretchv']})
            expected.update({name: coalesce(field(delimiter, name), None) for name in ['ext', 'min']})
            expected['c'] = None if delimiter.get('c') is None else number(delimiter['c'])
            expected.update({name: coalesce(field(delimiter, name), None) for name in ['hd', 'HDW', 'fullExt']})
            eq(actual, expected, 'delimiters.' + cp)
        if len(native['delimiters']) != len(font['effectiveDelimiters']):
            fail('delimiters count', len(native['delimiters']), len(font['effectiveDelimiters']))
        source_variants, actual_variants = font['loadedVariants'], native['variants']
        source_aliases = {resource_key(left): right for left, right in source.get('svgAliases', [])}
        inheritance = {row[0]: {'parent': coalesce(slot(row, 1), None), 'link': coalesce(slot(row, 2), None)} for row in font['defaultVariants']}
        for extension in source['extensions']:
            for row in extension['data'].get('variants', {}).get('[+]', []):
                inheritance[row[0]] = inheritance.get(row[0], {'parent': None, 'link': None}) | {'parent': row[1]}
        for name, record in inheritance.items():
            actual = actual_variants.get(name)
            if truthy(actual):
                eq(field(actual, 'inherit'), record['parent'], 'variants.' + name + '.inherit')
                eq(field(actual, 'link_overlay'), record['link'], 'variants.' + name + '.link_overlay')
        counts = {extension['name']: sum(len(chars) for chars in extension['data'].get('chars', {}).values()) for extension in source['extensions']}
        eq(field(native, 'extension_glyph_counts'), counts, 'extension_glyph_counts')
        for name, variant in source_variants.items():
            actual = actual_variants.get(name)
            if not truthy(actual):
                fail('variants.' + name, None, 'present')
                continue
            prefix = 'variants.' + name
            eq(field(actual, 'cache_id'), coalesce(field(variant, 'cacheID'), ''), prefix + '.cache_id')
            direct = {cp: source_glyph(row) for cp, row in variant['ownChars'].items() if not integer(field(slot(row, 3), 'smp'))}
            if len(actual['glyphs']) != len(direct):
                fail(prefix + '.glyph count', len(actual['glyphs']), len(direct))
            for cp, glyph in direct.items():
                item = actual['glyphs'].get(cp)
                at = prefix + '.glyphs.' + cp
                if not truthy(item):
                    fail(at, None, {key: glyph[key] for key in ['metrics', 'path_defined', 'composition']})
                    continue
                for key in ['metrics', 'path_defined', 'composition']:
                    eq(field(item, key), glyph[key], at + '.' + key)
                parsed = outline_from_fragment(glyph['path_data']) if glyph['path_defined'] else None
                if parsed is None:
                    if field(item, 'outline') is not None:
                        fail(at + '.outline', field(item, 'outline'), None)
                    eq(field(item, 'outline_error'), None, at + '.outline_error')
                else:
                    if not truthy(field(item, 'outline')):
                        fail(at + '.outline', None, 'resource')
                    else:
                        eq(field(item['outline'], 'font_id'), 'MathJaxNewcm/' + name, at + '.outline.font_id')
                        eq(field(item['outline'], 'segments'), parsed['segments'], at + '.outline.segments')
                    eq(field(item, 'outline_error'), parsed['error'], at + '.outline_error')
            eq(field(actual, 'remap'), effective_remaps(name, variant['visibleChars']), prefix + '.remap')
            # Every Source-visible alias is checked through the exact native catalog.
            for cp, row in variant['visibleChars'].items():
                resolved_cp = field(slot(row, 3), 'smp')
                resolved_cp = resolved_cp if integer(resolved_cp) else number(cp)
                resolved = source.get('sourceResolvedVisible', {}).get(name, {}).get(cp)
                if not truthy(resolved):
                    fail(f'Source getChar {name}/{cp}', None, 'captured result')
                    continue
                eq(resolved['requested_codepoint'], number(cp), f'Source getChar {name}/{cp}.requested_codepoint')
                eq(resolved['target_codepoint'], resolved_cp, f'Source getChar {name}/{cp}.target_codepoint')
                glyph, resource = source_glyph(resolved['tuple']), lookup([name, resolved_cp])
                if glyph['path_defined'] or glyph['content_data'] is not None:
                    if not truthy(resource):
                        fail(f'svg {name}/{cp}->{int(resolved_cp)}', None, 'present Source resource')
                        continue
                    for key, value in self.svg_resource(glyph).items():
                        eq(field(resource, key), value, f'svg {name}/{cp}.' + key)
            effective = native.get('effective_glyphs', {}).get(name) or {}
            if len(effective) != len(variant['visibleChars']):
                fail('effective_glyphs.' + name + ' count', len(effective), len(variant['visibleChars']))
            for cp, row in variant['visibleChars'].items():
                resolved_cp = field(slot(row, 3), 'smp')
                resolved_cp = resolved_cp if integer(resolved_cp) else number(cp)
                resolved = source.get('sourceResolvedVisible', {}).get(name, {}).get(cp)
                if not truthy(resolved):
                    fail(f'Source getChar {name}/{cp}', None, 'captured result')
                    continue
                eq(resolved['requested_codepoint'], number(cp), f'Source getChar {name}/{cp}.requested_codepoint')
                eq(resolved['target_codepoint'], resolved_cp, f'Source getChar {name}/{cp}.target_codepoint')
                glyph, item = source_glyph(resolved['tuple']), effective.get(cp)
                at = 'effective_glyphs.' + name + '.' + cp
                if not truthy(item):
                    fail(at, None, 'present')
                    continue
                for key in ['metrics', 'path_defined', 'composition']:
                    eq(field(item, key), glyph[key], at + '.' + key)
                eq(field(item, 'glyph_key'), [name, coalesce(field(variant, 'cacheID'), ''), resolved_cp], at + '.glyph_key')
                parsed = outline_from_fragment(glyph['path_data']) if glyph['path_defined'] else None
                eq(field(item, 'outline_error'), parsed['error'] if parsed else None, at + '.outline_error')
                alias = source_aliases.get(resource_key([name, resolved_cp]))
                if glyph['path_defined']:
                    eq(field(item, 'outline_font_id'), 'MathJaxNewcm/' + (alias[0] if alias is not None else name), at + '.outline_font_id')
                eq(field(item, 'svg_resource'), self.svg_resource(glyph) if glyph['path_defined'] or glyph['content_data'] is not None else None,
                   at + '.svg_resource')
        for name in actual_variants:
            if name not in source_variants:
                fail('variants.' + name, actual_variants[name], None)
        actual_count = sum(len(row['glyphs']) for row in actual_variants.values())
        source_count = sum(sum(not integer(field(slot(row, 3), 'smp')) for row in variant['ownChars'].values()) for variant in source_variants.values())
        semantic_count = sum(len(row['ownChars']) for row in source_variants.values())
        if actual_count != source_count:
            fail('actual native direct glyph count', actual_count, source_count)
        # Catalog iteration is unordered; retain the entire mapping and duplicates.
        alias_order = lambda rows: sorted(rows or [], key=lambda row: json.dumps(row, ensure_ascii=True, sort_keys=True))
        eq(alias_order(native.get('svg_aliases')), alias_order(source.get('svgAliases')), 'svg_aliases')
        remap_count = sum(len(row['remap']) for row in actual_variants.values())
        outlines = [glyph for variant in actual_variants.values() for glyph in variant['glyphs'].values() if glyph.get('outline') is not None]
        segments = sum(len(glyph['outline']['segments']) for glyph in outlines)
        totals = {'variant_records': len(actual_variants), 'direct_glyph_records': actual_count, 'remap_records': remap_count,
                  'distinct_glyph_resource_objects': len(outlines), 'outline_segment_records': segments}
        for key, value in totals.items():
            eq(field(native['native_actual_counts'], key), value, 'native_actual_counts.' + key)
        return {'passed': self.mismatch_count == 0, 'compared_values': self.compared, 'mismatch_count': self.mismatch_count,
                'mismatch_sample_limit': 100, 'mismatches': self.failures or None,
                'source_semantic_variant_keys': semantic_count, 'source_direct_glyph_rows': source_count,
                'native_actual_direct_glyph_records': actual_count, 'native_actual_remap_records': remap_count,
                'native_actual_glyph_resource_objects': len(outlines), 'native_actual_outline_segment_records': segments,
                'native_actual_svg_resources': len(native['svg_resources']), 'native_actual_svg_aliases': len(native.get('svg_aliases', []))}

    @staticmethod
    def svg_resource(glyph):
        return {'path_defined': glyph['path_defined'],
                'path_data': 'M' + glyph['path_data'] + 'Z' if glyph['path_defined'] and glyph['path_data'] else '',
                'content_data': glyph['content_data']}


def main(args):
    require(len(args) == 2, 'Expected font-source-expected.json and native-font-dump.json')
    report = FontComparison().compare(read(args[0]), read(args[1]))
    emit(report)
    return int(not report['passed'])
