# Large-document core benchmark

The phased memory probe is documented in
[document-memory.md](../../docs/document-memory.md), with its own
[raw results and environment](memory-results). Build `FoliaMemoryProfile` with
the same `FOLIA_BUILD_SCALABILITY_BENCHMARK` option used below. Its default input
is synthetic; `--input /explicit/path.md` is the only mode that opens a supplied
file. `FoliaMemoryProfile 1 unique` adds distinct formula identifiers to the
mixed-content control. No input content is printed or saved by the probe.

`memory_profile.cpp` records parse/index, virtual projection, bounded projection
sweep, first/middle/last edit+undo, source entry/projection/exit, and close. On
glibc it additionally reports in-use allocator bytes from `mallinfo2`; other
platforms report zero for that field. This excludes native layout and formula
rendering. `materialized_units` is the virtual model's sparse resident-index
count: eager source models do not populate that index, so zero does **not** mean
that the source projection holds no text objects. The recorded sources and
compiler options match between baseline and current runs; RSS and allocator
bytes are distinct measurements.

Recompute the memory summary, including timing ranges for all retained samples:

```sh
python3 tools/document-scalability/summarize_memory.py
```

## List editing benchmark

`core_scalability.cpp` generates complete repetitions of:

```markdown
- Sample markdown list item for parser and upload testing.
```

The default input is at least 10 MiB (rounded up to a complete line). No uploaded
file is opened. The JSONL metadata records the actual byte count. Each run uses
the real parser, editor transactions and history, and render projection APIs.

## Build

Use the [standalone core toolchain](../../tests/libraries/core/README.md), with
`-DFOLIA_BUILD_SCALABILITY_BENCHMARK=ON`, then:

```sh
cmake --build build/core-portable --target FoliaCoreScalability --parallel 4
build/core-portable/FoliaCoreScalability 10 3 > build/core-portable/scalability.jsonl
```

Arguments are input size in MiB and edit/undo rounds per location. The first,
middle, and final list items are exercised separately. Every undo is checked
against the exact original source of its owner.

## What is measured

Each phase records wall time, Linux process resident memory and its high-water
mark, and available architectural counters. Measurements cover:

- Initial parsing and document indexes.
- Initial virtual render projection.
- Materializing a window of at most 32 physical render units at each location.
- Selection, one-character insertion, undo, and both resulting incremental
  render updates, separately.
- List-item splitting with Enter, its undo, and the resulting structural render
  projections, separately from ordinary typing.

`null` counters are absent from the older revision, not evidence of zero work.
`rebuilt_units` is the model's most recent incremental-update count; viewport
rows can retain that previous update's value. The work counters are reset for
each timed phase.
RSS includes allocator reservations and every live document/index/projection;
it is not an allocation census. Outside Linux, the memory fields are zero and
only timings/counters are usable. Run each executable in a fresh process, on
the same otherwise idle machine, with the same optimization and toolchain.

This is **not a Windows application or DirectWrite benchmark**. It does not
estimate character widths, claim native layout accuracy, or count GPU/font
caches. Native scrolling, IME and exact geometry need the Windows gates.

## Comparing a previous revision

The standalone CMake accepts `-DFOLIA_SOURCE_ROOT=/path/to/other/checkout` while
keeping this exact benchmark source. Build only `FoliaCoreScalability` in a
different build directory. This avoids maintaining different benchmark
implementations for the two revisions.

When a previous MSVC-only revision needs direct module imports or forward-type
definition ordering corrected before Clang can compile it, keep those
declaration-only changes separate and disclose them with the results. Do not
copy new editing, parsing, indexing, or rendering algorithms into the baseline.

`baseline-clang.patch` contains exactly those declaration-only corrections for
baseline `5861fff47b5697c65949ba41cd3f4edd5256ef45`. It changes direct imports,
removes a redundant nested `export`, makes one deduced return type explicit,
and moves two inline definitions below a type they require to be complete.
Apply it only to an isolated baseline checkout, not to the current branch:

```sh
git worktree add --detach ../Folia-baseline 5861fff47b5697c65949ba41cd3f4edd5256ef45
git -C ../Folia-baseline apply --unidiff-zero ../Folia/tools/document-scalability/baseline-clang.patch
git -C ../Folia-baseline submodule update --init third_party/ut
```

