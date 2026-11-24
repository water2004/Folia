# Platform layout

`src/platform` contains narrow Windows adapters used below the WinUI application:

- `graphics/`: Direct2D factory and native theme adaptation.
- `text/`: DirectWrite factory and text measurement.
- `windows/editor/layout/`: block-local source to UTF-16 display projection,
  DirectWrite inline-object reservation and range geometry.
- `windows/editor/rendering/`: Direct2D text and SVG drawing primitives.
- `windows/editor/interaction/`: DirectWrite hit testing plus deterministic
  input-command translation, table-action/drop hit policy, scrolling,
  key-gesture/shortcut settings, snippet/LaTeX completion sessions, and
  selection-drag models.
- `windows/media/`: Windows backends for native MathJax and Mermaid, SVG
  normalization, HTTP image retrieval, WIC metadata and GIF decoding.
  `folia::windows_media` builds with the Windows SDK and the native renderer
  inputs independently of WinUI. The usvg DLL source and its C ABI live next
  to the SVG adapter.
- `windows/storage/`: Windows file watching, clipboard-image persistence
  and workspace file operations. Callers coordinate open documents and
  forward progress from the worker thread to the UI.
- `windows/completion/`: WinHTTP requests and streaming, credential storage,
  and request cancellation for `folia::windows_completion`.
- `windows/printing/`: Direct2D PDF print-package creation and finalization.

This layer may use Windows, WinRT, Direct2D, and DirectWrite APIs. It must not own Markdown parsing, document mutation, selection, history, or application-shell behavior. Core modules must not depend on this directory.

Keep adapters small and capability-oriented. The editor modules accept plain
values and core `TextPosition`/`Command` types so `FoliaWindowsEditorTests` can
assert geometry, coordinates, caret movement and input translation without
creating a WinUI window. The core supplies block geometry and
viewport/prefetch/retention plans.

Application orchestration, XAML controls and routed events, swap-chain
submission, `DispatcherQueue` frame scheduling, viewport media lifetime,
and user-facing print workflows belong in `app-winui`. They execute the
core and platform plans.

Platform APIs return values and plans; they do not own application resources.
For example, the table interaction module decides which semantic table action
a coordinate represents, while the WinUI application paints the D2D controls.
Likewise, the preparation planner decides which block indices are invalidated
or retained, while the application creates and releases DWrite/image resources.

Run these deterministic Windows tests with `cmd /c tests\platform\run.bat`.
They are not WinUI automation: XAML controls, window activation, swap-chain
submission, and asynchronous media lifetimes are verified by application
build/manual integration checks instead.
