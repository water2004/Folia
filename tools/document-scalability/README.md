# Large-document core benchmark

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

The raw 10 MiB runs are `results-baseline.jsonl` and `results-current.jsonl`;
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
