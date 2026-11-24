#!/usr/bin/env python3
"""Capture a completed native build without a JavaScript test dependency.

FOLIA_NATIVE_TEST_DIR selects the native executable directory (default build/core).
"""
import os
from pathlib import Path
import re
import sys

from validation.common import emit, require
from validation.processes import capture, pin


def main(args):
    require(len(args) >= 3, 'Gate, target and output required')
    gate, target, output, *native_args = args
    require(re.fullmatch(r'Folia[A-Za-z][A-Za-z0-9]*', target) is not None, 'Invalid native target')
    input_file = os.environ.get('MATH_OBSERVATION_STDIN')
    extra = {'runner': pin(Path(__file__).resolve())}
    if input_file:
        extra['stdin'] = pin(input_file)
    record, status = capture(int(gate), target, output, native_args,
                            input_bytes=Path(input_file).read_bytes() if input_file else b'',
                            timeout_ms=int(os.environ.get('MATH_OBSERVATION_TIMEOUT_MS', '60000')),
                            output_format=os.environ.get('MATH_OBSERVATION_FORMAT', 'json'), extra=extra)
    emit(record)
    return status


if __name__ == '__main__':
    raise SystemExit(main(sys.argv[1:]))
