# Workspace windows

Folia uses WinUI 3's native `TabView` tear-out events. A tab can move into a new
window or into another Folia window, retaining its original document session,
data item, unsaved state, selection, undo history and reading position.
Single-click preview tabs and edit/double-click pinning continue to apply.

Normal launches create a new window in the existing process for that executable
location. Launch requests carry absolute paths by value, including paths that
were relative to the launching process. Different installed/candidate executable
locations remain independent. `--new-instance` explicitly starts an isolated
process; startup tests use it so they never redirect into a user's running app.

## Responsibilities

| Source | Responsibility |
| --- | --- |
| `App.Windows.cpp` | Own live windows and release them on `Closed`. |
| `App.xaml.cpp` | Register/redirect App SDK activation and dispatch launches on the UI thread. |
| `workspace/WorkspaceLaunch.*` | Parse launch requests, normalize paths and marshal their values. |
| `workspace/WorkspaceDocuments.*` | Own document sessions; transfer an inactive session between workspaces. |
| `MainWindow.Tabs.cpp` | Tab presentation, preview/pinning, selection and close commands. |
| `MainWindow.TabTransfer.cpp` | Native tear-out events, container ownership and tab ordering. |
| `EditorDocumentController::Presentation` | Bind a session's UI callbacks to its current window. |

Transfer moves the same `WorkspaceDocument` and boxed document-id item. It does not reopen
the file, serialize its contents or reset its saved-content baseline. The shared
editor surface switches sessions using the existing presentation suspension and
resumption flow. Scroll restoration uses a source position and viewport inset.
Each window's `TabItemTemplate` creates its own `TabViewItem`, header and tooltip.
XAML containers never cross window roots: WinUI caches their parent TabView.
Native drag events are resolved by their stable `Items` data, not the original
`Tabs` container, which can belong to a window the tab has already left.
The native drag loop closes an emptied source window after the gesture ends.
An implicit empty document keeps the source window valid until then; returning a
tab removes that placeholder. An independently opened empty tab is retained.

The target rejects a different session for a file already open in that window.
Neither session's unsaved edits or history are discarded to resolve a duplicate.
`TabItemsChanged` updates model order because native tear-out mode does not raise
the older drag-completed events. Selecting a document only updates selection;
collection reconciliation belongs to document creation, opening, closing and
transfer. It must not run inside a native reorder's collection notification.
A gesture explicitly retains its receiving
window on the data item until the originating move loop ends. This also covers
merging and tearing out again before releasing the pointer, and a single-tab
drag that uses its existing window without requesting a new one.

The shipped WinUI source leaves `TabTearOutRequested.NewWindowId` at zero, so it
cannot identify the target. It also requests and briefly shows a preparatory
window on an ordinary tab click. Preparatory windows are DWM-cloaked, excluded
from switchers and marked nonactivating. The SDK's explicit `SetWindowPos` can
activate even such a window: a preparation-scoped, UI-thread-local CBT guard
vetoes activation and focus only for that HWND (including its child focus).
It is removed on transfer or close. No window procedure or input loop is replaced.
Only a completed session transfer
restores their normal presentation; an unused host closes with the native loop.
External merges reject hidden/preparatory hosts and use the actual `TabListView`
bounds, matching the native within-strip drag test. The SDK's broader admission
region includes the add button and otherwise permits a merge immediately followed
by a tear-out at the same pointer position. Transfer completes the receiving
layout before returning to that move loop.
These are event-protocol details of the pinned SDK, not alternate document paths.
The corresponding SDK sources are
[TabView](https://github.com/microsoft/microsoft-ui-xaml/blob/693f472aeaf89c6dff66256ee6fcd453e5d553b8/controls/dev/TabView/TabView.cpp)
and [TabViewListView](https://github.com/microsoft/microsoft-ui-xaml/blob/693f472aeaf89c6dff66256ee6fcd453e5d553b8/controls/dev/TabView/TabViewListView.cpp).

Child XAML controls can deliver final notifications after their window closes.
`MainWindow::WindowEvent` locks the weak window owner during delivery and skips
closed owners. Tab-container callbacks remain local to their creating window.
Renderer invalidation and animation callbacks also acquire their window weakly.
CoreText focus follows window activation and is restored when a new document
context inherits an already-focused editor surface.

## Verification

After a Debug application build:

```powershell
./tests/app/run.ps1 -Configuration Debug
./tests/app/run.ps1 -Configuration Debug -Visual
./tests/app/test_activation.ps1 -Executable build/app-winui/bin/x64/Debug/Folia.exe
./tests/app/test_startup.ps1 -Executable build/app-winui/bin/x64/Debug/Folia.exe -ReportDirectory build/tests/startup -Runs 3
```

Run controller and visual test builds sequentially: they reuse the production
PCH's compiler PDB. The activation test creates its own candidate directory with
unchanged executable/resource-map names and cleans up only its own processes and
links. Window tests exercise the production transfer path, including exact
source/history restoration, placeholders, duplicates, empty sessions, unloaded
targets, click-only preparation, repeated merge/tear-out in one gesture and a
long document's viewport anchor. They also assert fresh XAML roots and the DWM
cloak/switcher and activation contracts. Tests use real HWNDs and XAML layout,
but do not inject desktop mouse input. End-to-end pointer interaction needs a
separate manual check; direct protocol tests do not establish that result.
