# TeX HTML source fixtures

The 22 direct cases, five complete pipeline cases, recovery case, three DOM
strings and original Source JSON SHA-256 are preserved as 32 ordered records.
Inputs, serialized MML, length-prefixed source snapshots, error fields and
initial allowTexHTML flags remain unchanged.

`texhtml-source.cjs --write-fixture` still captures and checks the pinned Source
JSON digest, then writes these tables. Its separate evidence JSON path remains
unchanged. The ordinary native tests need only the committed TSV data.

Every string preserves exact UTF-16 code units using reversible `\uHHHH`
escapes, including NUL and unpaired surrogates. The generator retains the
original three escaping-boundary inputs; the runtime reader validates exact
column and record counts without interpreting or normalizing snapshots.
