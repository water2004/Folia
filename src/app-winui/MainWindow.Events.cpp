#include "pch.h"
#include "MainWindow.xaml.h"
#include "localization/Localization.h"

import folia.core.command;

namespace winrt::Folia::implementation
{
    void MainWindow::AttachControllers()
    {
        AttachDocumentController(workspaceDocuments.Active());
        sidebarController.Attach(
            editorSession(),
            editorRenderer,
            textInputController,
            OutlineList(),
            [this] { RenderEditorSurface(); });
        keyboardController.Attach(
            editorSession(),
            editorRenderer,
            textInputController,
            [this](folia::Command const& command) { return ExecuteEditorCommand(command); },
            [this] { documentController().CopySelection(); },
            [this] { documentController().CutSelection(); },
            [this] { documentController().PasteClipboard(); },
            [this](std::string_view action)
            {
                DispatchApplicationAction(action);
            },
            appSettings.shortcutBindings,
            [this] { RenderEditorSurface(); });
        latexCompletionController.Attach(
            editorSession(),
            editorRenderer,
            EditorSurface(),
            EditorOverlay(),
            LatexSuggestionPopup(),
            LatexSuggestionPrefix(),
            LatexSuggestionList(),
            latexCommandCatalog,
            [this](folia::NodeId container, folia::SourceRange replacement, std::u32string_view snippet)
            {
                return keyboardController.InsertSnippetReplacing(
                    container, replacement, snippet);
            });
        latexCompletionController.SetEnabled(appSettings.latexSuggestionsEnabled);
        auto languagePicker = Root().Resources().Lookup(winrt::box_value(L"CodeLanguagePickerTemplate"))
            .as<Microsoft::UI::Xaml::DataTemplate>().LoadContent().as<Microsoft::UI::Xaml::Controls::Grid>();
        CodeLanguageFlyout().Content(languagePicker);
        codeLanguageController.Attach(
            editorSession(),
            editorRenderer,
            EditorSurface(),
            CodeLanguageFlyout(),
            languagePicker.FindName(L"Search").as<Microsoft::UI::Xaml::Controls::TextBox>(),
            languagePicker.FindName(L"Languages").as<Microsoft::UI::Xaml::Controls::ListView>(),
            [this](folia::Command const& command) { return ExecuteEditorCommand(command); });
        pointerController.Attach(
            editorSession(),
            editorRenderer,
            scrollController,
            textInputController,
            EditorSurface(),
            [this](folia::Command const& command) { return ExecuteEditorCommand(command); },
            [this] { RenderEditorSurface(); },
            [this](std::string href) { documentController().OpenLink(std::move(href)); },
            [this](auto hit, auto position) { ShowFootnoteFlyout(hit, position); },
            [this]
            {
                keyboardController.ResetCaretGoal();
                keyboardController.CancelSnippetSession();
                latexCompletionController.Cancel();
            });
    }