Configure a second build with the same compiler/options and
`-DFOLIA_SOURCE_ROOT=/absolute/path/to/Folia-baseline`, and build only the
benchmark target. The model algorithms are still those of the baseline.
The patch deliberately uses zero context lines; `--unidiff-zero` is required.

## Recorded observations

The first raw 10 MiB runs are `results-baseline.jsonl` and `results-current.jsonl`;
the corresponding 1 MiB controls have the `-1mib` suffix. The actual machine,
compiler, standard-library headers/runtime versions, flags, and measurement
conditions are recorded in `results-environment.json`. The first results keep
the structural projection cost visible; they do not claim constant-time Enter.

Regenerate the exact unrounded summary with:

```sh
python tools/document-scalability/summarize.py \
  --output tools/document-scalability/results-summary.json
```

Each ordinary edit phase contains three observations per location and is
summarized by its median. Startup, viewport materialization, and structural
phases occur once per location and remain individual observations. KiB values
are divided by 1024 to report MiB. The raw files remain the source of truth.

### Structural reconciliation and final series

The intermediate `-structural` series records local structural reconciliation
before the initial planner allocation improvement. It is preserved, including
the 375.943 ms initial virtual projection. The final runs use separate files:
`results-baseline-final.jsonl`, `results-current-final.jsonl`, and the matching
`-1mib` controls. `results-final-environment.json` records the compiler/runtime,
source fingerprint, benchmark fingerprint, and measurement conditions for
this series. Both executables were rebuilt from the same benchmark source,
including normalized tree-delta forwarding when the revision provides it.
The baseline still contains only the disclosed Clang declaration/import patch.
The final core source snapshot is commit
`925d24af98bd477d49de060cb0109f205e2401ec` on
[`architecture/bounded-document-layout`](https://github.com/water2004/Folia/tree/architecture/bounded-document-layout).
The environment record preserves the local build commit and verifies the
matching source fingerprint; the published commit has the same Git tree.

```sh
python tools/document-scalability/summarize.py --series final \
  --output tools/document-scalability/results-final-summary.json
```

Selected 10 MiB observations, in milliseconds:

| Phase | Baseline | Final implementation |
| --- | ---: | ---: |
| Parse and index | 3289.11 | 1870.09 |
| Initial virtual projection | 105.665 | 188.154 |
| First viewport projection | 940.401 | 4.175 |
| Insert into final item | 3.68405 | 0.002644 |
| Project that insertion | 645.73 | 0.101612 |
| Project ordinary undo at final item | 672.765 | 0.101112 |
| Enter in first item | 21.315 | 9.30102 |
| Project Enter in first item | 759.243 | 19.6957 |
| Enter in final item | 32.2169 | 0.022193 |
| Project Enter in final item | 878.563 | 3.94721 |
| Project structural undo at final item | 737.556 | 1.66522 |

Initial virtual projection is more expensive because the new model constructs
physical-fragment and navigation indexes. The subsequent viewport and edit
work is substantially smaller. A tail split visits 125 fragment-planning nodes
and updates 124 navigation entries; a split at the beginning visits 131
planning nodes but updates 355,452 navigation entries in the shifted suffix.
Structural edits are therefore not universally constant-time. The remaining
work is visible in the raw counters rather than excluded from the timings.

The last recorded process RSS is 877,828 KiB for the baseline and 383,272 KiB
for the new model. Process high-water RSS is 1,072,924 KiB and 592,424 KiB,
respectively; the new high-water mark is already reached during parsing.
These measurements include core source/CST/tree/index storage and allocator
reservations. They do not measure the native application's working set or
prove its DirectWrite retention and scrolling behavior.

## Verification boundaries

`validation-linux.json` records the exact local test commands, counts and
selected uncolored runner summaries. The complete portable run passed 821
tests and 539,461 assertions before the last initial-planner, source-root
validation and pagination refinements. After those refinements, the rebuilt
runner passed the focused fragment set (15 tests /31,933 assertions) and
print set (9 tests /33,813 assertions). These focused sets overlap; their
counts must not be added as if they were independent full-suite runs.

The native Windows and CI jobs have **not run**.
[GitHub Actions run 43720698863](https://github.com/water2004/Folia/actions/runs/43720698863)
was rejected before any job steps because the repository account was locked
due to a billing issue. The committed DirectWrite, retention, source-anchor,
shared-flow and preview regressions therefore remain unverified on Windows.
