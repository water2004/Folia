"""Legacy Source-profile coverage checks over independent upstream observations."""

import argparse
from pathlib import Path
import struct
import subprocess

from .common import ROOT, digest, emit, equal, keys, read, require
from .source_profiles import capture


CONFIG = ROOT / "tools/math-port/observer-packets.json"


def legacy_inputs(entry, expected, *, root=ROOT):
    records = [entry["legacy"]] if "legacy" in entry else []
    if entry["comparison"] in ("leaf", "mml-node"):
        records.append(expected["legacy" if entry["comparison"] == "leaf" else "originalProfile"])
    for record in records:
        file = root / record["path"]
        equal(record["sha256"], digest(file), "Original observer profile bytes changed: " + record["path"])
        if "bytes" in record:
            equal(record["bytes"], file.stat().st_size, "Original observer profile size")
    return read(root / entry["legacy"]["path"]) if "legacy" in entry else None


def encode_original(value):
    """Original MathChoice observer encoding: binary64 and ordered own keys."""
    if isinstance(value, dict) and value.get("$type") == "undefined":
        return ["undefined"]
    if isinstance(value, dict) and value.get("$type") == "number":
        value = float(value["value"])
    if value is None:
        return ["null"]
    if isinstance(value, bool):
        return ["boolean", value]
    if isinstance(value, (int, float)):
        return ["number", struct.pack("<d", float(value)).hex()]
    if isinstance(value, str):
        return ["string", value]
    if isinstance(value, list):
        return ["array", len(value), [[str(index), encode_original(item)] for index, item in enumerate(value)]]
    require(isinstance(value, dict), "Unsupported original Source value")
    return ["object", None, [[key, encode_original(value[key])] for key in keys(value)]]


def compare_packet(packet, entry, actual, expected, *, legacy=None):
    kind = entry["comparison"]
    if kind in ("leaf", "mml-node"):
        profile = actual["profile"] if kind == "leaf" else actual
        for key in ("assertions", "cases"):
            equal(expected["profile"][key], profile[key], key)
        for key in ("sourceProfile", "directProfile"):
            equal(expected["profile"][key], profile[key], key, ordered=True)
        source = profile["sourceProfile"]
        result = {"sourceCases": sum(row[0] == "case" for row in source),
                  "helperObservations": sum(row[0] != "case" for row in source),
                  "directObservations": len(profile["directProfile"]), "profileSha256": entry["sha256"], "mode": "check"}
        if kind == "leaf":
            result.update(sources=actual["sources"], legacyAssertions=profile["assertions"], legacyCases=profile["cases"])
        else:
            result.update(assertions=profile["assertions"], cases=profile["cases"], originalProfileSha256=expected["originalProfile"]["sha256"])
        return result
    if kind == "mathchoice":
        require(legacy is not None, "Missing original MathChoice profile")
        equal(expected["records"], actual["records"], "MathChoice canonical observations", ordered=True)
        equal(410, len(actual["records"]), "MathChoice records")
        equal(412, legacy["assertions"], "MathChoice original assertions")
        equal(410, legacy["cases"], "MathChoice original cases")
        equal(legacy["names"], [row["name"] for row in actual["records"]], "MathChoice original names")
        source = [[*metadata, encode_original(actual["records"][index]["observations"][0])]
                  for index, metadata in enumerate(actual["sourceMetadata"])]
        equal(409, len(source), "MathChoice original Source calls")
        equal(legacy["sourceProfile"], source, "MathChoice original Source profile", ordered=True)
        return {"assertions": legacy["assertions"], "cases": legacy["cases"], "sourceCalls": len(source),
                "originalValueDeclarations": len(legacy["expectedValues"]), "originalProfileSha256": entry["legacy"]["sha256"], "mode": "check"}
    equal(expected, actual, "Complete Source profile")
    if packet == "script":
        return {**actual["summary"], "mode": "check"}
    if packet == "mml-registry":
        count = len(actual["keys"][1])
        return {"registry_entries": count, "construction_cases": len(actual["constructs"]), "instance_observations": count ** 2,
                "protocol_observations": len(actual["protocol"][1]), "mode": "check"}
    cases = actual["cases"]
    return {"profile": Path(entry["profile"]).name, "cases": len(cases) if isinstance(cases, list) else cases,
            "assertions": actual.get("assertions"), "mode": "check"}


def replay(packet, entry, *, root=ROOT, runner=subprocess.run):
    equal(entry["sha256"], digest(root / entry["profile"]), "Frozen Source profile changed")
    expected = read(root / entry["profile"])
    legacy = legacy_inputs(entry, expected, root=root)
    actual = capture(entry, root=root, runner=runner)
    return compare_packet(packet, entry, actual, expected, legacy=legacy)


def main(group, args):
    packets = read(CONFIG)[group]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--packet", choices=list(packets), required=True)
    parsed = parser.parse_args(args)
    entry = packets[parsed.packet]
    emit(replay(parsed.packet, entry))
    if "legacy" in entry:
        emit({"packet": parsed.packet, "legacyProfile": entry["legacy"]["path"],
              "legacyProfileSha256": entry["legacy"]["sha256"], "profile": entry["profile"], "mode": "check"})
    return 0


def direct(packet, args):
    require(not args or args == ["--check"], "Usage: Source-profile checker [--check]")
    emit(replay(packet, read(CONFIG)["direct"][packet]))
    return 0
