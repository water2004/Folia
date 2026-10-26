#include "pch.h"
#include "MainWindow.xaml.h"

import folia.core.command;

namespace winrt::Folia::implementation
{
    void MainWindow::RegisterCommandHandlers()
    {
        NewButton().Click([this](auto const&, auto const&) { NewDocument(); });
        OpenFolderButton().Click([this](auto const&, auto const&) { PickWorkspaceFolder(); });
        SaveAsButton().Click([this](auto const&, auto const&) { documentController().SaveDocumentAs(); });
        CloseTabButton().Click([this](auto const&, auto const&) { CloseDocumentAsync(workspaceDocuments.Active()->id); });
        UndoButton().Click([this](auto const&, auto const&) { ExecuteEditorCommand(folia::Command{folia::CommandKind::Undo}); });
        RedoButton().Click([this](auto const&, auto const&) { ExecuteEditorCommand(folia::Command{folia::CommandKind::Redo}); });
        EditCutButton().Click([this](auto const&, auto const&) { documentController().CutSelection(); });
        EditCopyButton().Click([this](auto const&, auto const&) { documentController().CopySelection(); });
        EditPasteButton().Click([this](auto const&, auto const&) { documentController().PasteClipboard(); });
        FindButton().Click([this](auto const&, auto const&) { ShowFindBar(false); });
        ReplaceButton().Click([this](auto const&, auto const&) { ShowFindBar(true); });
        OpenButton().Click([this](auto const&, auto const&)
        {
            PickWorkspaceFile();
        });

        ReloadButton().Click([this](auto const&, auto const&)
        {
            documentController().ReloadDocument();
        });

        SaveButton().Click([this](auto const&, auto const&)
        {
            documentController().SaveDocument();
        });

        ExportPdfButton().Click([this](auto const&, auto const&)
        {
            documentController().ExportPdf();
        });

        CancelOperationButton().Click([this](auto const&, auto const&)
        {
            documentController().CancelOperation();
        });

        ReloadExternalButton().Click([this](auto const&, auto const&)
        {
            documentController().AcceptExternalReload();
        });

        KeepLocalButton().Click([this](auto const&, auto const&)
        {
            documentController().KeepLocalVersion();
        });

        SettingsButton().Click([this](auto const&, auto const&)
        {
            ToggleSettingsMode();
        });

        BoldButton().Click([this](auto const&, auto const&)
        {
            folia::Command command;
            command.kind = folia::CommandKind::ToggleStrong;
            ExecuteEditorCommand(command);
        });

        ItalicButton().Click([this](auto const&, auto const&)
        {
            folia::Command command;
            command.kind = folia::CommandKind::ToggleEmphasis;
            ExecuteEditorCommand(command);
        });

        StrikeButton().Click([this](auto const&, auto const&)
        {
            folia::Command command;
            command.kind = folia::CommandKind::ToggleStrikethrough;
            ExecuteEditorCommand(command);
        });

        InlineCodeButton().Click([this](auto const&, auto const&)
        {
            folia::Command command;
            command.kind = folia::CommandKind::ToggleInlineCode;
            ExecuteEditorCommand(command);
        });

        Heading1Button().Click([this](auto const&, auto const&)
        {
            folia::Command command;
            command.kind = folia::CommandKind::SetHeading;
            command.level = 1;
            ExecuteEditorCommand(command);
        });

        Heading2Button().Click([this](auto const&, auto const&)
        {
            folia::Command command;
            command.kind = folia::CommandKind::SetHeading;
            command.level = 2;
            ExecuteEditorCommand(command);
        });

        QuoteButton().Click([this](auto const&, auto const&)
        {
            folia::Command command;
            command.kind = folia::CommandKind::ToggleBlockQuote;
            ExecuteEditorCommand(command);
        });

        UnorderedListButton().Click([this](auto const&, auto const&)
        {
            folia::Command command;
            command.kind = folia::CommandKind::ToggleUnorderedList;
            ExecuteEditorCommand(command);
        });

        OrderedListButton().Click([this](auto const&, auto const&)
        {
            folia::Command command;
            command.kind = folia::CommandKind::ToggleOrderedList;
            ExecuteEditorCommand(command);
        });

        TaskListButton().Click([this](auto const&, auto const&)
        {
            folia::Command command;
            command.kind = folia::CommandKind::ToggleTaskList;
            ExecuteEditorCommand(command);
        });

        CodeBlockButton().Click([this](auto const&, auto const&)
        {
            folia::Command command;
            command.kind = folia::CommandKind::InsertCodeBlock;
            ExecuteEditorCommand(command);
        });

        TableButton().Click([this](auto const&, auto const&)
        {
            folia::Command command;
            command.kind = folia::CommandKind::InsertTable;
            command.rows = 2;
            command.cols = 3;
            ExecuteEditorCommand(command);
        });

        InlineMathButton().Click([this](auto const&, auto const&)
        {
            folia::Command command;
            command.kind = folia::CommandKind::InsertMathInline;
            ExecuteEditorCommand(command);
        });

        BlockMathButton().Click([this](auto const&, auto const&)
        {
            folia::Command command;
            command.kind = folia::CommandKind::InsertMathBlock;
            ExecuteEditorCommand(command);
        });

        LinkButton().Click([this](auto const&, auto const&)
        {
            folia::Command command;
            command.kind = folia::CommandKind::InsertLink;
            command.href = U"https://";
            ExecuteEditorCommand(command);
        });

        ImageButton().Click([this](auto const&, auto const&)
        {
            documentController().InsertImage();
        });

        FootnoteButton().Click([this](auto const&, auto const&)
        {
            folia::Command command;
            command.kind = folia::CommandKind::InsertFootnote;
            ExecuteEditorCommand(command);
        });

        auto toggleCallout = [this](std::u32string kind)
        {
            folia::Command command;
            command.kind = folia::CommandKind::ToggleCallout;
            command.callout_kind = std::move(kind);
            ExecuteEditorCommand(command);
        };
        CalloutNoteMenuItem().Click([toggleCallout](auto const&, auto const&) { toggleCallout(U"NOTE"); });
        CalloutTipMenuItem().Click([toggleCallout](auto const&, auto const&) { toggleCallout(U"TIP"); });
        CalloutWarningMenuItem().Click([toggleCallout](auto const&, auto const&) { toggleCallout(U"WARNING"); });

        FormatDocumentButton().Click([this](auto const&, auto const&)
        {
            folia::Command command;
            command.kind = folia::CommandKind::FormatDocument;
            ExecuteEditorCommand(command);
        });

        TocButton().Click([this](auto const&, auto const&)
        {
            folia::Command command;
            command.kind = folia::CommandKind::InsertToc;
            ExecuteEditorCommand(command);
        });

        CutMenuItem().Click([this](auto const&, auto const&)
        {
            documentController().CutSelection();
        });

        CopyMenuItem().Click([this](auto const&, auto const&)
        {
            documentController().CopySelection();
        });

        PasteMenuItem().Click([this](auto const&, auto const&)
        {
            documentController().PasteClipboard();
        });

        SelectAllMenuItem().Click([this](auto const&, auto const&)
        {
            folia::Command command;
            command.kind = folia::CommandKind::SelectAll;
            ExecuteEditorCommand(command);
        });
    }
}
