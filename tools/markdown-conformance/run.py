"""Run Folia's formal CommonMark 0.31.2 conformance tests and report results."""
import argparse
from collections import Counter, defaultdict
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
DEFAULT_OUTPUT = ROOT / 'build' / 'markdown-conformance'
sys.path.insert(0, str(HERE))
from canonical_html import canonical as canonical_html


def load_fixtures(suite="commonmark"):
    sys.path.insert(0, str(HERE))
    if suite == 'gfm':
        from prepare_gfm import prepare
    else:
        from prepare import prepare
    return prepare(fetch=True)


def normalize_tree(value):
    if isinstance(value, str):
        try:
            return json.loads(value)
        except (ValueError, TypeError):
            return value
    return value


def first_diff(expected, actual, path='$'):
    if type(expected) is not type(actual):
        return {'path': path, 'expected': expected, 'actual': actual}
    if isinstance(expected, dict):
        for key in sorted(set(expected) | set(actual), key=str):
            child = f'{path}.{key}'
            if key not in expected:
                return {'path': child, 'expected': '<missing>', 'actual': actual[key]}
            if key not in actual:
                return {'path': child, 'expected': expected[key], 'actual': '<missing>'}
            mismatch = first_diff(expected[key], actual[key], child)
            if mismatch:
                return mismatch
        return None
    if isinstance(expected, list):
        for index, (left, right) in enumerate(zip(expected, actual)):
            mismatch = first_diff(left, right, f'{path}[{index}]')
            if mismatch:
                return mismatch
        if len(expected) != len(actual):
            index = min(len(expected), len(actual))
            return {
                'path': f'{path}[{index}]',
                'expected': expected[index] if index < len(expected) else '<missing>',
                'actual': actual[index] if index < len(actual) else '<missing>',
            }
        return None
    if expected != actual:
        return {'path': path, 'expected': expected, 'actual': actual}
    return None


def read_results(path):
    """Read per-example records; semantic truth is computed only with fixtures."""
    results = []
    malformed = []
    if not path.exists():
        return results, [{'line': 0, 'error': 'test runner did not create JSONL report'}]
    for line_number, line in enumerate(path.read_text(encoding='utf-8').splitlines(), 1):
        if not line.strip():
            continue
        try:
            item = json.loads(line)
            if not isinstance(item, dict) or 'example' not in item:
                raise ValueError('record must be an object with an example field')
            if isinstance(item['example'], bool):
                raise ValueError('example must be an integer')
            item['example'] = int(item['example'])
            # A per-case exception is a distinct failure record. Every ordinary
            # record must provide the CST-derived HTML stream; never trust a
            # precomputed semantic_match as a fallback.
            if not item.get('error') and not isinstance(item.get('projected_html'), str):
                raise ValueError('normal record must contain string projected_html')
            item['limitations'] = item.get('limitations') or []
            if isinstance(item['limitations'], str):
                item['limitations'] = [item['limitations']]
            results.append(item)
        except (ValueError, TypeError, json.JSONDecodeError) as error:
            malformed.append({'line': line_number, 'error': str(error), 'raw': line})
    return results, malformed


