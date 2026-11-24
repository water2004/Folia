"""Shared exact profile75 validation for the historical and clean-session tools."""
from pathlib import Path

from ..common import digest, emit, read, require, write
from .metadata import validate, validate_boundaries
from .records import compare_records


def main(args, *, clean=False):
    require(len(args) >= 2, 'Expected source JSON, native JSON [summary JSON]')
    source_path, native_path = Path(args[0]).resolve(), Path(args[1]).resolve()
    destination = Path(args[2] if len(args) > 2 else 'build/math-port/profile75-full-svg-comparison.json').resolve()
    source, native = read(source_path), read(native_path)
    cases = validate(source, native)
    if clean:
        validate_boundaries(source, native)
    matched, failures = compare_records(cases, source, native)
    summary = {'sourceCommit': source['source']['commit'], 'casesPerGroup': 75, 'total': 150,
               'matched': len(matched), 'accepted': len(matched) == 150, 'matchedCases': matched, 'failures': failures,
               'sourceInputSha256': digest(source_path), 'nativeInputSha256': digest(native_path)}
    write(destination, summary, parents=True)
    emit({key: summary[key] for key in ['casesPerGroup', 'total', 'matched', 'accepted', 'failures']})
    return int(not summary['accepted'])
