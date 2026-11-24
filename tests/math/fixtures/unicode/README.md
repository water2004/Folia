# Unicode normalization reference data

`normalization-corpus.tsv` is the existing 19,965-row, ten-column source corpus.
Its bytes are unchanged. Each field is a sequence of four-hex-digit UTF-16 units:
the first five columns are inputs and the final five are their pinned NFD outputs.

The former compiled metadata now lives in three tables:

- `normalization-metadata.tsv`: original corpus row count and source SHA-256.
- `normalization-sequences.tsv`: all 85 extra UTF-16 input/output observations,
  including NULs and isolated surrogates, represented with reversible unit escapes.
- `normalization-digests.tsv`: all 17 ordered plane digests as exact hexadecimal
  64-bit integers.

The tests retain all five comparisons per corpus row and iterate every code point
in all 17 planes. Data paths resolve through the shared fixture module, independent
of the working directory. Missing data or malformed columns fail the test.

`tools/math-port/unicode-normalization-oracle.cjs` captures the original Unicode
16.0 JavaScript host behavior used by the MathJax port. It emits these tables,
the original corpus, and its provenance manifest; `--check` compares them without
writing. Ordinary native tests use the committed observations and do not run Node.