def case_result(example, fixture, result, skip_reason=None):
    """Bind report output to immutable fixture input and compute HTML semantics."""
    row = dict(result or {})
    row['reported_markdown'] = result.get('markdown') if result is not None else None
    row['reported_expected_html'] = result.get('expected_html') if result is not None else None
    row['fixture_input_match'] = (
        None if result is None else
        result.get('markdown') == fixture['markdown'] and
        result.get('expected_html') == fixture['html'])
    row['native_id'] = fixture.get('native_id', example)
    row['example'] = fixture['example']
    row['id'] = fixture_identity(fixture)
    row['corpus'] = fixture.get('corpus', 'commonmark')
    row['section'] = fixture['section']
    row['markdown'] = fixture['markdown']
    row['expected_html'] = fixture['html']
    row['source_markdown_raw'] = fixture.get('source_markdown_raw', fixture['markdown'])
    row['source_html_raw'] = fixture.get('source_html_raw', fixture['html'])
    row['fixture_transformations'] = fixture.get('fixture_transformations', [])
    row['expected'] = canonical_html(fixture['html'])
    row['actual'] = None
    row['serialized'] = row.get('serialized')
    row['inline_lossless'] = row.get('inline_lossless')
    row['exact_serialization'] = row.get('exact_serialization', row.pop('roundtrip', None))
    row['limitations'] = row.get('limitations') or []
    row['error'] = row.get('error')
    row['reported'] = result is not None
    row['semantic_skip_reason'] = None
    row['source_lines'] = fixture.get('source_lines')
    row['source_path'] = fixture.get('source_path')
    row['upstream_extensions'] = fixture.get('upstream_extensions', [])
    row['upstream_disabled'] = fixture.get('upstream_disabled', False)
    row['native_profile'] = fixture.get('native_profile')
    row['native_interface_gaps'] = fixture.get('native_interface_gaps', [])
    row['license'] = fixture.get('license')
    row['expected_html_ignore'] = fixture.get('expected_html_ignore', False)
    reference_difference = fixture.get('reviewed_reference_difference')
    row['reviewed_reference_difference'] = reference_difference
    row['reference_current_expected_html'] = (
        reference_difference.get('current_expected_html') if reference_difference else None)
    row['reference_difference_html_match'] = None
    row['reference_difference_verified'] = None
    row['semantic_match'] = None
    row['relaxed_list_match'] = None
    if result is not None and not row['error'] and isinstance(row.get('projected_html'), str):
        row['actual'] = canonical_html(row['projected_html'])
        row['semantic_match'] = row['expected'] == row['actual']
        row['relaxed_list_match'] = (
            canonical_html(fixture['html'], relax_list_paragraphs=True) ==
            canonical_html(row['projected_html'], relax_list_paragraphs=True))
        if reference_difference:
            row['reference_difference_html_match'] = (
                row['actual'] == canonical_html(reference_difference['current_expected_html']))
            row['reference_difference_verified'] = (
                row['reference_difference_html_match']
                and row.get('fixture_input_match') is True
                and row.get('inline_lossless') is True
                and row.get('exact_serialization') is True)
    if skip_reason and result is not None and not row['error']:
        row['semantic_skip_reason'] = skip_reason
        row['semantic_match'] = None
        row['relaxed_list_match'] = None
    row['first_tree_difference'] = first_diff(row['expected'], row['actual'])
    return row


def fixture_identity(fixture):
    """Return the public corpus-qualified label, including for CommonMark fixtures."""
    return fixture.get('id', f"commonmark:{fixture['example']}")


def label_native_examples(numbers, native_to_fixture):
    """Use fixture identities for known native IDs while retaining unknown IDs."""
    return [fixture_identity(native_to_fixture[number]) if number in native_to_fixture else number
            for number in numbers]


def calculate_formal_exit_code(native_exit_code, cases, missing, unexpected, duplicates,
                     malformed, filtered):
    """Combine native assertions with required semantic/report assertions."""
    if native_exit_code != 0:
        return native_exit_code
    semantic_failed = any(
        case.get('reported') and (
            case.get('error') or case.get('fixture_input_match') is False
            or case.get('inline_lossless') is not True
            or case.get('exact_serialization') is not True
            or (case.get('reviewed_reference_difference')
                and case.get('reference_difference_verified') is not True)
            or (case.get('semantic_match') is False
                and case.get('reference_difference_verified') is not True))
        for case in cases)
    reported_count = sum(bool(case.get('reported')) for case in cases)
    report_failed = bool(duplicates or malformed or unexpected or reported_count == 0)
    if not filtered:
        report_failed = report_failed or bool(missing)
    return 1 if semantic_failed or report_failed else 0


