# Native Mermaid Windows host integration

The WinUI application calls the existing `FoliaNativeMermaid.dll` C ABI through
`src/platform/windows/media/mermaid/MermaidRenderer`. That component owns the
asynchronous source/theme cache and notifications. DLL-allocated buffers are
released through the same DLL. Windows text measurement and font-family
resolution remain in `src/platform/text/`; the portable core receives measured
text metrics through its renderer contract.

Native measured SVG passes through the existing `SvgNormalizer` before Direct2D
display or PDF export. The normalizer remains the common boundary for Mermaid
and other SVG sources. The application uses one Mermaid rendering path; do not
add a parallel Rust renderer or a second host-specific implementation.

## Build and packaging responsibilities

`build_app.ps1` enables the integrated Mermaid CMake target and builds the app.
MSBuild stages the native Mermaid DLL beside the app. The DLL and third-party
license notices are collected through the existing WiX payload rules. The
normalizer remains a separate dependency and is restored by `setup.ps1`.

Keep these responsibilities aligned when changing packaging:

- CMake builds the Mermaid library and stages its runtime artifacts.
- MSBuild copies the selected build output to the app runtime directory.
- License notices accompany the native dependencies in app and installer output.
- The normalizer handles SVG preparation for Direct2D and PDF; Mermaid does not
  bypass it.

## Verification methods

Run setup and a Release build before the targeted app tests:

```powershell
powershell -ExecutionPolicy Bypass -File ./setup.ps1
./build_app.ps1 -Configuration Release
./tests/app/run.ps1 -Configuration Release -SourceFilter test_mermaid_rendering -TestFilter '*native Mermaid*'
```

The app tests cover worker behavior, normalization, preview/edit/theme behavior,
available width, and PDF integration. A host SVG gate can check text and viewport
preservation across the normalizer:

```powershell
python tools/mermaid-cpp/check-winui-svg.py --native build/core-release/tools/mermaid-cpp/FoliaNativeMermaid.dll --normalizer build/dependencies/usvg-normalizer/release/folia_svg_normalizer.dll
```

For changes to display or PDF behavior, inspect rendered output as well as test
results. Record the source revision, build configuration, input set, and which
host/display/PDF paths ran. A native DLL build or offscreen test does not establish
full-shell interaction or installer startup behavior.
