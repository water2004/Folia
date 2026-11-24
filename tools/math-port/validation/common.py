"""Strict reference-data checks shared by the native math test tools.

Node is used only when explicitly regenerating a pinned MathJax oracle. Native
execution, comparisons, source locks, and reports belong to Python.
"""
from __future__ import annotations

import gzip
import hashlib
import json
import math
import os
from pathlib import Path
import subprocess
from typing import Any

ROOT = Path(__file__).resolve().parents[3]
UPSTREAM_COMMIT = 'fb987178c2d279a99b0db5ea02e751691703955e'
MISSING = object()


class ComparisonError(RuntimeError):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ComparisonError(message)


def resolve(value: str | Path) -> Path:
    return Path(value).resolve()


def default_path(args: list[str], index: int, default: str) -> Path:
    return resolve(args[index]) if len(args) > index else ROOT / default


def root_path(args: list[str], index: int, default: str) -> Path:
    return (ROOT / (args[index] if len(args) > index else default)).resolve()


def digest(path: str | Path) -> str:
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def _invalid_constant(value: str) -> None:
    raise ComparisonError(f'Non-JSON numeric constant: {value}')


def loads(text: str | bytes) -> Any:
    # JSON.parse uses IEEE-754 numbers, including signed zero. Preserve that
    # contract: bool is distinct from number, and -0 is distinct from +0.
    return json.loads(text, parse_int=float, parse_float=float, parse_constant=_invalid_constant)


def read(path: str | Path) -> Any:
    file = Path(path)
    data = file.read_bytes()
    if file.suffix == '.gz':
        data = gzip.decompress(data)
    return loads(data.decode('utf-8'))


def js_type(value: Any) -> str:
    if value is MISSING:
        return 'undefined'
    if value is None or isinstance(value, (dict, list)):
        return 'object'
    if isinstance(value, bool):
        return 'boolean'
    if isinstance(value, (int, float)):
        return 'number'
    if isinstance(value, str):
        return 'string'
    raise TypeError(type(value))


def same_scalar(expected: Any, actual: Any) -> bool:
    if js_type(expected) != js_type(actual):
        return False
    if isinstance(expected, (dict, list)):
        return expected is actual
    if js_type(expected) == 'number':
        if expected == actual == 0:
            return math.copysign(1, expected) == math.copysign(1, actual)
        return expected == actual or math.isnan(expected) and math.isnan(actual)
    return expected == actual


def truthy(value: Any) -> bool:
    if value is MISSING or value is None:
        return False
    if isinstance(value, (dict, list)):
        return True
    if isinstance(value, (int, float)) and not isinstance(value, bool):
        return value != 0 and not math.isnan(value)
    return bool(value)


def shown(value: Any) -> Any:
    return '<missing>' if value is MISSING else value


def keys(value: Any) -> list[str]:
    require(isinstance(value, dict), f'Expected JSON object, found {js_type(value)}')
    # Object.keys lists canonical array-index names before ordinary own keys,
    # even if those names appeared later in the serialized JSON object.
    indexes = [key for key in value if key.isascii() and key.isdecimal()
               and str(int(key)) == key and int(key) < 2**32 - 1]
    return sorted(indexes, key=int) + [key for key in value if key not in indexes]


def sorted_keys(value: dict[str, Any]) -> list[str]:
    return sorted(value, key=lambda key: key.encode('utf-16-be', errors='surrogatepass'))


def difference(expected: Any, actual: Any, at: str = '$', *, ordered: bool = False) -> dict | None:
    if same_scalar(expected, actual):
        return None
    if (js_type(expected) != js_type(actual) or expected is None or actual is None
            or not isinstance(expected, (dict, list)) or not isinstance(actual, (dict, list))):
        return {'path': at, 'source': shown(expected), 'native': shown(actual)}
    if isinstance(expected, list) or isinstance(actual, list):
        if not isinstance(expected, list) or not isinstance(actual, list) or len(expected) != len(actual):
            return {'path': at + '.length',
                    'source': len(expected) if isinstance(expected, list) else js_type(expected),
                    'native': len(actual) if isinstance(actual, list) else js_type(actual)}
        for i, (left, right) in enumerate(zip(expected, actual)):
            found = difference(left, right, f'{at}[{i}]', ordered=ordered)
            if found:
                return found
        return None
    expected_keys = keys(expected) if ordered else sorted_keys(expected)
    actual_keys = keys(actual) if ordered else sorted_keys(actual)
    if expected_keys != actual_keys:
        return {'path': at + '.keys', 'source': expected_keys, 'native': actual_keys}
    for key in expected_keys:
        found = difference(expected[key], actual[key], f'{at}.{key}', ordered=ordered)
        if found:
            return found
    return None


def equal(expected: Any, actual: Any, label: str = '$', *, ordered: bool = False) -> None:
    found = difference(expected, actual, label, ordered=ordered)
    require(found is None, json.dumps(found, ensure_ascii=True))


def field(value: Any, name: str) -> Any:
    return value.get(name, MISSING) if isinstance(value, dict) else MISSING


def write(path: str | Path, value: Any, *, parents: bool = False) -> None:
    target = Path(path)
    if parents:
        target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps(value, ensure_ascii=True, indent=2) + '\n', encoding='utf-8')


def emit(value: Any) -> None:
    print(json.dumps(value, ensure_ascii=True, separators=(',', ':')))


def oracle(script: str, args: list[str] | None = None) -> Any:
    # The invoked program only captures the upstream MathJax reference data.
    result = subprocess.run([os.environ.get('FOLIA_NODE_EXECUTABLE', 'node'), str(ROOT / script), *(args or [])],
                            cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True)
    return loads(result.stdout)


def records_report(source: list, native: list, field_name: str = 'cases') -> tuple[int, list]:
    failures = []
    if len(source) != len(native):
        failures.append({'path': field_name + '.length', 'source': len(source), 'native': len(native)})
    matched = 0
    for i, row in enumerate(source):
        found = difference(row, native[i] if i < len(native) else MISSING,
                           f'{field_name}[{i}]({row.get("name", "")})')
        if found:
            failures.append(found)
        else:
            matched += 1
    return matched, failures