def summarize(cases):
    by_section = defaultdict(list)
    for case in cases:
        section = str(case['section'])
        key = f"{case['corpus']} / {section}" if case.get('corpus') else section
        by_section[key].append(case)
    section_counts = {}
    for section, items in sorted(by_section.items()):
        section_counts[section] = {
            'total': len(items),
            'reported': sum(item.get('reported', False) for item in items),
            'strict_pass': sum(item.get('semantic_match') is True for item in items),
            'semantic_skipped': sum(bool(item.get('semantic_skip_reason')) for item in items),
            'strict_diff': sum(item.get('semantic_match') is False for item in items),
            'unreported': sum(not item.get('reported') for item in items),
            'relaxed_list_only': sum(item.get('semantic_match') is False and
                                     item.get('relaxed_list_match') is True for item in items),
            'inline_lossless': sum(item.get('inline_lossless') is True for item in items),
            'inline_lossless_fail': sum(item.get('inline_lossless') is False for item in items),
            'exact_serialization': sum(item.get('exact_serialization') is True for item in items),
            'exact_serialization_fail': sum(item.get('exact_serialization') is False for item in items),
            'reference_difference_verified': sum(
                item.get('reference_difference_verified') is True for item in items),
            'unresolved_diff': sum(
                bool(item.get('semantic_match') is False or
                     (item.get('reviewed_reference_difference') and item.get('reported')))
                and item.get('reference_difference_verified') is not True for item in items),
        }
    all_results = [case for items in by_section.values() for case in items]
    limitations = Counter(limit for case in all_results for limit in case.get('limitations', []))
    strict_diffs = [case for case in all_results if case.get('semantic_match') is False]
    difference_categories = {
        'only_list_tightness': [],
        'html_policy_or_projection': [],
        'reviewed_reference_difference': [],
        'other_reported_projection_limit': [],
        'no_known_projection_limit': [],
    }
    for case in strict_diffs:
        if case.get('reference_difference_verified') is True:
            case['difference_category'] = 'reviewed_reference_difference'
            difference_categories['reviewed_reference_difference'].append(
                case.get('id', case['example']))
            continue
        labels = [str(label).casefold() for label in case.get('limitations', [])]
        has_html_policy_label = any('html-policy/projection' in label for label in labels)
        other_labels = [label for label in labels if 'list-tightness' not in label]
        if case.get('relaxed_list_match') is True and not has_html_policy_label and not other_labels:
            category = 'only_list_tightness'
        elif has_html_policy_label:
            category = 'html_policy_or_projection'
        elif labels:
            category = 'other_reported_projection_limit'
        else:
            category = 'no_known_projection_limit'
        case['difference_category'] = category
        difference_categories[category].append(case.get('id', case['example']))
    return {
        'sections': section_counts,
        'totals': {
            'fixture_rows': len(all_results),
            'reported': sum(case.get('reported', False) for case in all_results),
            'strict_pass': sum(case.get('semantic_match') is True for case in all_results),
            'semantic_skipped': sum(bool(case.get('semantic_skip_reason')) for case in all_results),
            'strict_diff': len(strict_diffs),
            'unreported': sum(not case.get('reported') for case in all_results),
            'relaxed_list_only': sum(case.get('semantic_match') is False and
                                     case.get('relaxed_list_match') is True for case in all_results),
            'inline_lossless': sum(case.get('inline_lossless') is True for case in all_results),
            'inline_lossless_fail': sum(case.get('inline_lossless') is False for case in all_results),
            'exact_serialization': sum(case.get('exact_serialization') is True for case in all_results),
            'exact_serialization_fail': sum(case.get('exact_serialization') is False for case in all_results),
            'fixture_input_mismatch': sum(case.get('fixture_input_match') is False for case in all_results),
            'reference_difference_reported': sum(
                case.get('reviewed_reference_difference') is not None and case.get('reported')
                for case in all_results),
            'reference_difference_verified': sum(
                case.get('reference_difference_verified') is True for case in all_results),
            'unresolved_diff': sum(
                bool(case.get('semantic_match') is False or
                     (case.get('reviewed_reference_difference') and case.get('reported')))
                and case.get('reference_difference_verified') is not True
                for case in all_results),
            'with_reported_limitations': sum(bool(case.get('limitations')) for case in all_results),
        },
        'limitations': dict(sorted(limitations.items())),
        'difference_categories': {
            category: {'count': len(examples), 'examples': examples}
            for category, examples in difference_categories.items()
        },
        'worst_sections': sorted(section_counts.items(),
                                 key=lambda entry: (-entry[1]['strict_diff'], entry[0]))[:10],
        'strict_differences': strict_diffs,
    }


def fenced(value, language='json'):
    value = str(value)
    longest = max((len(run) for run in re.findall(r'`+', value)), default=0)
    fence = '`' * max(3, longest + 1)
    return f'{fence}{language}\n{value}\n{fence}'


