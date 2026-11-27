#include "pch.h"
#include "MainWindow.xaml.h"
#include "localization/Localization.h"
#include "storage/AssetPaths.h"
#include <winrt/Microsoft.UI.Xaml.Automation.h>
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

import folia.core.command;
import folia.markdown.utf;

namespace winrt::Folia::implementation
{
    MainWindow::MainWindow() : MainWindow(winrt::hstring{})
    {
    }

    MainWindow::MainWindow(winrt::hstring const& initialDocumentPath)
    {
        InitializeComponent();
        presentedEditorView = editorGroups.Add(EditorGroup::Left, workspaceDocuments.Active()->id);
        documentContextMenu = EditorSurface().ContextFlyout().as<Microsoft::UI::Xaml::Controls::MenuFlyout>();
        EditorSurface().ContextFlyout(nullptr);
        LocalizeShell();
        auto iconPath = winrt::Folia::AssetPath(std::filesystem::path(L"branding") / L"Folia.ico");
        if (std::filesystem::exists(iconPath)) AppWindow().SetIcon(winrt::hstring(iconPath.c_str()));
        auto loadedSettings = winrt::Folia::LoadAppSettings();
        appSettings = std::move(loadedSettings.settings);
        editorRenderer.SetMediaDisplaySettings(appSettings.media);
        ExtendsContentIntoTitleBar(true);
        AppWindow().TitleBar().PreferredHeightOption(Microsoft::UI::Windowing::TitleBarHeightOption::Standard);
        RegisterCommandHandlers();
        InitializeTextInput();
        AttachControllers();
        UpdateTheme();
        RegisterWindowHandlers();
        RegisterWorkspaceHandlers();
        UpdateSourceModeUi();
        UpdateDocumentInfo();
        if (!loadedSettings.diagnostic.empty()) ShowError(loadedSettings.diagnostic);
        else if (!themeCatalog->Diagnostics().empty()) ShowError(themeCatalog->Diagnostics().front());
        else if (!latexCommandCatalog->Diagnostics().empty())
            ShowError(latexCommandCatalog->Diagnostics().front());
        if (!initialDocumentPath.empty()) OpenWorkspaceFile(std::filesystem::path(initialDocumentPath.c_str()), winrt::Folia::WorkspaceOpenMode::Keep);
    }

