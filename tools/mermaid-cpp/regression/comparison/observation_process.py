"""Bounded JSON-line processes and provenance-bound oracle sample capture."""

from __future__ import annotations

import json
import queue
import re
import subprocess
import threading
from pathlib import Path

from .corpus import digest, require


def encoded(value):
    return json.dumps(value, ensure_ascii=True, separators=(",", ":"), allow_nan=False)


class JsonProcess:
    """A portable pipe reader with a timeout, normal-exit checks and cleanup."""

    def __init__(self, command, *, timeout=30, stderr=None):
        self.command, self.timeout, self.stderr = command, timeout, stderr

    def __enter__(self):
        self.process = subprocess.Popen(self.command, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                        stderr=self.stderr, text=True, encoding="utf-8", bufsize=1)
        self.lines = queue.Queue()

        def consume():
            try:
                for line in self.process.stdout:
                    self.lines.put(line)
            finally:
                self.lines.put(None)

        self.reader = threading.Thread(target=consume, daemon=True)
        self.reader.start()
        return self

    def write(self, value):
        line = encoded(value)
        self.process.stdin.write(line + "\n")
        self.process.stdin.flush()
        return digest(line)

    def read(self):
        try:
            line = self.lines.get(timeout=self.timeout)
        except queue.Empty as error:
            raise ValueError("Timed out waiting for JSON observation: " + str(self.command[0])) from error
        require(line is not None, "Observation process terminated before responding")
        value = json.loads(line)
        require(isinstance(value, dict), "Observation response must be an object")
        return value

    def __exit__(self, kind, value, traceback):
        if kind is not None and self.process.poll() is None:
            self.process.kill()
        if not self.process.stdin.closed:
            try:
                self.process.stdin.close()
            except BrokenPipeError:
                pass
        try:
            code = self.process.wait(timeout=self.timeout)
        except subprocess.TimeoutExpired:
            self.process.kill()
            self.process.wait()
            if kind is None:
                raise ValueError("Observation process did not exit normally")
        else:
            if kind is None:
                require(code == 0, f"Observation process failed with exit status {code}")
        finally:
            self.process.stdout.close()
            self.reader.join(timeout=1)


class SampleCapture:
    """Invoke only a sample generator; all decisions remain in Python."""

    def __init__(self, script, directory, *, node="node", timeout=30):
        self.directory = Path(directory)
        self.script, self.node, self.timeout = Path(script), node, timeout

    def __enter__(self):
        self.directory.mkdir(parents=True, exist_ok=True)
        self.log = (self.directory / "samples.jsonl").open("w", encoding="utf-8", newline="\n")
        try:
            self.client = JsonProcess([self.node, str(self.script), str(self.directory)], timeout=self.timeout).__enter__()
        except BaseException:
            self.log.close()
            raise
        return self

    def sample(self, request, case=None):
        input_hash = self.client.write(request)
        response = self.client.read()
        require(response.get("schema") == 1 and response.get("inputSha256") == input_hash,
                "Oracle sample does not match its exact request")
        require(response.get("kind") == request["kind"], "Oracle sample kind mismatch")
        self.log.write(encoded({"case": case, "request": request, "sample": response}) + "\n")
        self.log.flush()
        return response

    def __exit__(self, kind, value, traceback):
        try:
            return self.client.__exit__(kind, value, traceback)
        finally:
            self.log.close()


def sample_file(directory, sample, kind):
    name = sample[kind]
    require(re.fullmatch(r"[0-9a-f]{64}\." + kind, name) is not None, "Invalid oracle sample filename")
    require(name == sample["inputSha256"] + "." + kind, "Oracle sample filename/request mismatch")
    data = (Path(directory) / name).read_bytes()
    require(digest(data) == sample[kind + "Sha256"], "Oracle sample bytes changed: " + name)
    return data
