# Workspace windows

Folia uses WinUI 3's native `TabView` tear-out events. A tab can move into a new
window or into another Folia window, retaining its original document session,
tab container, unsaved state, selection, undo history and reading position.
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

Transfer moves the same `WorkspaceDocument` and `TabViewItem`. It does not reopen
the file, serialize its contents or reset its saved-content baseline. The shared
editor surface switches sessions using the existing presentation suspension and
resumption flow. Scroll restoration uses a source position and viewport inset.
The native drag loop closes an emptied source window after the gesture ends.
An implicit empty document keeps the source window valid until then; returning a
tab removes that placeholder. An independently opened empty tab is retained.

The target rejects a different session for a file already open in that window.
Neither session's unsaved edits or history are discarded to resolve a duplicate.
`TabItemsChanged` updates model order because native tear-out mode does not raise
the older drag-completed events. `NewWindowId` identifies the receiving window
even when a single gesture merges and tears the tab out again.

Child XAML controls can deliver final notifications after their window closes.
`MainWindow::WindowEvent` locks the weak window owner during delivery and skips
closed owners. Tab-container callbacks instead follow their movable weak owner.
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
source/history restoration, placeholders, duplicates and a long document's
viewport anchor. They do not inject desktop mouse input.