    void MainWindow::LocalizeShell()
    {
        using Microsoft::UI::Xaml::Controls::ToolTipService;
        auto tooltip = [](auto const& element, wchar_t const* resource)
        {
            ToolTipService::SetToolTip(element, winrt::box_value(Localize(resource)));
        };

        FormatMenu().Text(Localize(L"FormatMenu"));
        OpenFolderButton().Text(Localize(L"OpenFolder"));
        RecentFilesMenu().Text(Localize(L"OpenRecentFile"));
        RecentFoldersMenu().Text(Localize(L"OpenRecentFolder"));
        OpenFolderPaneButton().Content(winrt::box_value(Localize(L"OpenFolder")));
        NoFolderHeading().Text(Localize(L"NoFolderHeading"));
        NoFolderHint().Text(Localize(L"NoFolderHint"));
        SaveAsButton().Text(Localize(L"ShortcutSaveAs"));
        CloseTabButton().Text(Localize(L"CloseTab"));
        UndoButton().Text(Localize(L"ShortcutUndo"));
        RedoButton().Text(Localize(L"ShortcutRedo"));
        FindButton().Text(Localize(L"Find"));
        ReplaceButton().Text(Localize(L"Replace"));
        tooltip(SidebarButton(), L"ToggleSidebar");
        OutlineEmptyText().Text(Localize(L"NoOutline"));

        OpenButton().Text(Localize(L"Open"));
        tooltip(OpenButton(), L"OpenTooltip");
        ReloadButton().Text(Localize(L"Reload"));
        tooltip(ReloadButton(), L"ReloadTooltip");
        SaveButton().Text(Localize(L"Save"));
        tooltip(SaveButton(), L"SaveTooltip");
        ExportPdfButton().Text(Localize(L"Pdf"));
        tooltip(ExportPdfButton(), L"PdfTooltip");
        BoldButton().Text(Localize(L"Bold"));
        tooltip(BoldButton(), L"Bold");
        ItalicButton().Text(Localize(L"Italic"));
        tooltip(ItalicButton(), L"Italic");
        StrikeButton().Text(Localize(L"Strike"));
        tooltip(StrikeButton(), L"Strikethrough");
        InlineCodeButton().Text(Localize(L"InlineCode"));
        tooltip(InlineCodeButton(), L"InlineCode");
        BlocksButton().Text(Localize(L"Blocks"));
        tooltip(BlocksButton(), L"BlockStructure");
        HeadingMenuItem().Text(Localize(L"Heading"));
        Heading1Button().Text(Localize(L"Heading1"));
        Heading2Button().Text(Localize(L"Heading2"));
        QuoteButton().Text(Localize(L"Quote"));
        ListMenuItem().Text(Localize(L"List"));
        UnorderedListButton().Text(Localize(L"BulletedList"));
        OrderedListButton().Text(Localize(L"NumberedList"));
        TaskListButton().Text(Localize(L"TaskList"));
        CodeBlockButton().Text(Localize(L"CodeBlock"));
        BlockMathButton().Text(Localize(L"MathBlock"));
        CalloutMenuItem().Text(Localize(L"Callout"));
        CalloutNoteMenuItem().Text(Localize(L"Note"));
        CalloutTipMenuItem().Text(Localize(L"Tip"));
        CalloutWarningMenuItem().Text(Localize(L"Warning"));
        FormatDocumentButton().Text(Localize(L"FormatDocument"));
        tooltip(FormatDocumentButton(), L"FormatDocument");
        InsertButton().Text(Localize(L"Insert"));
        tooltip(InsertButton(), L"InsertContent");
        ReloadExternalButton().Content(winrt::box_value(Localize(L"Reload")));
        KeepLocalButton().Content(winrt::box_value(Localize(L"KeepLocalChanges")));
        TableButton().Text(Localize(L"Table"));
        InlineMathButton().Text(Localize(L"InlineMath"));
        LinkButton().Text(Localize(L"Link"));
        ImageButton().Text(Localize(L"Image"));
        FootnoteButton().Text(Localize(L"Footnote"));
        TocButton().Text(Localize(L"TableOfContents"));
        tooltip(SettingsButton(), L"Settings");
        CutMenuItem().Text(Localize(L"Cut"));
        CopyMenuItem().Text(Localize(L"Copy"));
        PasteMenuItem().Text(Localize(L"Paste"));
        SelectAllMenuItem().Text(Localize(L"SelectAll"));
        CancelOperationButton().Content(winrt::box_value(Localize(L"CancelOperation")));
        tooltip(EncodingButton(), L"SelectFileEncoding");
        tooltip(SourceModeButton(), L"SourceMode");
        FindQueryBox().PlaceholderText(Localize(L"Find"));
        ReplaceTextBox().PlaceholderText(Localize(L"Replace"));
        ReplaceCurrentButton().Content(winrt::box_value(Localize(L"Replace")));
        ReplaceAllButton().Content(winrt::box_value(Localize(L"ReplaceAll")));
        tooltip(FindRegexButton(), L"RegularExpression");
        tooltip(FindCaseButton(), L"MatchCase");
        tooltip(FindPreviousButton(), L"PreviousMatch");
        tooltip(FindNextButton(), L"NextMatch");
        tooltip(CloseFindButton(), L"Close");
        LatexSuggestionHint().Text(Localize(L"LatexSuggestionHint"));
    }

