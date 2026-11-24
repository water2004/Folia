"""Original diagnostic records and negative controls, without a Node dependency."""
import copy
from contextlib import redirect_stderr, redirect_stdout
import hashlib
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from validation.common import ComparisonError, ROOT, equal, loads
from validation import diagnostics, extpfeil

FIXTURES = Path(__file__).with_name('fixtures')
DIFFERENCES = json.loads((FIXTURES / 'diagnostic-differences.json').read_bytes())
EXTPFEIL = json.loads((FIXTURES / 'extpfeil-diagnostic.json').read_bytes())


class DifferenceDiagnosticsTests(unittest.TestCase):
    def test_original_complete_reports(self):
        for row in DIFFERENCES['checks']:
            with self.subTest(name=row['name']):
                observed = diagnostics.summarize(loads(row['source_json']), loads(row['native_json']))
                expected = copy.deepcopy(row['report'])
                # Only the former host-locale presentation order changes.
                expected['groups'].sort(key=lambda item: (
                    -item['count'], item['path'].encode('utf-16-be', errors='surrogatepass')))
                equal(expected, observed)

    def test_non_corpus_json_produces_differences_instead_of_callback_failure(self):
        report = diagnostics.summarize({'nested': [True, None]}, {'nested': [1, False]})
        self.assertEqual(report['cases'], 1)
        self.assertEqual(report['matched'], 0)
        equal(report['rows'][0], {'name': '$', 'differences': [
            {'path': '$.nested[0]', 'source': True, 'native': 1},
            {'path': '$.nested[1]', 'source': None, 'native': False}]})
        self.assertEqual(diagnostics.summarize([], [])['matched'], 1)

    def test_diagnostics_do_not_modify_inputs(self):
        row = DIFFERENCES['checks'][2]
        source, native = loads(row['source_json']), loads(row['native_json'])
        before = copy.deepcopy((source, native))
        diagnostics.summarize(source, native)
        equal(before[0], source)
        equal(before[1], native)

    def test_cli_keeps_input_hashes_and_reports_every_difference(self):
        row = DIFFERENCES['checks'][2]
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory)
            source, native = folder / 'source.json', folder / 'native.json'
            source.write_text(row['source_json'], encoding='utf-8')
            native.write_text(row['native_json'], encoding='utf-8')
            before = source.read_bytes(), native.read_bytes()
            result = subprocess.run([sys.executable,
                str(ROOT / 'tools/math-port/diagnose-tex-package-differences.py'), str(source), str(native)],
                cwd=folder, capture_output=True, text=True, check=False)
            self.assertEqual(result.returncode, 0, result.stderr)
            report = json.loads(Path(str(native) + '.all-differences.json').read_bytes())
            self.assertEqual(report['sourceSha256'], hashlib.sha256(before[0]).hexdigest())
            self.assertEqual(report['nativeSha256'], hashlib.sha256(before[1]).hexdigest())
            self.assertEqual(report['totalDifferences'], row['report']['totalDifferences'])
            self.assertEqual(json.loads(result.stdout)['totalDifferences'], report['totalDifferences'])
            self.assertEqual(before, (source.read_bytes(), native.read_bytes()))

    def test_invalid_and_missing_captures_fail(self):
        with tempfile.TemporaryDirectory() as directory:
            source, native = Path(directory) / 'source.json', Path(directory) / 'native.json'
            native.write_text('{}')
            for contents in (None, 'NaN', '{broken'):
                with self.subTest(contents=contents), redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
                    if contents is not None:
                        source.write_text(contents)
                    self.assertEqual(diagnostics.main([str(source), str(native)]), 1)
                    self.assertFalse(Path(str(native) + '.all-differences.json').exists())


