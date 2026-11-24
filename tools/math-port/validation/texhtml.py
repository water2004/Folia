"""Pinned TexHtml, HTMLDomStrings and XML source/native validation."""
import os
from pathlib import Path

from .common import ROOT, MISSING, digest, emit, field, js_type, keys, read, require, root_path, truthy, write
from .ordered import differences
from .processes import execute

SOURCE_ROOT = ROOT / '.reference/MathJax-src'
PACKAGE_ROOT = ROOT / 'tools/mathjax-bundle/node_modules/@mathjax/src'
META_KEYS = ['schema', 'reference', 'commit', 'scopeSha256', 'sourceHashes']
DOM_KEYS = ['defaultsBefore', 'configReplacement', 'registeredAfterConfig', 'instanceOptions', 'nestedMarkers']


class TexHtmlValidation:
    def __init__(self, lock_path):
        self.failures = []
        self.provenance = {'lock': {'path': os.path.relpath(lock_path, ROOT), 'sha256': digest(lock_path)},
                           'nativeInputs': [], 'originalOracles': [], 'lockedDependencies': {'typescript': [], 'cjs': []}}

    def fail(self, record, detail):
        self.failures.append({'record': record, 'difference': detail})

    def compare_value(self, record, expected, actual, path):
        for found in differences(expected, actual, path):
            self.fail(record, found)

    def validate_lock(self, lock):
        for key, value in [('schema', 1), ('reference', 'MathJax 4.1.3')]:
            if js_type(field(lock, key)) != js_type(value) or field(lock, key) != value:
                self.fail('lock.' + key, {'expected': value, 'actual': lock.get(key)})
        scope_path = ROOT / 'docs/math-port/scope.json'
        if not scope_path.exists():
            self.fail('lock.scope.json', {'missing': str(scope_path)})
        elif digest(scope_path) != lock['scopeSha256']:
            self.fail('lock.scopeSha256', {'expected': lock['scopeSha256'], 'actual': digest(scope_path)})
        if not isinstance(lock.get('sources'), list):
            self.fail('lock.sources', {'expected': 'array', 'actual': js_type(field(lock, 'sources'))})
        else:
            for item in lock['sources']:
                upstream, installed = SOURCE_ROOT / item['file'], PACKAGE_ROOT / item['file']
                if not upstream.exists():
                    self.fail('locked TS ' + item['file'], {'missing': str(upstream)})
                    continue
                actual = digest(upstream)
                if actual != item['sha256']:
                    self.fail('locked TS ' + item['file'], {'expected': item['sha256'], 'actual': actual})
                self.provenance['lockedDependencies']['typescript'].append({'file': item['file'], 'sha256': actual})
                if not installed.exists():
                    self.fail('installed TS ' + item['file'], {'missing': str(installed)})
                    continue
                source_text = upstream.read_bytes().decode('utf-8').replace('\r\n', '\n')
                installed_text = installed.read_bytes().decode('utf-8').replace('\r\n', '\n')
                if installed_text != source_text:
                    self.fail('installed TS ' + item['file'], {'differsFrom': os.path.relpath(upstream, ROOT)})
        if not isinstance(lock.get('cjsDependencies'), list):
            self.fail('lock.cjsDependencies', {'expected': 'array', 'actual': js_type(field(lock, 'cjsDependencies'))})
        else:
            for item in lock['cjsDependencies']:
                file = PACKAGE_ROOT / item['file']
                if not file.exists():
                    self.fail('locked CJS ' + item['file'], {'missing': str(file)})
                    continue
                actual = digest(file)
                if actual != item['sha256']:
                    self.fail('locked CJS ' + item['file'], {'expected': item['sha256'], 'actual': actual})
                self.provenance['lockedDependencies']['cjs'].append({'file': item['file'], 'sha256': actual})

    def generate_oracle(self, script, output_name):
        script_path = ROOT / script
        output_path = ROOT / 'build/math-port/texhtml-xml-comparer' / output_name
        output_path.parent.mkdir(parents=True, exist_ok=True)
        try:
            result, stdout, stderr = execute(os.environ.get('FOLIA_NODE_EXECUTABLE', 'node'), [script_path, os.path.relpath(output_path, ROOT)])
            require(result['exitCode'] == 0 and not result.get('timedOut'), stderr.decode('utf-8', errors='replace') or str(result))
            self.provenance['originalOracles'].append({'script': script, 'scriptSha256': digest(script_path),
                'output': os.path.relpath(output_path, ROOT), 'outputSha256': digest(output_path), 'stdout': stdout.decode('utf-8').strip()})
            return read(output_path)
        except (OSError, RuntimeError, ValueError) as error:
            self.fail('oracle ' + script, {'message': str(error), 'stderr': ''})
            return None

    def validate_original(self, label, source, expected_keys, lock):
        if not truthy(source):
            return
        if keys(source) != expected_keys:
            self.fail(label + ' envelope keys', {'expected': expected_keys, 'actual': keys(source)})
        metadata = {key: field(lock, key) for key in META_KEYS[:-1]} | {'sourceHashes': field(lock, 'sources')}
        for key, value in metadata.items():
            self.compare_value(label + ' metadata', value, field(source, key), label + '.' + key)
        if not isinstance(source.get('loadedFiles'), list):
            self.fail(label + '.loadedFiles', {'expected': 'array'})
            return
        by_file = {row['file']: row['sha256'] for row in lock['cjsDependencies']}
        seen = set()
        for item in source['loadedFiles']:
            if item['file'] in seen:
                self.fail(label + '.loadedFiles', {'duplicate': item['file']})
            seen.add(item['file'])
            if by_file.get(item['file']) != item['sha256']:
                self.fail(label + '.loadedFiles.' + item['file'], {'expected': by_file.get(item['file']), 'actual': item['sha256']})
            actual_path = PACKAGE_ROOT / item['file']
            if not actual_path.exists():
                self.fail(label + '.loadedFiles.' + item['file'], {'missing': str(actual_path)})
            elif digest(actual_path) != item['sha256']:
                self.fail(label + '.loadedFiles.' + item['file'], {'expected': item['sha256'], 'actual': digest(actual_path)})
        # The pinned 175 dependency names are ASCII and have unique casefold
        # keys. Their original localeCompare order equals this portable order.
        ordered = sorted(source['loadedFiles'], key=lambda row: row['file'].casefold())
        if ordered != source['loadedFiles']:
            self.fail(label + '.loadedFiles order', {'expected': [row['file'] for row in ordered], 'actual': [row['file'] for row in source['loadedFiles']]})

    def native_input(self, file, label):
        try:
            self.provenance['nativeInputs'].append({'label': label, 'path': os.path.relpath(file, ROOT), 'sha256': digest(file)})
            return read(file)
        except (OSError, ValueError) as error:
            self.fail(label + ' input', {'path': str(file), 'message': str(error)})
            return None

    def cases(self, label, expected, actual):
        if not isinstance(expected, list) or not isinstance(actual, list):
            self.fail(label + '.cases', {'sourceIsArray': isinstance(expected, list), 'nativeIsArray': isinstance(actual, list)})
            return
        if len(expected) != len(actual):
            self.fail(label + '.cases.length', {'source': len(expected), 'native': len(actual)})
        for index in range(max(len(expected), len(actual))):
            if index >= len(expected):
                self.fail(f'{label}.cases[{index}]', {'unexpectedNativeRecord': actual[index]})
            elif index >= len(actual):
                self.fail(f'{label}.cases[{index}]', {'missingNativeRecord': expected[index]})
            else:
                name = expected[index].get('name') or f'record-{index}'
                self.compare_value(label + ' record ' + name, expected[index], actual[index], f'{label}.cases[{index}]({name})')

    def observations(self, tex_source, dom_source, xml_source, native_tex, native_xml):
        if truthy(tex_source) and truthy(native_tex):
            metadata = {key: field(tex_source, key) for key in META_KEYS}
            self.compare_value('TexHtml native envelope', list(metadata) + ['texhtml', 'domStrings'], keys(native_tex), 'nativeTex.[key-order]')
            for key, value in metadata.items():
                self.compare_value('TexHtml native metadata', value, field(native_tex, key), 'nativeTex.' + key)
            self.cases('TexHtml', field(tex_source, 'cases'), field(field(native_tex, 'texhtml'), 'cases'))
            dom_cases = field(field(native_tex, 'domStrings'), 'cases')
            if not isinstance(dom_cases, list) or len(dom_cases) != 2:
                self.fail('HTMLDomStrings native cases', {'expected': 2, 'actual': len(dom_cases) if isinstance(dom_cases, list) else js_type(dom_cases)})
            else:
                combined = {key: dom_cases[0][key] for key in DOM_KEYS[:-1] if key in dom_cases[0]}
                if 'nestedMarkers' in dom_cases[1]:
                    combined['nestedMarkers'] = dom_cases[1]['nestedMarkers']
                expected_dom = {key: dom_source[key] for key in DOM_KEYS if isinstance(dom_source, dict) and key in dom_source}
                self.compare_value('HTMLDomStrings native records', expected_dom, combined, 'HTMLDomStrings')
                for index, names in [(0, DOM_KEYS[:-1]), (1, ['nestedMarkers'])]:
                    if keys(dom_cases[index]) != names:
                        self.fail(f'HTMLDomStrings native case {index} keys', {'expected': names, 'actual': keys(dom_cases[index])})
            for field_name, label in [('texhtml', 'TexHtml'), ('domStrings', 'HTMLDomStrings')]:
                value = field(native_tex, field_name)
                if not truthy(value) or keys(value) != ['cases']:
                    self.fail(label + ' native group keys', {'expected': ['cases'], 'actual': keys(value) if truthy(value) else None})
        if truthy(xml_source) and truthy(native_xml):
            if keys(native_xml) != ['cases']:
                self.fail('XML native envelope keys', {'expected': ['cases'], 'actual': keys(native_xml)})
            self.cases('XML', field(xml_source, 'cases'), field(native_xml, 'cases'))


