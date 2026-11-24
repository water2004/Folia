"""Tiny process fixture: exercise protocol failure paths, never render samples."""

import json
import sys
import time

mode = sys.argv[1]
if mode == "timeout":
    time.sleep(10)
elif mode == "missing":
    sys.exit(0)
elif mode == "malformed":
    print("not JSON", flush=True)
elif mode == "abnormal":
    print(json.dumps({"received": True}), flush=True)
    sys.exit(7)
else:
    for line in sys.stdin:
        print(json.dumps(json.loads(line)), flush=True)