class ExtpfeilDiagnosticsTests(unittest.TestCase):
    def setUp(self):
        self.fixture = copy.deepcopy(EXTPFEIL)

    def diagnose(self):
        return extpfeil.diagnose(self.fixture['source'], self.fixture['native_json'].encode(),
            self.fixture['comparison'], self.fixture['provenance'],
            ROOT / 'build/math-port/common-layout-native-g113.json', self.fixture['warnings'])

    def native_change(self, change):
        native = json.loads(self.fixture['native_json'])
        change(native)
        self.fixture['native_json'] = json.dumps(native)
        self.fixture['comparison']['nativeSha256'] = hashlib.sha256(
            self.fixture['native_json'].encode()).hexdigest()

    def test_complete_original_report(self):
        equal(self.fixture['expected'], self.diagnose())

    def test_changed_native_bytes_rejected_before_semantic_analysis(self):
        self.fixture['native_json'] += '\n'
        with self.assertRaisesRegex(ComparisonError, 'hash changed'):
            self.diagnose()

    def test_failed_or_malformed_native_provenance_is_rejected(self):
        for code in (None, False, 1, '0'):
            with self.subTest(code=code):
                self.fixture['provenance']['exitCode'] = code
                with self.assertRaisesRegex(ComparisonError, 'complete successfully'):
                    self.diagnose()
        self.fixture['provenance']['exitCode'] = 0
        for sha in (None, 'short', ['1'] * 64):
            with self.subTest(sha=sha):
                self.fixture['provenance']['executableSha256'] = sha
                with self.assertRaisesRegex(ComparisonError, 'hash is missing'):
                    self.diagnose()

    def test_missing_source_and_native_cases_are_rejected(self):
        self.fixture['source']['records'].clear()
        with self.assertRaisesRegex(ComparisonError, 'Source profile omitted'):
            self.diagnose()
        self.fixture = copy.deepcopy(EXTPFEIL)
        self.native_change(lambda native: native['records'].clear())
        with self.assertRaisesRegex(ComparisonError, 'native capture omitted'):
            self.diagnose()

    def test_missing_semantic_arrow_source_glyph_and_native_glyph_are_rejected(self):
        mutations = [
            (lambda row: row['mml']['children'].clear(), 'arrow text differs'),
            (lambda row: row['calls'].clear(), 'semantic xmapsto glyph'),
        ]
        for mutate, message in mutations:
            with self.subTest(message=message):
                self.fixture = copy.deepcopy(EXTPFEIL)
                self.native_change(lambda native: mutate(native['records'][0]))
                with self.assertRaisesRegex(ComparisonError, message):
                    self.diagnose()
        self.fixture = copy.deepcopy(EXTPFEIL)
        self.fixture['source']['records'][0]['calls'].clear()
        with self.assertRaisesRegex(ComparisonError, 'recorded NewCM'):
            self.diagnose()

    def test_missing_wrapper_is_rejected(self):
        self.native_change(lambda native: native['records'][0]['wrappers'].clear())
        with self.assertRaisesRegex(ComparisonError, 'Missing wrapper 13'):
            self.diagnose()

    def test_reference_capture_runs_only_pinned_observer(self):
        with patch.object(extpfeil, 'execute', return_value=(
                {'exitCode': 0}, json.dumps(self.fixture['source']).encode(), b'notice\n')) as run:
            source, warnings = extpfeil.capture_source()
        self.assertEqual(source, self.fixture['source'])
        self.assertEqual(warnings, 'notice\n')
        self.assertEqual(run.call_args.args[1], [
            ROOT / 'tools/math-port/common-layout-original-oracle.cjs', '--profile-corpus'])
        self.assertEqual(run.call_args.kwargs['cwd'], ROOT)
        self.assertEqual(run.call_args.kwargs['timeout'], 60)

    def test_failed_timed_out_and_malformed_reference_captures_are_rejected(self):
        for process in ({'exitCode': 1}, {'exitCode': 0, 'timedOut': True},
                        {'exitCode': None, 'error': 'missing executable'}):
            with self.subTest(process=process), patch.object(extpfeil, 'execute',
                    return_value=(process, b'{}', b'observer failed')):
                with self.assertRaisesRegex(ComparisonError, 'observer failed'):
                    extpfeil.capture_source()
        with patch.object(extpfeil, 'execute', return_value=({'exitCode': 0}, b'NaN', b'')):
            with self.assertRaises(ComparisonError):
                extpfeil.capture_source()


if __name__ == '__main__':
    unittest.main()
