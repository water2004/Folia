# Workspace windows

Folia uses WinUI 3 tabs across multiple windows. Drag a tab into a new window or
another Folia window to move the same document session, including unsaved edits,
selection, undo history and reading position. Single-click preview tabs and
edit/double-click pinning continue to apply.

Launching Folia without a file opens a new window. Opening a file from Explorer
brings its existing tab and window forward, including a minimized window. By
default, a new file opens as a pinned tab in the last active window and editor
group. The General setting to open files in new windows gives each file its own
window. The first file starts the workspace on a cold launch.

Drop supported documents or images onto the reading area to open pinned tabs in
the group under the pointer. Dropping onto a workspace folder in the file tree
uses that folder's file-transfer controls.

Launch requests use absolute paths, including paths that were relative to the
launching process. Each executable location registers its own activation key.
`--new-instance` starts an isolated process for startup tests.

## Text input focus

Each editor has a CoreText edit context. Moving between windows restores text
input to the active editor without taking focus from another control, such as a
Settings text box. Folia does not force a keyboard layout or Chinese/English
conversion mode, and does not recreate contexts per key.

Automated window tests check TSF document focus across window switches and late
focus notifications. They do not inject keyboard input; Shift conversion with
an installed IME still needs a manual check.

## Workspace layout

The activity rail, sidebar, title strip and reading card form the workspace
shell. The sidebar retains its state across tab changes. In a restored,
resizable window it opens and closes by moving the window's left edge; in a
maximized or non-resizable window it animates its width and reflows the reading
card. The file explorer lists the workspace's children, with large directories
loaded as needed. Document tools appear on hover or keyboard focus without
resizing the editor.
