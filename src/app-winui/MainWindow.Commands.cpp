#include "pch.h"
#include "MainWindow.xaml.h"

import folia.core.command;

namespace winrt::Folia::implementation
{
    void MainWindow::RegisterCommandHandlers()
    {
        NewButton().Click(WindowEvent([this](auto const&, auto const&) { NewDocument(); }));
        OpenFolderButton().Click(WindowEvent([this](auto const&, auto const&) { PickWorkspaceFolder(); }));
        SaveAsButton().Click(WindowEvent([this](auto const&, auto const&) { documentController().SaveDocumentAs(); }));
        CloseTabButton().Click(WindowEvent([this](auto const&, auto const&) { CloseDocumentAsync(workspaceDocuments.Active()->id); }));
        UndoButton().Click(WindowEvent([this](auto const&, auto const&) { ExecuteEditorCommand(folia::Command{folia::CommandKind::Undo}); }));
        RedoButton().Click(WindowEvent([this](auto const&, auto const&) { ExecuteEditorCommand(folia::Command{folia::CommandKind::Redo}); }));
        EditCutButton().Click(WindowEvent([this](auto const&, auto const&) { documentController().CutSelection(); }));
        EditCopyButton().Click(WindowEvent([this](auto const&, auto const&) { documentController().CopySelection(); }));
        EditPasteButton().Click(WindowEvent([this](auto const&, auto const&) { documentController().PasteClipboard(); }));
        FindButton().Click(WindowEvent([this](auto const&, auto const&) { ShowFindBar(false); }));
        ReplaceButton().Click(WindowEvent([this](auto const&, auto const&) { ShowFindBar(true); }));
        OpenButton().Click(WindowEvent([this](auto const&, auto const&)
        {
            PickWorkspaceFile();
        }));

        ReloadButton().Click(WindowEvent([this](auto const&, auto const&)
        {
            documentController().ReloadDocument();
        }));

        SaveButton().Click(WindowEvent([this](auto const&, auto const&)
        {
            documentController().SaveDocument();
        }));

        ExportPdfButton().Click(WindowEvent([this](auto const&, auto const&)
        {
            documentController().ExportPdf();
        }));

        CancelOperationButton().Click(WindowEvent([this](auto const&, auto const&)
        {
            documentController().CancelOperation();
        }));

        ReloadExternalButton().Click(WindowEvent([this](auto const&, auto const&)
        {
            documentController().AcceptExternalReload();
        }));

        KeepLocalButton().Click(WindowEvent([this](auto const&, auto const&)
        {
            documentController().KeepLocalVersion();
        }));

        SettingsButton().Click(WindowEvent([this](auto const&, auto const&)
        {
            ToggleSettingsMode();
        }));

        BoldButton().Click(WindowEvent([this](auto const&, auto const&)
        {
            folia::Command command;
            command.kind = folia::CommandKind::ToggleStrong;
            ExecuteEditorCommand(command);
        }));

        ItalicButton().Click(WindowEvent([this](auto const&, auto const&)
        {
            folia::Command command;
            command.kind = folia::CommandKind::ToggleEmphasis;
            ExecuteEditorCommand(command);
        }));

        StrikeButton().Click(WindowEvent([this](auto const&, auto const&)
        {
            folia::Command command;
            command.kind = folia::CommandKind::ToggleStrikethrough;
            ExecuteEditorCommand(command);
        }));

        InlineCodeButton().Click(WindowEvent([this](auto const&, auto const&)
        {
            folia::Command command;
            command.kind = folia::CommandKind::ToggleInlineCode;
            ExecuteEditorCommand(command);
        }));

        Heading1Button().Click(WindowEvent([this](auto const&, auto const&)
        {
            folia::Command command;
            command.kind = folia::CommandKind::SetHeading;
            command.level = 1;
            ExecuteEditorCommand(command);
        }));

        Heading2Button().Click(WindowEvent([this](auto const&, auto const&)
        {
            folia::Command command;
            command.kind = folia::CommandKind::SetHeading;
            command.level = 2;
            ExecuteEditorCommand(command);
        }));

        QuoteButton().Click(WindowEvent([this](auto const&, auto const&)
        {
            folia::Command command;
            command.kind = folia::CommandKind::ToggleBlockQuote;
            ExecuteEditorCommand(command);
        }));

        UnorderedListButton().Click(WindowEvent([this](auto const&, auto const&)
        {
            folia::Command command;
            command.kind = folia::CommandKind::ToggleUnorderedList;
            ExecuteEditorCommand(command);
        }));

        OrderedListButton().Click(WindowEvent([this](auto const&, auto const&)
        {
            folia::Command command;
            command.kind = folia::CommandKind::ToggleOrderedList;
            ExecuteEditorCommand(command);
        }));

        TaskListButton().Click(WindowEvent([this](auto const&, auto const&)
        {
            folia::Command command;
            command.kind = folia::CommandKind::ToggleTaskList;
            ExecuteEditorCommand(command);
        }));

        CodeBlockButton().Click(WindowEvent([this](auto const&, auto const&)
        {
            folia::Command command;
            command.kind = folia::CommandKind::InsertCodeBlock;
            ExecuteEditorCommand(command);
        }));

        TableButton().Click(WindowEvent([this](auto const&, auto const&)
        {
            folia::Command command;
            command.kind = folia::CommandKind::InsertTable;
            command.rows = 2;
            command.cols = 3;
            ExecuteEditorCommand(command);
        }));

        InlineMathButton().Click(WindowEvent([this](auto const&, auto const&)
        {
            folia::Command command;
            command.kind = folia::CommandKind::InsertMathInline;
            ExecuteEditorCommand(command);
        }));

        BlockMathButton().Click(WindowEvent([this](auto const&, auto const&)
        {
            folia::Command command;
            command.kind = folia::CommandKind::InsertMathBlock;
            ExecuteEditorCommand(command);
        }));

        LinkButton().Click(WindowEvent([this](auto const&, auto const&)
        {
            folia::Command command;
            command.kind = folia::CommandKind::InsertLink;
            command.href = U"https://";
            ExecuteEditorCommand(command);
        }));

        ImageButton().Click(WindowEvent([this](auto const&, auto const&)
        {
            documentController().InsertImage();
        }));

        FootnoteButton().Click(WindowEvent([this](auto const&, auto const&)
        {
            folia::Command command;
            command.kind = folia::CommandKind::InsertFootnote;
            ExecuteEditorCommand(command);
        }));

        auto toggleCallout = [this](std::u32string kind)
        {
            folia::Command command;
            command.kind = folia::CommandKind::ToggleCallout;
            command.callout_kind = std::move(kind);
            ExecuteEditorCommand(command);
        };
        CalloutNoteMenuItem().Click(WindowEvent([toggleCallout](auto const&, auto const&) { toggleCallout(U"NOTE"); }));
        CalloutTipMenuItem().Click(WindowEvent([toggleCallout](auto const&, auto const&) { toggleCallout(U"TIP"); }));
        CalloutWarningMenuItem().Click(WindowEvent([toggleCallout](auto const&, auto const&) { toggleCallout(U"WARNING"); }));

        FormatDocumentButton().Click(WindowEvent([this](auto const&, auto const&)
        {
            folia::Command command;
            command.kind = folia::CommandKind::FormatDocument;
            ExecuteEditorCommand(command);
        }));

        TocButton().Click(WindowEvent([this](auto const&, auto const&)
        {
            folia::Command command;
            command.kind = folia::CommandKind::InsertToc;
            ExecuteEditorCommand(command);
        }));

        CutMenuItem().Click(WindowEvent([this](auto const&, auto const&)
        {
            documentController().CutSelection();
        }));

        CopyMenuItem().Click(WindowEvent([this](auto const&, auto const&)
        {
            documentController().CopySelection();
        }));

        PasteMenuItem().Click(WindowEvent([this](auto const&, auto const&)
        {
            documentController().PasteClipboard();
        }));

        SelectAllMenuItem().Click(WindowEvent([this](auto const&, auto const&)
        {
            folia::Command command;
            command.kind = folia::CommandKind::SelectAll;
            ExecuteEditorCommand(command);
        }));
    }
}
