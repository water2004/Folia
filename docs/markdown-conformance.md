# Markdown conformance and parser stress cases

Markdown conformance suites verify parser semantics alongside Folia's
block-local source/CST and exact save behavior. CommonMark and GFM HTML
comparisons are reported separately from source-losslessness checks. A fixture
that has a documented reference difference or an unscored HTML feature must
remain explicitly classified; totals must not hide missing, skipped, or
unresolved rows.

Fixtures, licenses, hashes, generation, and runner details are maintained by
[`tools/markdown-conformance`](../tools/markdown-conformance/README.md). Run the
portable suite through [`tests/run.py`](../tests/run.py) or the focused wrappers
in `tests/parsing/`. Generated pathological cases run as an isolated bounded
suite so large inputs cannot stall the ordinary test runner.

The pinned conformance sources are test data only. Folia does not add cmark or
md4c as a production Markdown parser dependency. A reference comparison is
evidence for the pinned source/version and declared profile; it is not a claim
of universal renderer equivalence.

The conformance suite also includes the 2,125 named HTML entities from the
hash-locked cmark-gfm table and 21 active generated pathological parser cases.
The entity generator writes the committed runtime fixture at
`tests/parsing/fixtures/conformance/entities.tsv`; tests verify decoded text,
lossless CST/source, and exact Markdown save. The pathological runner executes
each generated case in its own process with a timeout and reports pass, fail,
and not-run states. These stress cases stay outside the ordinary core run.