    winrt::hstring MainWindow::LocalizedDocumentName()
    {
        auto active = workspaceDocuments.Active();
        if (active->kind == WorkspaceTabKind::Chat) return to_hstring(active->chatTitle);
        if (active->editor && editorSession().HasFile()) return editorSession().DisplayName();
        if (!active->path.empty()) return winrt::hstring(active->path.filename().c_str());
        return Localize(SettingsActive() ? L"Settings" : L"UntitledMarkdown");
    }

    void MainWindow::UpdateDocumentInfo()
    {
        auto const displayName = LocalizedDocumentName();
        auto active = workspaceDocuments.Active();
        if (active->editor) workspaceDocuments.RefreshPath(active->id);
        if (!SettingsActive()) Title(LocalizeFormat(L"WindowTitleDocument", { displayName }));
        ToolbarFilename().Text(displayName);
        QueueOtherGroupRender();
        if (!active->editor) { UpdateDocumentTab(active->id); return; }
        CharacterCountText().Text(LocalizeFormat(
            L"CharacterCount", { winrt::to_hstring(editorSession().CharacterCount()) }));
        auto encodingLabel = winrt::to_hstring(editorSession().Encoding().name);
        if (editorSession().Encoding().byteOrderMark == folia::encoding::DocumentByteOrderMark::Utf8)
            encodingLabel = encodingLabel + L" BOM";
        EncodingButton().Content(winrt::box_value(encodingLabel));
        EncodingButton().IsEnabled(editorSession().HasFile());
        workspaceDocuments.ObserveEdit(active->id);
        UpdateDocumentTab(active->id);
    }

    void MainWindow::UpdateSourceModeUi()
    {
        auto source = editorSession().IsSourceMode();
        SourceModeButton().IsChecked(source);
        BoldButton().IsEnabled(true);
        ItalicButton().IsEnabled(true);
        StrikeButton().IsEnabled(true);
        InlineCodeButton().IsEnabled(true);
        BlocksButton().IsEnabled(true);
        InsertButton().IsEnabled(true);
    }

    void MainWindow::ToggleSourceMode()
    {
        if (!HasActiveEditor() || workspaceDocuments.Active()->editor->opening || documentController().IsLoading()) return;
        if (!editorSession().ToggleSourceMode())
        {
            UpdateSourceModeUi();
            return;
        }
        keyboardController.ResetCaretGoal();
        editorRenderer.ResetDocumentCaches();
        UpdateSourceModeUi();
        UpdateDocumentInfo();
        sidebarController.Refresh();
        textInputController.NotifyTextChanged();
        if (FindReplaceBar().Visibility() == Microsoft::UI::Xaml::Visibility::Visible)
            RefreshSearch(true);
        else
            RenderEditorSurface();
        const auto activePosition = editorSession().Selection().active;
        if (auto cell = editorSession().TableAddress(activePosition))
            if (auto offset = editorRenderer.RevealTableColumn(*cell))
            {
                editorSession().SetTableScrollOffset(cell->table_id, *offset);
                RenderEditorSurface();
            }
        if (editorRenderer.ScrollToPosition(activePosition)) RenderEditorSurface();
        if (FindReplaceBar().Visibility() != Microsoft::UI::Xaml::Visibility::Visible)
            EditorSurface().Focus(Microsoft::UI::Xaml::FocusState::Programmatic);
    }

    void MainWindow::InitializeTextInput()
    {
        textInputController.Attach(
            editorSession(),
            editorRenderer,
            EditorSurface(),
            [this](folia::Command const& command) { return ExecuteEditorCommand(command); },
            [this] { RenderEditorSurface(); },
            [this] { return WindowHandle(); });
        // Rebinding a document can keep XAML focus on the same surface, so no
        // new GotFocus event is guaranteed for its new CoreText edit context.
        if (EditorSurface().FocusState() != Microsoft::UI::Xaml::FocusState::Unfocused
            && ::GetActiveWindow() == WindowHandle()) textInputController.FocusEnter();
    }

