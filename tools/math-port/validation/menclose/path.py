"""Exact base-ten SVG path and transform normalization for menclose probes."""
from decimal import Decimal
from fractions import Fraction
import math
import re

from ..common import require

# ECMAScript WhiteSpace + LineTerminator (the original tokenizer's \s).
SPACE = r'[\t\n\v\f\r \u00a0\u1680\u2000-\u200a\u2028\u2029\u202f\u205f\u3000\ufeff]'
NUMBER = r'[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][+-]?[0-9]+)?'
TOKEN = re.compile(r'[A-Za-z]|[-+]?(?:[0-9]*\.[0-9]+|[0-9]+\.?[0-9]*)(?:[eE][-+]?[0-9]+)?')
COMMAND = re.compile(r'[A-Za-z]')


def decimal(token, context='number'):
    require(isinstance(token, str) and re.fullmatch(NUMBER, token) is not None,
            f'{context}: non-finite or invalid number {token}')
    parts = token.lower().split('e')
    exponent = int(parts[1]) if len(parts) == 2 else 0
    require(abs(exponent) <= 10000, f'{context}: invalid exponent {token}')
    unsigned = parts[0].lstrip('+-')
    whole, _, fraction = unsigned.partition('.')
    numerator = int(Decimal((whole or '0') + fraction))
    if parts[0].startswith('-'):
        numerator = -numerator
    scale = len(fraction) - exponent
    return Fraction(numerator, 10 ** scale) if scale >= 0 else Fraction(numerator * 10 ** (-scale))


def key(value):
    return format(Decimal(value.numerator), 'f') + '/' + format(Decimal(value.denominator), 'f')


def finite_number(token, context):
    try:
        finite = math.isfinite(float(token))
    except (TypeError, ValueError, OverflowError):
        finite = False
    require(finite, f'{context}: outside finite SVG number range {token}')
    return decimal(token, context)


def tokenize(data):
    tokens, offset, previous, separated = [], 0, 'start', False
    while offset < len(data):
        if re.fullmatch(SPACE, data[offset]):
            offset, separated = offset + 1, True
            continue
        if data[offset] == ',':
            require(previous == 'number', f'Unexpected comma at {offset} in path data: {data}')
            offset += 1
            while offset < len(data) and re.fullmatch(SPACE, data[offset]):
                offset += 1
            require(offset < len(data) and not re.fullmatch('[A-Za-z,]', data[offset]),
                    f'Comma must separate numeric parameters in path data: {data}')
            previous, separated = 'comma', True
            continue
        match = TOKEN.match(data, offset)
        require(match is not None, f'Path data contains an invalid character at {offset}: {data[offset:offset + 16]}')
        value = match[0]
        command = COMMAND.fullmatch(value) is not None
        require(not command or previous != 'comma', f'Comma cannot precede a path command: {data}')
        require(command or previous != 'number' or separated, f'Numeric parameters require a separator: {data}')
        tokens.append(value)
        previous, separated, offset = ('command' if command else 'number'), False, match.end()
    require(previous != 'comma', f'Path data cannot end with a comma: {data}')
    return tokens


def source_path(data):
    tokens, result = tokenize(data), []
    index, command, x, y, first = 0, '', Fraction(0), Fraction(0), True
    while index < len(tokens):
        if COMMAND.fullmatch(tokens[index]):
            command, index = tokens[index], index + 1
        else:
            require(bool(command), f'Source path starts without command: {data}')
        require(command in 'MmLlAaZz', f'Unsupported Source path command {command}')
        require(not first or command.upper() == 'M', 'Source path must start with M')
        first = False
        if command.upper() == 'Z':
            result.append(['Z'])
            command = ''
            continue
        relative, upper, start = command.islower(), command.upper(), index
        while index < len(tokens) and not COMMAND.fullmatch(tokens[index]):
            arity = 2 if upper in ['M', 'L'] else 7
            require(index + arity <= len(tokens) and not any(COMMAND.fullmatch(token) for token in tokens[index:index + arity]),
                    f'{upper} requires ' + ('pairs' if arity == 2 else 'seven parameters'))
            if arity == 2:
                nx, ny = decimal(tokens[index]), decimal(tokens[index + 1])
                index += 2
                if relative:
                    nx, ny = x + nx, y + ny
                x, y = nx, ny
                result.append(['M' if upper == 'M' and index - start == 2 else 'L', key(x / 1000), key(y / 1000)])
                if upper == 'M':
                    command = 'l' if relative else 'L'
            else:
                rx, ry, rotation = map(decimal, tokens[index:index + 3])
                large, sweep = tokens[index + 3:index + 5]
                require(large in ['0', '1'] and sweep in ['0', '1'], 'Invalid Source arc flag')
                nx, ny = decimal(tokens[index + 5]), decimal(tokens[index + 6])
                index += 7
                if relative:
                    nx, ny = x + nx, y + ny
                x, y = nx, ny
                result.append(['A', key(rx / 1000), key(ry / 1000), key(rotation), large == '1', sweep == '1', key(x / 1000), key(y / 1000)])
        require(index != start, f'{upper} requires coordinates' if upper in ['M', 'L'] else 'A requires parameters')
    return result


def native_path(data):
    for token in tokenize(data):
        if not COMMAND.fullmatch(token):
            finite_number(token, 'native path')
    result = source_path(data)
    for segment in result:
        require(segment[0] != 'A' or Fraction(segment[1]) >= 0 and Fraction(segment[2]) >= 0,
                'A arc radii must be nonnegative')
    return result


def transform_ops(value):
    if value is None:
        return []
    result = []
    pattern = re.compile(r'(translate|rotate)\(([^)]*)\)')
    for match in pattern.finditer(value):
        raw = re.split(r'[ ,]+', re.sub('^' + SPACE + '+|' + SPACE + '+$', '', match[2]))
        numbers = [finite_number(token, f'{match[1]} parameter {index}') for index, token in enumerate(raw)]
        if match[1] == 'translate':
            require(len(numbers) == 2, f'Translate requires exactly two parameters: {value}')
            result.append(['T', key(numbers[0] / 1000), key(numbers[1] / 1000)])
        else:
            require(len(numbers) == 3, f'Rotate requires exactly three parameters: {value}')
            result.append(['R', key(numbers[0]), key(numbers[1] / 1000), key(numbers[2] / 1000)])
    require(re.fullmatch(SPACE + '*', pattern.sub('', value)) is not None, f'Unsupported native transform: {value}')
    return result