def markdown_report(report):
    totals = report['summary']['totals']
    lines = [
        f"# {report.get('suite_title', 'CommonMark 0.31.2')} conformance results",
        '',
        f"Status: **{report['status']}**. Formal test exit code: **{report.get('exit_code')}**.",
        f"Fixture source and provenance: `{json.dumps(report['source'], ensure_ascii=False)}`.",
        '',
        'The formal test canonicalizes C++ projected HTML and immutable official expected HTML with the same Python HTMLParser. This checks CommonMark semantic output and is not Folia’s WinUI HTML renderer. Native assertions separately check inline source losslessness and exact Markdown save serialization.',
        '',
        '## Totals',
        '',
        f"- Fixture cases: {report['fixture_count']}; records: {totals['reported']}; missing: {len(report['missing_examples'])}; malformed JSONL lines: {len(report['malformed_records'])}.",
        f"- Upstream-disabled cases retained and run: {report.get('upstream_disabled_fixture_count', 0)}.",
        "- Cases by corpus: " + ", ".join(
            f"{name} {count}" for name, count in report.get('fixture_counts_by_corpus', {}).items()) + ".",
        f"- Strict semantic matches: {totals['strict_pass']}; strict differences: {totals['strict_diff']}; explicit HTML skips: {totals['semantic_skipped']}; unreported: {totals['unreported']}.",
        f"- Reviewed current-reference differences: {totals['reference_difference_verified']} verified; {totals['unresolved_diff']} unresolved strict/reference differences.",
        f"- List tight/loose diagnostic only (strict fail, relaxed list comparison passes): {totals['relaxed_list_only']}.",
        f"- Inline source lossless: {totals['inline_lossless']} pass, {totals['inline_lossless_fail']} fail; exact Markdown save serialization: {totals['exact_serialization']} pass, {totals['exact_serialization_fail']} fail.",
        f"- Native-reported Markdown/expected HTML matched fixture bytes: {totals['reported'] - totals['fixture_input_mismatch']} pass, {totals['fixture_input_mismatch']} mismatch.",
        f"- Cases with one or more reported projection/implementation limitations: {totals['with_reported_limitations']}.",
        '',
        '## Sections',
        '',
        '| Section | Cases | Strict pass | Strict differences | Relaxed-list-only | Lossless fail | Exact serialization fail |',
        '|---|---:|---:|---:|---:|---:|---:|',
    ]
    for section, counts in report['summary']['sections'].items():
        lines.append(f"| {section} | {counts['total']} | {counts['strict_pass']} | {counts['strict_diff']} | {counts['relaxed_list_only']} | {counts['inline_lossless_fail']} | {counts['exact_serialization_fail']} |")
    lines.extend(['', '## Explicit HTML presentation skips', ''])
    skipped = [case for case in report['results'] if case.get('semantic_skip_reason')]
    if not skipped:
        lines.append('None.')
    for case in skipped:
        lines.append(f"- {case.get('id', case['example'])} ({case['section']}): {case['semantic_skip_reason']}. Native source/CST/save assertions still run.")
    lines.extend(['', '## Difference categories', ''])
    if report['summary']['limitations']:
        lines.extend(['Reported limitations are counts of case-level limitation labels:'])
        for label, count in report['summary']['limitations'].items():
            lines.append(f'- `{label}`: {count}')
    else:
        lines.append('The formal test reported no per-case limitation labels.')
    gaps = report.get('native_interface_gaps', {})
    lines.extend(['', '## Missing native output options', ''])
    if gaps:
        for name, count in gaps.items():
            lines.append(f'- {name}: {count} fixture cases retain and compare the pinned upstream expectation.')
    else:
        lines.append('None.')
    lines.extend(['', '## Sections with most strict differences', ''])
    worst = report['summary']['worst_sections']
    if worst:
        for section, counts in worst:
            lines.append(f"- **{section}**: {counts['strict_diff']} strict differences / {counts['total']} cases; {counts['relaxed_list_only']} pass only under the list diagnostic.")
    else:
        lines.append('No section results were reported.')
    lines.extend(['', '## Mutually exclusive strict-difference categories', ''])
    for category, details in report['summary']['difference_categories'].items():
        lines.append(f"- **{category}**: {details['count']} — examples: {', '.join(map(str, details['examples'])) or 'none'}.")
    lines.extend(['', '## Strict differences', ''])
    diffs = report['summary']['strict_differences']
    if not diffs:
        lines.append('No strict differences were reported.')
    for case in diffs:
        diff = case.get('first_tree_difference') or {}
        lines.extend([
            f"### {case.get('id', case['example'])} — {case['section']}",
            '',
            '**Markdown source:**',
            '',
            fenced(json.dumps(case.get('markdown'), ensure_ascii=False), 'json'),
            '',
            f"**First tree difference:** `{diff.get('path', 'unknown')}`",
            '',
            '**Expected:**',
            '',
            fenced(json.dumps(diff.get('expected', case.get('expected')), ensure_ascii=False)),
            '',
            '**Actual:**',
            '',
            fenced(json.dumps(diff.get('actual', case.get('actual')), ensure_ascii=False)),
        ])
        if case.get('limitations'):
            lines.append('Reported limitations: ' + ', '.join(f'`{x}`' for x in case['limitations']))
        lines.append('')
    quality_failures = [case for case in report['results']
                        if case.get('inline_lossless') is False or case.get('exact_serialization') is False]
    incomplete = [case for case in report['results'] if case.get('error')]
    lines.extend(['', '## Source-preservation failures and case errors', ''])
    if not quality_failures and not incomplete:
        lines.append('No source-preservation failures or per-case errors were reported.')
    for case in quality_failures:
        failed = []
        if case.get('inline_lossless') is False:
            failed.append('inline source lossless')
        if case.get('exact_serialization') is False:
            failed.append('exact Markdown save serialization')
        lines.append(f"- {case.get('id', case['example'])} ({case['section']}): {', '.join(failed)}.")
    for case in incomplete:
        lines.extend([
            f"### {case.get('id', case['example'])} — {case['section']} raised an error",
            '',
            f"Error: {case['error']}",
            '',
            '**Markdown source:**',
            '',
            fenced(json.dumps(case.get('markdown'), ensure_ascii=False), 'json'),
            '',
        ])
    lines.extend(['Full per-example records are in `results.json`; raw test output is in `runner.log`.'])
    return '\n'.join(lines) + '\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--suite', choices=['commonmark', 'gfm'], default='commonmark')
    parser.add_argument('--executable', type=Path)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--timeout', type=float, default=120.0)
    args, filters = parser.parse_known_args()
    if args.executable is None:
        target = 'FoliaGfmTests.exe' if args.suite == 'gfm' else 'FoliaCommonMarkTests.exe'
        args.executable = ROOT / 'build' / 'core' / target
    if args.output is None:
        args.output = ROOT / 'build' / 'gfm-conformance' if args.suite == 'gfm' else DEFAULT_OUTPUT
    if args.timeout <= 0:
        parser.error('--timeout must be greater than zero')

    fixtures = load_fixtures(args.suite)
    policy = {}
    if args.suite == 'gfm':
        policy = json.loads((HERE / 'gfm-policy.json').read_text(encoding='utf-8'))['semantic_skips']
        known = {case['id']: case for case in fixtures}
        for example, entry in policy.items():
            if example not in known or known[example]['section'] != entry['section'] or not entry['reason']:
                raise ValueError('Invalid GFM semantic skip policy: ' + example)
    args.output.mkdir(parents=True, exist_ok=True)
    jsonl_path = args.output / 'results.jsonl'
    report_path = args.output / 'results.json'
    markdown_path = args.output / 'results.md'
    log_path = args.output / 'runner.log'
    jsonl_path.unlink(missing_ok=True)

    env = os.environ.copy()
    env['FOLIA_MARKDOWN_REPORT'] = str(jsonl_path.resolve())
    command = [str(args.executable.resolve()), *filters]
    started = time.time()
    stdout = stderr = ''
    exit_code = None
    timed_out = False
    launch_error = None
    try:
        completed = subprocess.run(command, cwd=ROOT, env=env, text=True,
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                   timeout=args.timeout, check=False)
        stdout, stderr = completed.stdout, completed.stderr
        exit_code = completed.returncode
    except subprocess.TimeoutExpired as error:
        timed_out = True
        stdout = error.stdout.decode(errors='replace') if isinstance(error.stdout, bytes) else (error.stdout or '')
        stderr = error.stderr.decode(errors='replace') if isinstance(error.stderr, bytes) else (error.stderr or '')
    except OSError as error:
        launch_error = str(error)
    elapsed = time.time() - started
    log_path.write_text('COMMAND: ' + subprocess.list2cmdline(command) + '\n\nSTDOUT:\n' + stdout + '\nSTDERR:\n' + stderr, encoding='utf-8')
    if stdout:
        print(stdout, end='' if stdout.endswith('\n') else '\n')
    if stderr:
        print(stderr, file=sys.stderr, end='' if stderr.endswith('\n') else '\n')

    raw, malformed = read_results(jsonl_path)
    indexed = {}
    duplicates = []
    for item in raw:
        number = item['example']
        if number in indexed:
            duplicates.append(number)
        indexed[number] = item
    native_to_fixture = {
        case.get('native_id', case['example']): case for case in fixtures
    }
    all_cases = [
        case_result(
            case.get('native_id', case['example']),
            case,
            indexed.get(case.get('native_id', case['example'])),
            policy.get(case.get('id', ''), {}).get('reason'))
        for case in fixtures]
    present = set(indexed)
    expected_ids = set(native_to_fixture)
    missing_native = sorted(expected_ids - present)
    unexpected_native = sorted(present - expected_ids)
    missing = label_native_examples(missing_native, native_to_fixture)
    unexpected = label_native_examples(unexpected_native, native_to_fixture)
    duplicate_ids = label_native_examples(duplicates, native_to_fixture)
    semantic_failed = any(
        case.get('reported') and (
            case.get('error') or case.get('fixture_input_match') is False
            or case.get('inline_lossless') is not True
            or case.get('exact_serialization') is not True
            or (case.get('reviewed_reference_difference')
                and case.get('reference_difference_verified') is not True)
            or (case.get('semantic_match') is False
                and case.get('reference_difference_verified') is not True))
        for case in all_cases)
    formal_exit_code = calculate_formal_exit_code(
        exit_code, all_cases, missing, unexpected, duplicate_ids, malformed, bool(filters))
    status = ('completed' if exit_code in (0, 1) and not timed_out and not launch_error
              else 'timed_out' if timed_out else 'abnormal_exit' if exit_code is not None
              else 'launch_error')

    lock = json.loads((HERE / ('gfm-sources.lock.json' if args.suite == 'gfm' else 'sources.lock.json')).read_text(encoding='utf-8'))
    spec = next((item for item in lock.get('files', []) if item['name'] == 'spec.json'), None)
    report = {
        'status': status,
        'exit_code': formal_exit_code,
        'native_exit_code': exit_code,
        'elapsed_seconds': round(elapsed, 3),
        'timeout_seconds': args.timeout,
        'filtered': bool(filters),
        'filters': filters,
        'fixture_count': len(fixtures),
        'fixture_counts_by_corpus': dict(sorted(Counter(
            case.get('corpus', 'commonmark') for case in fixtures).items())),
        'upstream_disabled_fixture_count': sum(
            case.get('upstream_disabled', False) for case in fixtures),
        'native_interface_gaps': dict(sorted(Counter(
            gap for case in fixtures for gap in case.get('native_interface_gaps', [])).items())),
        'jsonl_record_count': len(raw),
        'suite_title': 'GFM published page and pinned cmark-gfm corpora' if args.suite == 'gfm' else 'CommonMark 0.31.2',
        'source': lock if args.suite == 'gfm' else {
            'commonmark_version': lock['commonmark_version'],
            'spec_json_url': spec['url'],
            'spec_json_sha256': spec['sha256'],
            'commonmark_license': lock['commonmark_license'],
            'md4c_commit': lock['md4c_commit'],
            'md4c_license': lock['md4c_license'],
        },
        'comparison_scope': 'C++ projected HTML semantics canonicalized with the same Python HTMLParser used for official HTML; not the Folia WinUI HTML renderer.',
        'missing_examples': missing,
        'unexpected_examples': unexpected,
        'duplicate_examples': sorted(set(map(str, duplicate_ids))),
        'malformed_records': malformed,
        'launch_error': launch_error,
        'summary': summarize(all_cases),
        'results': all_cases,
    }
    report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    markdown_path.write_text(markdown_report(report), encoding='utf-8')
    print(f"Markdown conformance report: {markdown_path}")
    print(f"JSON report: {report_path}")

    if timed_out:
        print(f'Formal test timed out after {args.timeout:g}s; partial results are recorded.', file=sys.stderr)
        return 124
    if launch_error:
        print(f'Could not launch formal test executable: {launch_error}', file=sys.stderr)
        return 2
    if exit_code not in (0, 1):
        print(f'Formal test exited abnormally with code {exit_code}; partial results are recorded.', file=sys.stderr)
        return exit_code if exit_code is not None else 2
    if exit_code == 0 and not filters and (missing or unexpected or duplicates or malformed):
        print('Formal test returned success but its per-example report is incomplete or malformed.', file=sys.stderr)
        return 1
    if exit_code == 0 and semantic_failed:
        print('Formal test found CommonMark HTML semantic differences; details are recorded in the report.', file=sys.stderr)
        return 1
    return formal_exit_code


if __name__ == '__main__':
    raise SystemExit(main())