    bool MainWindow::ExecuteEditorCommand(folia::Command const& command)
    {
        if (windowClosed || SettingsActive() || !HasActiveEditor()
            || workspaceDocuments.Active()->editor->opening || documentController().IsLoading()) return false;
        if (command.kind == folia::CommandKind::CopyTable) return documentController().CopyTable();
        if (command.kind == folia::CommandKind::CopyPlainTable) return documentController().CopyTable(false, true);
        keyboardController.ResetCaretGoal();
        auto oldRevision = editorSession().Revision();
        auto beforeSelection = editorSession().Selection();
        if (!editorSession().ExecuteCommand(command))
        {
            return false;
        }

        const bool changed = editorSession().Revision() != oldRevision;
        const auto selection = editorSession().Selection();
        if (!changed && selection.selects_cells() && beforeSelection == selection) return true;
        if (inlineCompletion) inlineCompletion->Edited(beforeSelection, command);
        if (changed) {
            UpdateDocumentInfo();
            sidebarController.Refresh();
        }
        const auto active = selection.active;
        if (auto cell = editorSession().TableAddress(active))
            if (auto offset = editorRenderer.RevealTableColumn(*cell))
                editorSession().SetTableScrollOffset(cell->table_id, *offset);
        if (changed
            && FindReplaceBar().Visibility() == Microsoft::UI::Xaml::Visibility::Visible)
            RefreshSearch(true);
        else
            RenderEditorSurface();
        if (editorRenderer.ScrollToPosition(active)) RenderEditorSurface();
        if (changed) textInputController.NotifyTextChanged();
        else textInputController.NotifySelectionChanged();
        return true;
    }

    void MainWindow::NavigateToFootnote(folia::TextPosition position)
    {
        editorSession().SetSelection(position, position);
        textInputController.NotifySelectionChanged();
        editorRenderer.ScrollToPosition(position);
        RenderEditorSurface();
        EditorSurface().Focus(Microsoft::UI::Xaml::FocusState::Programmatic);
    }

    void MainWindow::ShowFootnoteFlyout(
        folia::platform::editor::EditorVisualFootnoteHit const& hit,
        Windows::Foundation::Point position)
    {
        using namespace Microsoft::UI::Xaml;
        using namespace Microsoft::UI::Xaml::Controls;
        using namespace Microsoft::UI::Xaml::Controls::Primitives;

        if (footnoteFlyout) footnoteFlyout.Hide();
        footnoteFlyout = Flyout();

        StackPanel panel;
        panel.Spacing(themeProfile.layout.footnote_preview_spacing);
        panel.MaxWidth(themeProfile.layout.footnote_preview_width);

        TextBlock heading;
        heading.Text(L"[^" + winrt::to_hstring(hit.label) + L"]");
        heading.FontWeight(Windows::UI::Text::FontWeights::SemiBold());
        panel.Children().Append(heading);

        const auto isReference = hit.kind == winrt::Folia::EditorFootnoteControlKind::Reference;
        auto target = isReference
            ? editorSession().FootnoteDefinitionTarget(hit.label)
            : editorSession().FirstFootnoteReferenceTarget(hit.label);
        TextBlock preview;
        preview.TextWrapping(TextWrapping::Wrap);
        preview.IsTextSelectionEnabled(false);
        if (isReference)
        {
            auto text = editorSession().FootnotePreview(hit.label);
            preview.Text(target
                ? text.empty() ? Localize(L"EmptyFootnoteDefinition") : winrt::to_hstring(text)
                : Localize(L"ReferenceHasNoDefinition"));
        }
        else
        {
            preview.Text(target ? Localize(L"ReturnToFirstReference") : Localize(L"DefinitionHasNoReference"));
        }
        panel.Children().Append(preview);

        Button action;
        if (target)
        {
            action.Content(winrt::box_value(Localize(isReference ? L"GoToDefinition" : L"BackToReference")));
            action.Click([this, target = *target](auto const&, auto const&)
            {
                if (footnoteFlyout) footnoteFlyout.Hide();
                NavigateToFootnote(target);
            });
        }
        else if (isReference)
        {
            action.Content(winrt::box_value(Localize(L"CreateDefinition")));
            action.Click([this, label = hit.label](auto const&, auto const&)
            {
                if (footnoteFlyout) footnoteFlyout.Hide();
                folia::Command command;
                command.kind = folia::CommandKind::CreateFootnoteDefinition;
                command.footnote_label = label;
                ExecuteEditorCommand(command);
            });
        }
        if (target || isReference) panel.Children().Append(action);

        footnoteFlyout.Content(panel);
        FlyoutShowOptions options;
        options.Position(winrt::box_value(position).as<Windows::Foundation::IReference<Windows::Foundation::Point>>());
        options.ShowMode(FlyoutShowMode::Standard);
        footnoteFlyout.ShowAt(EditorSurface(), options);
    }

