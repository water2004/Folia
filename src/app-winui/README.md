# app-winui

WinUI 3/C++/WinRT application shell for Folia.

This layer owns the native title bar and document tabs, main menu, animated file/outline sidebars, settings navigation, status/progress surfaces, file dialogs, and one shared `SwapChainPanel` host. It must not implement Markdown mutations directly; UI actions are translated to core `Command` values and applied through core document transactions.

Source ownership:

- The directory root contains XAML application/window files and the composition root. `MainWindow.xaml.cpp` owns lifecycle and window state; command and event wiring live in separate translation units.
- `workspace/` owns the open-document collection, file explorer, outline
  pane and shared vector icon definitions. Each document keeps its
  original session and document controller; activation only rebinds the shared
  input and rendering resources. `MainWindow.Documents.cpp` owns that binding,
  `MainWindow.Tabs.cpp` projects the collection into native tabs and sidebars,
  `MainWindow.Folders.cpp` owns file/folder pickers, and
  `MainWindow.Shortcuts.cpp` routes application shortcuts.
- `editor/session/` adapts the core editor/document state to the native
  application. The session facade is split by boundary: document/render-model
  lifecycle, rendered/source editing dispatch, search, and UTF-32-to-UTF-16
  text-input projection. These translation units share one session state; they
  must not introduce parallel selections or cached document models.
- `editor/interaction/` owns routed events, text input, scrolling and frame
  scheduling. It translates input into platform actions and core commands.
- `editor/document/` coordinates document open, save, reload, file relocation,
  clipboard and unsaved state with each tab's session. Windows storage adapters
  own file watching and filesystem operations.
- `editor/rendering/` assembles native layouts and paints with the active
  theme. The Windows platform supplies the text projection and its UTF-16
  ranges, inline-object geometry, and text and SVG drawing primitives.
  `EditorSurfaceRenderer` is the stateful facade; document work is delegated to
  a preparation pass, a one-block resource preparer, and a render pass. Native
  layouts and the prepared-document cache live in `layout/`, asset placement
  and previews in `media/`, and the themed drawing passes in `painting/`.
- `media/` owns asynchronous viewport-facing image caching and display
  preferences. Windows backends load image bytes and dimensions and handle
  GIF decoding, math, Mermaid, and SVG integration. Code and TeX syntax
  analysis comes from `folia::highlight`.
- `completion/` connects editor state and suggestions to the Windows
  completion backend.
- `settings/views/` owns the settings pages and controls. `settings/storage/`
  owns persisted application settings, feature schemas and atomic replacement.
  `settings/latex/` coordinates the editable command catalog and usage history.
- `export/` owns PDF export orchestration, page drawing, and the types used
  for progress and cancellation. The Windows printing adapter owns the print
  package and output stream.
- `localization/`, `theme/`, and `storage/` own compiled WinUI resources,
  complete theme materialization and the compile-time-configurable Assets
  root. `folia::encoding` detects and converts document bytes, and the session
  owns the encoding selected for the open document. File watching, clipboard
  image writes and workspace filesystem operations use Windows storage
  adapters. Pickers, unsaved state and open-document relocation stay in the
  application.

File clicks use a replaceable preview tab. Double-clicking or editing keeps
that document open; dirty previews cannot be replaced. The folder sidebar is
hidden until the user opens a folder or explicitly expands it. The outline
has its own button and pane. Each tab retains its own selection, undo history,
source-view state, reading position, and file-backed formula geometry; it does
not own another GPU surface. Switching tabs performs no full-document parse
or serialization.

The default workspace shortcuts are Ctrl+N (new), Ctrl+O (open file),
Ctrl+Shift+O (open folder), Ctrl+W (close tab), and Ctrl+Tab / Ctrl+Shift+Tab
(next / previous tab). They use the existing configurable shortcut catalog.

The document pipeline has three app-owned stages:

1. `EditorDocumentPreparationPass` reconciles cached blocks and executes the
   viewport/prefetch/retention plans supplied by `folia_platform`.
2. `EditorDocumentBlockPreparer` turns one core `RenderBlock` into DWrite/D2D,
   code-highlight, math/SVG, Mermaid, and image resources without rewalking the
   document. Work is budgeted and prioritized around the viewport.
3. `EditorDocumentRenderPass` paints already-prepared visible blocks and
   populates the interaction map.

These are concrete collaborators, not virtual plug-ins. The hot path keeps the
same prepared cache, incremental work budget, and one visible-range traversal;
the split must not add per-block heap objects, callbacks, or a second document
scan. Large files remain appropriate for cohesive state machines such as GIF
decoding and PDF page recording. New UI commands and Markdown behavior do not
belong in rendering files.

Document opening and charset conversion are asynchronous at the shell boundary.
The status-bar encoding command separates detection-driven reopening (ranked
candidates with confidence) from explicit conversion saves (the alphabetical
ICU converter catalog without confidence). Picker selection,
command-line/Explorer activation, and MSI file associations all converge on
the same document controller; parsing still produces one authoritative core
document. PDF export records pages off the UI thread, reports preparation and
per-page progress, and observes cancellation through the document-controller
lifetime.

Settings replaces the document workspace rather than opening a separate
window. Its navigation currently contains General, Shortcuts, LaTeX Commands,
Themes, Licenses, and About. Only theme selection is staged behind an `Apply`
button; other switches and bindings persist immediately. See
[docs/SETTINGS.md](../../docs/SETTINGS.md) and
[docs/THEMES.md](../../docs/THEMES.md).

Do not create another top-level presentation library merely to move code. A
new shared presentation target is justified only when a second UI backend needs
the same platform-neutral presentation API. Until then, deterministic native
geometry and scheduling policy belongs in `platform`, while resource ownership
and orchestration remain in this application.

Build notes:

- Build `Folia.vcxproj` through the root `build_app.ps1` script so Windows App SDK XAML/IDL/C++WinRT generation runs through the NuGet targets and all output stays below `build/app-winui`.
- NuGet packages are restored into `packages/` by running `powershell -ExecutionPolicy Bypass -File setup.ps1` from the repository root.
- Keep CMake for `folia_core`, `folia_platform`, and tests; do not configure `-DFOLIA_BUILD_WINUI=ON`.
- Run `cmd /c tests\platform\run.bat` for deterministic Windows editor tests. These
  tests cover block geometry, DirectWrite hit testing/caret coordinates,
  table action/drop policy, preparation invalidation, viewport planning, scroll
  state, key gestures, and selection drag projection without UI automation.
- The editor area is a `SwapChainPanel`; the WinUI rendering layer consumes the core `RenderModel` and maps DirectWrite positions to block-local `TextPosition` values.
- Do not replace this with WebView2, RichEditBox, TextBox, RichTextBlock, or an HTML renderer.
