# Native math verification

This is the current verification contract for changes to the native math
renderer. It describes required evidence; it does not claim that every check
was rerun for every release. The current source and tests are authoritative.

## Required evidence

Choose checks according to the changed owner and its consumers:

1. **Pinned Source and generated data:** use the corresponding `tools/math-port/`
   witness or generator. Verify upstream version, file hashes, package order,
   exact values and generated output. Do not record native output as the Source
   expectation. A missing or mismatched lock is a failure, not permission to
   refresh it automatically.
2. **Native behavior:** run the relevant tests under `tests/math/`, including
   family, boundary, error and Unicode cases. Ownership changes also need
   lifecycle checks for success, TeX error, exception and cancellation.
3. **Integration:** exercise the C ABI and affected Windows worker/editor path
   when those boundaries change. Verify cancellation, stale-result rejection,
   viewport sizing and that the UI thread performs no synchronous layout or
   font I/O.

Source observations and native tests answer different questions. A Source
witness validates the locked upstream behavior for its enumerated cases; it
does not prove C++ ownership or integration. A successful compile or isolated
render does not establish full profile parity.

## Rendering comparisons

- Preserve the pinned input set, package precedence, parser errors, numeric
  values, Unicode behavior and ordered output. Compare the SVG tree, attributes,
  resource ordering, escaping and numeric spelling where the family contract
  requires exact output.
- Compare layout metrics and geometry for the affected Common families. Keep
  the established tolerances; do not loosen them or hard-code sample
  coordinates.
- For paint inherited from MathJax’s stylesheet, independently materialize the
  expected paint from the pinned stylesheet and compare it with native output.
  Also compare the standalone native SVG with an image rendered using the real
  upstream stylesheet so both sides cannot pass by omitting the same property.
- Check both standalone fragments and ordinary document formulas where a
  change affects their shared rendering path. Batch and single-item behavior
  must agree where they represent the same request.

## Build and test entry points

The [test guide](../../tests/README.md) describes toolchain setup and the
portable runner. Restore the locked MathJax source inputs before the first
native math build:

```sh
npm ci --prefix tools/mathjax-bundle --ignore-scripts
python tools/math-port/prepare-source.py
python tests/run.py configure --suite math
python tests/run.py build --suite math --jobs 4
python tests/run.py --suite math
```

For a focused diagnostic, use the configured test runner’s test filter or the
corresponding CTest target, then run the complete affected math family and its
integration tests before accepting the change. Do not treat a source-only
generator invocation as native test coverage.

On Windows, keep native module compilation within the repository’s bounded
parallelism. Console tests link `folia_console_test_runtime`; abnormal process
exit requires stopping the broader run and diagnosing the focused failure.
Release console XAML hosts have a known initialization limitation, so verify
Release application startup through the real packaged application entry rather
than describing that host as product acceptance.

## Ownership and platform rules

- Common computes layout; Scene stores the finished values; SVG serializes.
  Do not introduce another layout or legacy object-model path.
- Request-owned MML, wrappers and glyph data must return to baseline after
  success, error, exception and cancellation. Do not use RSS alone as proof of
  object lifetime.
- Keep source coordinates and UTF conversions explicit at platform edges.
  Formula and editor geometry comes from the production measurement/layout
  path, not fixed character-width estimates or sample-specific coordinates.
- Keep generated artifacts in ignored build directories. Changes to upstream
  data must update the exact source lock and third-party notice as applicable.