    void MainWindow::ShowEditorContextMenu(Microsoft::UI::Xaml::Input::ContextRequestedEventArgs const& args)
    {
        if (!HasActiveEditor() || documentController().IsLoading()) return;
        auto surface = EditorSurface();
        Windows::Foundation::Point point{};
        auto location = args.TryGetPosition(surface, point) ? std::optional{point} : std::nullopt;
        auto menu = pointerController.ContextMenuAt(location);
        if (!menu) menu = documentContextMenu;
        if (!location)
            if (auto bounds = editorRenderer.CaretBounds(editorSession().Selection().active))
                location = Windows::Foundation::Point{bounds->left, bounds->bottom};
        Microsoft::UI::Xaml::Controls::Primitives::FlyoutShowOptions options;
        if (location) options.Position(*location);
        if (activeEditorContextMenu && activeEditorContextMenu != menu) activeEditorContextMenu.Hide();
        activeEditorContextMenu = menu;
        args.Handled(true);
        menu.ShowAt(surface, options);
    }

    void MainWindow::HandleEditorPointerPressed(Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args)
    {
        if (!HasActiveEditor() || workspaceDocuments.Active()->editor->opening || documentController().IsLoading()) return;
        auto point = args.GetCurrentPoint(EditorSurface());
        if (point.Properties().IsLeftButtonPressed()) {
            auto at = point.Position();
            if (auto code = editorRenderer.CodeLanguageAt(static_cast<float>(at.X), static_cast<float>(at.Y))) {
                scrollController.Stop();
                editorSession().SetSelection(folia::TextSelection::caret(code->contentStart));
                textInputController.NotifySelectionChanged();
                RenderEditorSurface();
                codeLanguageController.Open(code->contentStart.container_id);
                args.Handled(true);
                return;
            }
        }
        pointerController.PointerPressed(args);
    }

    void MainWindow::HandlePointerWheel(Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args)
    {
        // Wheel input can arrive much faster than the presentation cadence.
        // Clearing an already inactive suggestion ListView on every pulse
        // needlessly invalidates the XAML tree and makes a continuous wheel
        // gesture compete with the editor frame scheduler.
        if (latexCompletionController.Active()) latexCompletionController.Cancel();
        auto pointer = args.GetCurrentPoint(EditorSurface());
        auto delta = pointer.Properties().MouseWheelDelta();
        using Modifiers = winrt::Windows::System::VirtualKeyModifiers;
        const bool horizontal = pointer.Properties().IsHorizontalMouseWheel();
        const bool shift = (args.KeyModifiers() & Modifiers::Shift) == Modifiers::Shift;
        if ((horizontal || shift) && HasActiveEditor() && !SettingsActive())
        {
            const auto point = pointer.Position();
            if (auto target = editorRenderer.ScrollTableAt(point.X, point.Y, static_cast<float>(horizontal ? delta : -delta)))
            {
                editorSession().SetTableScrollOffset(target->first, target->second);
                RenderEditorSurface();
                args.Handled(true);
                return;
            }
        }
        auto scrollDelta = static_cast<float>(-delta);
        if (args.Pointer().PointerDeviceType() == Microsoft::UI::Input::PointerDeviceType::Touchpad)
            scrollController.ScrollPreciselyBy(scrollDelta);
        else
            scrollController.QueueScrollBy(scrollDelta);
        args.Handled(true);
    }