    void MainWindow::RegisterWindowHandlers()
    {
        Activated(WindowEvent([this](auto const&, Microsoft::UI::Xaml::WindowActivatedEventArgs const& args) {
            auto editing = args.WindowActivationState() != Microsoft::UI::Xaml::WindowActivationState::Deactivated
                && EditorSurface().FocusState() != Microsoft::UI::Xaml::FocusState::Unfocused && !settingsMode;
            animationController.Focus(editing);
            if (editing) textInputController.FocusEnter();
            else textInputController.FocusLeave();
        }));
        closingHandler.Attach(AppWindow(),
            [this](auto const&, Microsoft::UI::Windowing::AppWindowClosingEventArgs const& args)
            {
                if (closeApproved) return;
                args.Cancel(true);
                // Even an already-clean document must close outside this
                // synchronous Closing notification, without nested close events.
                DispatcherQueue().TryEnqueue([weak = get_weak()]
                {
                    if (auto window = weak.get()) window->ConfirmWindowCloseAsync();
                });
            });
        Closed(WindowEvent([this](auto const&, auto const&)
        {
            windowClosed = true;
            tabDragEnded.Reset();
            preparedTabActivation.Reset();
            EndTabDrag();
            animationController.Detach();
            closingHandler.Reset();
            if (latexCommandCatalog) latexCommandCatalog->Flush();
            if (settingsView) settingsView->Detach();
            folderExplorer.Detach();
            for (auto const& document : workspaceDocuments.Items()) document->controller.Detach();
            sidebarController.Detach();
            pointerController.Detach();
            latexCompletionController.Detach();
            codeLanguageController.Detach();
            keyboardController.Detach();
            textInputController.Detach();
            scrollController.Detach();
        }));

        SourceModeButton().Click(WindowEvent([this](auto const&, auto const&)
        {
            ToggleSourceMode();
        }));

        EncodingButton().Click(WindowEvent([this](auto const&, auto const&)
        {
            auto menu = Microsoft::UI::Xaml::Controls::MenuFlyout();
            auto reopen = Microsoft::UI::Xaml::Controls::MenuFlyoutItem();
            reopen.Text(Localize(L"ReopenWithEncoding"));
            reopen.Click(WindowEvent([this](auto const&, auto const&)
            {
                ShowEncodingPicker(false);
            }));
            menu.Items().Append(reopen);
            auto save = Microsoft::UI::Xaml::Controls::MenuFlyoutItem();
            save.Text(Localize(L"SaveWithEncoding"));
            save.Click(WindowEvent([this](auto const&, auto const&)
            {
                ShowEncodingPicker(true);
            }));
            menu.Items().Append(save);
            menu.ShowAt(EncodingButton());
        }));

        SidebarToggleButton().Click(WindowEvent([this](auto const&, auto const&)
        {
            outlineOpen = SidebarToggleButton().IsChecked();
            UpdateWorkspaceLayout();
        }));

        Root().Loaded(WindowEvent([this](auto const&, auto const&) { UpdateChromeMetrics(); }));
        Root().SizeChanged(WindowEvent([this](auto const&, auto const&) { UpdateChromeMetrics(); }));

        EditorSurface().Loaded(WindowEvent([this](auto const&, auto const&)
        {
            InitializeEditorSurface();
        }));

        EditorSurface().SizeChanged(WindowEvent([this](auto const&, Microsoft::UI::Xaml::SizeChangedEventArgs const& args)
        {
            ResizeEditorSurface(args.NewSize().Width, args.NewSize().Height);
        }));

        WorkspaceHost().SizeChanged(WindowEvent([this](auto const&, Microsoft::UI::Xaml::SizeChangedEventArgs const& args)
        {
            UpdateWorkspaceLayout();
        }));

        EditorSurface().CompositionScaleChanged(WindowEvent([this](auto const&, auto const&)
        {
            UpdateChromeMetrics();
            ResizeEditorSurface(EditorSurface().ActualWidth(), EditorSurface().ActualHeight());
        }));

        Root().ActualThemeChanged(WindowEvent([this](auto const&, auto const&)
        {
            if (appSettings.themeId == "system") UpdateTheme();
        }));

        EditorSurface().IsTabStop(true);
        EditorSurface().CharacterReceived([this](auto const&, Microsoft::UI::Xaml::Input::CharacterReceivedRoutedEventArgs const& args)
        {
            if (workspaceDocuments.Active()->opening || documentController().IsLoading()) { args.Handled(true); return; }
            auto handled = keyboardController.Character(args.Character());
            if (handled) latexCompletionController.Refresh();
            args.Handled(handled);
        });

        EditorSurface().KeyDown(WindowEvent([this](auto const&, Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const& args)
        {
            if (latexCompletionController.HandleKey(args.Key()))
            {
                args.Handled(true);
                return;
            }
            auto handled = keyboardController.Key(args.Key());
            if (handled) latexCompletionController.Refresh();
            args.Handled(handled);
        }));

        auto enterAccelerator = Microsoft::UI::Xaml::Input::KeyboardAccelerator();
        enterAccelerator.Key(winrt::Windows::System::VirtualKey::Enter);
        enterAccelerator.Invoked([this](auto const&, Microsoft::UI::Xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args)
        {
            if (latexCompletionController.AcceptSelected())
            {
                args.Handled(true);
                return;
            }
            auto handled = keyboardController.InsertNewline();
            if (handled) latexCompletionController.Refresh();
            args.Handled(handled);
        });
        EditorSurface().KeyboardAccelerators().Append(enterAccelerator);

        auto escapeAccelerator = Microsoft::UI::Xaml::Input::KeyboardAccelerator();
        escapeAccelerator.Key(winrt::Windows::System::VirtualKey::Escape);
        escapeAccelerator.Invoked([this](auto const&, auto const& args)
        {
            if (latexCompletionController.Active())
            {
                latexCompletionController.Cancel();
                args.Handled(true);
                return;
            }
            if (FindReplaceBar().Visibility() != Microsoft::UI::Xaml::Visibility::Visible) return;
            HideFindBar();
            args.Handled(true);
        });
        Root().KeyboardAccelerators().Append(escapeAccelerator);

        FindQueryBox().TextChanged(WindowEvent([this](auto const&, auto const&) { RefreshSearch(true); }));
        FindRegexButton().Click(WindowEvent([this](auto const&, auto const&) { RefreshSearch(true); }));
        FindCaseButton().Click(WindowEvent([this](auto const&, auto const&) { RefreshSearch(true); }));
        FindPreviousButton().Click(WindowEvent([this](auto const&, auto const&) { NavigateSearch(-1); }));
        FindNextButton().Click(WindowEvent([this](auto const&, auto const&) { NavigateSearch(1); }));
        CloseFindButton().Click(WindowEvent([this](auto const&, auto const&) { HideFindBar(); }));
        ReplaceCurrentButton().Click(WindowEvent([this](auto const&, auto const&) { ReplaceCurrentSearchMatch(); }));
        ReplaceAllButton().Click(WindowEvent([this](auto const&, auto const&) { ReplaceAllSearchMatches(); }));
        FindQueryBox().KeyDown(WindowEvent([this](auto const&, Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const& args)
        {
            if (args.Key() != winrt::Windows::System::VirtualKey::Enter) return;
            auto shift = Microsoft::UI::Input::InputKeyboardSource::GetKeyStateForCurrentThread(
                winrt::Windows::System::VirtualKey::Shift);
            NavigateSearch((static_cast<std::uint32_t>(shift) & 0x1u) != 0 ? -1 : 1);
            args.Handled(true);
        }));
        ReplaceTextBox().KeyDown(WindowEvent([this](auto const&, Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const& args)
        {
            if (args.Key() != winrt::Windows::System::VirtualKey::Enter) return;
            ReplaceCurrentSearchMatch();
            args.Handled(true);
        }));

        EditorSurface().PointerPressed(WindowEvent([this](auto const&, Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args)
        {
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
        }));

        EditorSurface().PointerMoved(WindowEvent([this](auto const&, Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args)
        {
            pointerController.PointerMoved(args);
        }));

        EditorSurface().PointerReleased(WindowEvent([this](auto const&, Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args)
        {
            pointerController.PointerReleased(args);
        }));

        EditorSurface().PointerExited(WindowEvent([this](auto const&, auto const&)
        {
            pointerController.PointerExited();
        }));

        EditorSurface().PointerCaptureLost(WindowEvent([this](auto const&, auto const&)
        {
            pointerController.CancelPointerInteraction();
        }));

        EditorSurface().PointerCanceled(WindowEvent([this](auto const&, auto const&)
        {
            pointerController.CancelPointerInteraction();
        }));

        EditorSurface().PointerWheelChanged(WindowEvent([this](auto const&, Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args)
        {
            HandlePointerWheel(args);
        }));

        EditorSurface().GotFocus(WindowEvent([this](auto const&, auto const&)
        {
            animationController.Focus(true);
            textInputController.FocusEnter();
        }));

        EditorSurface().LostFocus(WindowEvent([this](auto const&, auto const&)
        {
            animationController.Focus(false);
            pointerController.CancelPointerInteraction();
            textInputController.FocusLeave();
        }));

        EditorSurface().DoubleTapped(WindowEvent([this](auto const&, Microsoft::UI::Xaml::Input::DoubleTappedRoutedEventArgs const& args)
        {
            pointerController.DoubleTapped(args);
        }));

        OutlineList().ItemClick(WindowEvent([this](auto const&, Microsoft::UI::Xaml::Controls::ItemClickEventArgs const& args)
        {
            sidebarController.SelectOutline(args.ClickedItem());
        }));
        OutlineList().KeyUp(WindowEvent([this](auto const&, Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const& args)
        {
            // ItemClick handles mouse/Enter/Space activation. After the native
            // list has moved its keyboard selection, navigate that item too.
            using winrt::Windows::System::VirtualKey;
            switch (args.Key())
            {
            case VirtualKey::Up: case VirtualKey::Down:
            case VirtualKey::Home: case VirtualKey::End:
            case VirtualKey::PageUp: case VirtualKey::PageDown:
                sidebarController.SelectOutline(OutlineList().SelectedItem());
                break;
            }
        }));

    }
}
