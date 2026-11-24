"""Read-only validation of the retained method-scoped native fault plan."""
from dataclasses import dataclass
import hashlib
from pathlib import Path

from ..common import equal, read, require

WITNESSES = ('parser-array-host-reference.cpp', 'parser-iterator-reference.cpp')


def sha256(value):
    return hashlib.sha256(value).hexdigest()


@dataclass(frozen=True)
class Original:
    path: Path
    data: bytes
    text: str


def method_range(text, method):
    """Original C++ brace scanner, preserving quoted/comment/escaped braces."""
    name = method.split(' ')[0]
    start = text.find(name + '(')
    require(start >= 0, method)
    require(text.find(name + '(', start + 1) == -1, method + ' must identify one definition')
    index = text.find('{', start)
    require(index >= 0, 'Unterminated method ' + method)
    begin, depth, state = index, 0, 'code'
    while index < len(text):
        char = text[index]
        next_char = text[index + 1] if index + 1 < len(text) else ''
        if state == 'line':
            if char == '\n':
                state = 'code'
        elif state == 'comment':
            if char == '*' and next_char == '/':
                state = 'code'
                index += 1
        elif state in {'string', 'char'}:
            if char == '\\':
                index += 1
            elif char == ('"' if state == 'string' else "'"):
                state = 'code'
        elif char == '/' and next_char == '/':
            state = 'line'
            index += 1
        elif char == '/' and next_char == '*':
            state = 'comment'
            index += 1
        elif char == '"':
            state = 'string'
        elif char == "'":
            state = 'char'
        elif char == '{':
            depth += 1
        elif char == '}':
            depth -= 1
            if depth == 0:
                return begin, index + 1
        index += 1
    require(False, 'Unterminated method ' + method)


def replacement(row, original, candidate):
    begin, end = method_range(original.text, row['method'])
    body = original.text[begin:end]
    newline = '\r\n' if '\r\n' in original.text else '\n'
    anchor = row['anchor'].replace('\n', newline)
    changed = row['replacement'].replace('\n', newline)
    equal(2, len(body.split(anchor)), row['id'] + ' exact method anchor')
    equal(row['restoreSha256'].lower(), sha256(original.data), row['id'] + ' restoration hash')
    require('"' + row['nativeCase'] + '"_test' in candidate, row['id'] + ' named native witness')
    return (original.text[:begin] + body.replace(anchor, changed, 1) + original.text[end:]).encode('utf-8')


def preflight(root, plan_path=None):
    root = Path(root).resolve()
    tools = root / 'tools/math-port'
    plan_file = Path(plan_path or tools / 'parser-host-fault-plan.json')
    plan = read(plan_file)
    originals = {}
    for file, expected in plan['baselineSha256'].items():
        path = (root / file).resolve()
        require(path.is_relative_to(root) and path != root, 'Fault source must remain in the selected checkout')
        data = path.read_bytes()
        equal(expected.lower(), sha256(data), file + ' baseline')
        originals[file] = Original(path, data, data.decode('utf-8'))
    candidate = '\n'.join((tools / file).read_bytes().decode('utf-8') for file in WITNESSES)
    mutations = []
    for row in plan['rows']:
        require(row['source'] in originals, row['source'] + ' not pinned in fault plan')
        mutations.append(dict(row, changed=replacement(row, originals[row['source']], candidate)))
    gold = [(file, file.read_bytes()) for file in [plan_file, *(tools / file for file in WITNESSES)]]
    return plan, originals, mutations, gold