    void MainWindow::SetStatus(winrt::hstring const& text)
    {
        DocumentLoadingText().Text(text);
    }

    void MainWindow::ShowError(winrt::hstring const& text)
    {
        ErrorInfoBar().Message(text);
        ErrorInfoBar().IsOpen(true);
    }

    void MainWindow::SetOperationProgress(
        bool active,
        std::optional<double> value,
        bool cancellable)
    {
        operationActive = active;
        if (value) {
            auto percent = static_cast<int>(std::lround(std::clamp(*value, 0.0, 1.0) * 100));
            OperationRing().Value(percent);
            OperationPercent().Text(winrt::to_hstring(percent) + L"%");
        } else {
            OperationPercent().Text(L"");
            if (!active) OperationRing().Value(0);
        }
        CancelOperationButton().Visibility(active && cancellable
            ? Microsoft::UI::Xaml::Visibility::Visible
            : Microsoft::UI::Xaml::Visibility::Collapsed);
        ReloadButton().IsEnabled(!active);
        ExportPdfButton().IsEnabled(!active);
    }

    HWND MainWindow::WindowHandle()
    {
        HWND hwnd{};
        auto windowNative = get_strong().as<IWindowNative>();
        winrt::check_hresult(windowNative->get_WindowHandle(&hwnd));
        return hwnd;
    }

    void MainWindow::InitializeEditorSurface()
    {
        editorRenderer.Initialize(EditorSurface());
        auto render = [weak = get_weak()] {
            if (auto owner = weak.get(); owner && !owner->windowClosed) owner->RenderEditorSurface();
        };
        scrollController.Attach(editorRenderer, EditorScrollBar(), EditorScrollBarColumn(), [weak = get_weak()] {
            if (auto owner = weak.get(); owner && !owner->windowClosed) owner->animationController.RequestFrame();
        });
        animationController.Attach(editorRenderer, scrollController, DispatcherQueue(), render);
        editorRenderer.SetInvalidateCallback(render);
        RenderEditorSurface();
        EditorSurface().Focus(Microsoft::UI::Xaml::FocusState::Programmatic);
    }

    void MainWindow::ResizeEditorSurface(double width, double height)
    {
        try
        {
            editorRenderer.Resize(EditorSurface(), width, height);
            RenderEditorSurface();
        }
        catch (winrt::hresult_error const& error)
        {
            ShowError(LocalizeFormat(L"ResizeFailed", { error.message() }));
        }
    }

    void MainWindow::RenderEditorSurface()
    {
        if (windowClosed || !HasActiveEditor() || !EditorSurface().IsLoaded()) return;
        try
        {
            auto snippetPlaceholders = keyboardController.SnippetPlaceholders();
            animationController.PrepareRender();
            auto frame = editorSession().RenderFrame();
            frame.snippetPlaceholders = snippetPlaceholders;
            if (inlineCompletion) { inlineCompletion->Validate(); inlineCompletion->Decorate(frame); }
            editorRenderer.Render(frame);
            animationController.Rendered();
            if (latexCompletionController.Active()) latexCompletionController.UpdatePosition();
            sidebarController.SyncViewport();
            codeLanguageController.Refresh();
            auto style = editorSession().ActiveInlineStyle();
            BoldButton().IsChecked(style.bold);
            ItalicButton().IsChecked(style.italic);
            StrikeButton().IsChecked(style.strikethrough);
            InlineCodeButton().IsChecked(style.code == folia::CodeStyle::Inline);
            scrollController.Sync();
        }
        catch (winrt::hresult_error const& error)
        {
            ShowError(LocalizeFormat(L"RenderFailed", { error.message() }));
        }
        catch (std::exception const& error)
        {
            ShowError(LocalizeFormat(L"RenderFailed", { winrt::to_hstring(error.what()) }));
        }
    }
}