def main(args):
    require(len(args) >= 2, 'Expected TexHtml native JSON, XML native JSON [report JSON]')
    lock_path = ROOT / 'docs/math-port/evidence/texhtml-xml-source-lock.json'
    native_tex_path, native_xml_path = root_path(args, 0, ''), root_path(args, 1, '')
    report_path = root_path(args, 2, 'build/math-port/texhtml-xml-comparison.json')
    check, lock = TexHtmlValidation(lock_path), read(lock_path)
    check.validate_lock(lock)
    tex = check.generate_oracle('tools/math-port/texhtml-original-oracle.cjs', 'texhtml-original.json')
    dom = check.generate_oracle('tools/math-port/html-dom-strings-original-oracle.cjs', 'html-dom-strings-original.json')
    xml = check.generate_oracle('tools/math-port/texhtml-xml-original-oracle.cjs', 'texhtml-xml-original.json')
    check.validate_original('texhtml source', tex, META_KEYS + ['loadedFiles', 'cases'], lock)
    check.validate_original('HTMLDomStrings source', dom, META_KEYS + DOM_KEYS + ['loadedFiles'], lock)
    check.validate_original('XML source', xml, META_KEYS + ['loadedFiles', 'cases'], lock)
    native_tex = check.native_input(native_tex_path, 'TexHtml/HTMLDomStrings native')
    native_xml = check.native_input(native_xml_path, 'XML native')
    for label, filename in [('TexHtml native candidate source', 'texhtml-native-candidate.cpp'), ('XML native candidate source', 'texhtml-xml-native-candidate.cpp')]:
        file = ROOT / 'tools/math-port/drafts' / filename
        if file.exists():
            check.provenance['nativeInputs'].append({'label': label, 'path': os.path.relpath(file, ROOT), 'sha256': digest(file)})
        else:
            check.fail(label, {'missing': str(file)})
    check.observations(tex, dom, xml, native_tex, native_xml)
    report = {'accepted': not check.failures, 'recordTotals': {'texhtml': len(tex.get('cases', [])) if tex else 0,
              'domStrings': 2, 'xml': len(xml.get('cases', [])) if xml else 0, 'failures': len(check.failures)},
              'provenance': check.provenance, 'failures': check.failures}
    write(report_path, report, parents=True)
    emit({'accepted': report['accepted'], 'recordTotals': report['recordTotals'], 'failures': len(check.failures),
          'firstFailure': check.failures[0] if check.failures else None, 'report': os.path.relpath(report_path, ROOT)})
    return int(not report['accepted'])
