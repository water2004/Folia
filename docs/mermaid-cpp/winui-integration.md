# Native Mermaid WinUI integration

Folia 0.7.0 uses the native Mermaid renderer. The isolated acceptance worktree was based on `324271ea` under `build/candidates/folia-mermaid-integration`. The accepted native baseline includes the default ELK State path.

## Architecture and packaging

`MermaidRenderer` retains its asynchronous source/theme cache and notifications,
but calls the existing `FoliaNativeMermaid.dll` C ABI. DLL-owned buffers are
released through that DLL. Native measured SVG passes through the independent
`SvgNormalizer` for Direct2D and PDF. A numeric outer SVG preserves authoritative
ABI dimensions without rewriting the inner viewport (including Info 400 x 100
and fractional Class widths). HTML `foreignObject` is rejected before usvg can
silently discard it; native portable measured SVG labels are a prerequisite.
The accepted native baseline supplies these labels and canonical installed font
names. There is one rendering path.

The normalizer remains available for ordinary SVG and math; its queue worker
now starts lazily for asynchronous use, avoiding an idle worker for Mermaid's
synchronous normalization. Worker initialization and stale-generation queue
removal are corrected. An unused editor normalization wrapper is removed.
Rust's Mermaid export and dependency are removed, while usvg and its font cache
remain for SVG normalization.

`build_app.ps1` enables integrated `FOLIA_BUILD_MERMAID` and builds the existing
native target with at most four jobs. MSBuild copies the DLL and
`Mermaid-Licenses` from `build/core[-release]/tools/mermaid-cpp`.
`FoliaNativeMermaidBuildDir` only overrides the artifact location. CMake stages
17 license/notice files, including yaml-cpp. Existing recursive WiX payload
collection includes these files; no installer feature or workflow change is
needed. Setup still requires Cargo for the independent normalizer.

The app test runner detects the self-contained runtime by app-local
`Microsoft.UI.Xaml.dll`, stages its DLLs and embeds the existing app manifest.
Separate scratch directories prevent app-local DLLs leaking into framework
runs. The tracked worker tests do not require PDF artifacts or a printer driver.

## Verified isolated acceptance

- Required `setup.ps1` completed with cached NuGet/native dependencies and Cargo
  offline/locked. NuGet advisory metadata was queried; this was not a completely
  offline setup. True self-contained WinUI **Release** build and incremental
  lazy-worker rebuild passed (`build/winui-release-build.log`,
  `build/winui-release-followup.log`, `build/winui-setup.log`).
- Integrated native **Release** CMake target and license staging passed. The
  final DLL was copied by MSBuild `CopyNativeMermaidRuntime` from its default
  integrated path. The earlier Debug DLL is not the acceptance artifact.
- Final app tests: **6 tests, 81 assertions** passed in
  `build/winui-mermaid-final.log`: 38 worker/normalizer assertions and 43
  offscreen preview/edit/theme/width/PDF assertions. Production Direct2D
  preparation/painting and `ExportPdfStep` were exercised. PDF uses the actual
  frozen `BuildPrintRenderModel` on an MTA worker; both themes completed 3 pages.
  Flowchart/CJK, Info, ER, Requirement, Use Case, Swimlane, Event Modeling and
  Venn were included. This is offscreen acceptance, not full-shell interaction.
- Strict ABI viewport/glyph-preservation gate: **24/24** passed
  (`build/winui-svg-gate-final-release.log`). It checks sentinel text before
  normalization and compares glyph paths against the same SVG with only that
  text removed; absence of HTML alone is insufficient.
- Integration-owner full final Release DLL gate: **76/76**, 382 glyph elements,
  recorded in `D:/md-cpp/build/mermaid-full-winui-release-gate-final/results.json`.
  WiX manifest generation verified the two DLLs plus all 17 notices (19 required
  entries); this did not build an MSI.
- Independent normalizer `cargo test --offline --locked --jobs 2` passed 4/4;
  retained dependency versions are unchanged (`build/normalizer-test.log`).

Final app-output SHA-256:

| Artifact | SHA-256 |
| --- | --- |
| FoliaNativeMermaid.dll | `4C6F74C78858DBEEC9DC21A75486C150D2DC5FCD5FDD715A00FE4D7C8F45B966` |
| folia_svg_normalizer.dll | `A035D9191FAFDEA0FDCA47BF907E3F3C8D6114FA6900498B862C00DEDE053D85` |

## Reproduction

Coordinate compiler slots (global maximum four), then run setup before builds:

```powershell
powershell -ExecutionPolicy Bypass -File ./setup.ps1
./build_app.ps1 -Configuration Release
./tests/app/run.ps1 -Configuration Release -SourceFilter test_mermaid_rendering -TestFilter '*native Mermaid*'
python tools/mermaid-cpp/check-winui-svg.py --native build/core-release/tools/mermaid-cpp/FoliaNativeMermaid.dll --normalizer build/dependencies/usvg-normalizer/release/folia_svg_normalizer.dll
```

The machine-dependent offscreen/PDF harness is deliberately an ignored acceptance
artifact at `build/acceptance/test_mermaid_presentation.cpp`, not a default test.
On this candidate, with Windows Print to PDF available, replay it by temporarily
copying that file into `tests/app/`, then running:

```powershell
$env:FOLIA_MERMAID_ACCEPTANCE_OUTPUT = "$PWD/build/tests/mermaid-acceptance"
$env:CL_MPCount = '1'
./tests/app/run.ps1 -Configuration Release -SourceFilter 'test_mermaid*' -TestFilter '*native Mermaid*'
```

Remove only that temporary copied test afterward. Generated `native-light.pdf`,
`native-dark.pdf`, six rendered page PNGs, and preview/edit/theme PNGs are under
`build/tests/mermaid-acceptance`; render with `pdftoppm -r 90 -png` for inspection.
These PDF and preview artifacts record the isolated acceptance run. The local MSI build and startup check are recorded separately in [HANDOFF.md](HANDOFF.md).
