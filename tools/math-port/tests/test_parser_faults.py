import contextlib
import io
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from validation.common import ROOT, ComparisonError, read
from validation.parser_faults import main
from validation.parser_faults.plan import Original, WITNESSES, method_range, replacement, sha256
from validation.parser_faults.runner import configured_build, run_faults


class ParserFaultTests(unittest.TestCase):
    def test_method_scanner_ignores_quotes_comments_and_nested_braces(self):
        for newline in ['\n', '\r\n']:
            body = '{\n // }\n const char* s = "{\\\"}"; /* } */\n char c = \'{\';\n if (ok) { work(); }\n}'
            body = body.replace('\n', newline)
            text = '// preceding 😀 text\nvoid C::method() ' + body + '\nvoid other() {}\n'
            begin, end = method_range(text, 'C::method descriptive suffix')
            self.assertEqual(text[begin:end], body)
        for text in ['void other() {}', 'void C::method(); void C::method() {}', 'void C::method() {']:
            with self.assertRaises(ComparisonError):
                method_range(text, 'C::method')

    def test_scoped_replacement_and_crlf_preserve_all_unrelated_bytes(self):
        for newline in ['\n', '\r\n']:
            text = ('// before\nvoid C::method() {\n old();\n next();\n}\n// old(); outside\n').replace('\n', newline)
            original = Original(Path('unused.cpp'), text.encode(), text)
            row = {'id': 'unit', 'method': 'C::method', 'anchor': 'old();\n next();',
                   'replacement': 'changed();\n next();', 'nativeCase': 'witness',
                   'restoreSha256': sha256(original.data).upper()}
            changed = replacement(row, original, '"witness"_test = [] {};')
            self.assertEqual(changed, text.replace('old();', 'changed();', 1).encode())
            for field, value in [('anchor', 'absent'), ('restoreSha256', '0' * 64), ('nativeCase', 'absent')]:
                with self.assertRaises(ComparisonError):
                    replacement(dict(row, **{field: value}), original, '"witness"_test = [] {};')
            duplicate = text.replace('old();', 'old(); old();', 1)
            original = Original(Path('unused.cpp'), duplicate.encode(), duplicate)
            with self.assertRaises(ComparisonError):
                replacement(dict(row, anchor='old();', restoreSha256=sha256(original.data)), original,
                            '"witness"_test = [] {};')

    def test_every_original_fault_retains_its_named_native_witness(self):
        plan = read(ROOT / 'tools/math-port/parser-host-fault-plan.json')
        self.assertEqual(len(plan['rows']), 33)
        self.assertEqual(len({row['id'] for row in plan['rows']}), 33)
        candidate = '\n'.join((ROOT / 'tools/math-port' / file).read_text() for file in WITNESSES)
        for row in plan['rows']:
            self.assertIn('"' + row['nativeCase'] + '"_test', candidate)
            self.assertEqual(row['restoreSha256'], plan['baselineSha256'][row['source']])

    def test_native_failures_always_restore_source_and_release_lock(self):
        for failure in [None, 'compile', 'wrong-exit', 'missing-assertion', 'restored-compile', 'restored-witness']:
            with self.subTest(failure=failure), tempfile.TemporaryDirectory() as directory:
                work = Path(directory)
                source = work / 'source.cpp'
                source.write_bytes(b'original\r\n')
                original = Original(source, source.read_bytes(), source.read_text())
                row = {'id': 'unit', 'source': 'source.cpp', 'nativeCase': 'witness', 'changed': b'changed\r\n'}
                calls = []
                case = self

                class Native:
                    def build(self, label):
                        calls.append(label)
                        expected = original.data if label.endswith('-restored') else row['changed']
                        case.assertEqual(source.read_bytes(), expected)
                        if (failure == 'compile' and label == 'unit'
                                or failure == 'restored-compile' and label == 'unit-restored'):
                            raise ComparisonError('test compile failure')

                    def run(self, pattern, label):
                        if label in ['baseline', 'restored-full']:
                            return 0, b'2 asserts in 1 tests'
                        if label.endswith('-restored'):
                            return (1 if failure == 'restored-witness' else 0), b'passed'
                        return (0 if failure == 'wrong-exit' else 1), (b'' if failure == 'missing-assertion' else b'FAILED')

                with contextlib.redirect_stdout(io.StringIO()):
                    if failure is None:
                        report = run_faults({'baselineSha256': {}}, {'source.cpp': original}, [row], [], work, Native())
                        self.assertEqual(report['rows'][0]['detected'], True)
                        self.assertTrue((work / 'results.json').exists())
                    else:
                        with self.assertRaises(ComparisonError):
                            run_faults({'baselineSha256': {}}, {'source.cpp': original}, [row], [], work, Native())
                self.assertEqual(source.read_bytes(), original.data)
                self.assertEqual(calls[-1], 'unit-restored')
                self.assertFalse((work / 'run.lock').exists())

    def test_execution_requires_opt_in_build_and_correct_cmake_checkout(self):
        with self.assertRaisesRegex(ComparisonError, 'explicit --build-dir'):
            main(['--execute'])
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            build = root / 'build'
            build.mkdir()
            cache = build / 'CMakeCache.txt'
            cache.write_text('CMAKE_HOME_DIRECTORY:INTERNAL=' + str(root) + '\n')
            self.assertEqual(configured_build(root, build), build)
            cache.write_text('CMAKE_HOME_DIRECTORY:INTERNAL=' + str(root.parent / 'other') + '\n')
            with self.assertRaises(ComparisonError):
                configured_build(root, build)


if __name__ == '__main__':
    unittest.main()
