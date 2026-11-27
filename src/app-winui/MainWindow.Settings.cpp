#include "pch.h"
#include "App.xaml.h"
#include "MainWindow.xaml.h"
#include "agent/ChatView.h"
#include "localization/Localization.h"

namespace winrt::Folia::implementation
{
    void MainWindow::SetDocumentCommandsVisible(bool visible)
    {
        auto value = visible
            ? Microsoft::UI::Xaml::Visibility::Visible
            : Microsoft::UI::Xaml::Visibility::Collapsed;
        ReloadButton().Visibility(value);
        SaveButton().Visibility(value);
        SaveAsButton().Visibility(value);
        ExportPdfButton().Visibility(value);
        BoldButton().Visibility(value);
        ItalicButton().Visibility(value);
        StrikeButton().Visibility(value);
        InlineCodeButton().Visibility(value);
        BlocksButton().Visibility(value);
        InsertButton().Visibility(value);
        FormatDocumentButton().Visibility(value);
    }

    std::optional<winrt::hstring> MainWindow::ApplySettings(winrt::Folia::AppSettings const& proposed)
    {
        auto settings = proposed;
        settings.textLayouts = appSettings.textLayouts;
        if (auto saveError = winrt::Folia::SaveAppSettings(settings))
        {
            ShowError(*saveError);
            return saveError;
        }
        auto themeChanged = appSettings.themeId != settings.themeId;
        auto appearanceChanged = appSettings.appearance != settings.appearance;
        auto mediaChanged = appSettings.media != settings.media;
        auto openingChanged = appSettings.openFilesInNewWindow != settings.openFilesInNewWindow;
        auto shortcutsChanged = appSettings.shortcutBindings != settings.shortcutBindings;
        auto latexSuggestionsChanged =
            appSettings.latexSuggestionsEnabled != settings.latexSuggestionsEnabled;
        appSettings = settings;
        ConfigureInlineCompletion();
        if (shortcutsChanged) {
            keyboardController.SetShortcuts(appSettings.shortcutBindings);
            RegisterApplicationShortcuts();
        }
        if (latexSuggestionsChanged)
            latexCompletionController.SetEnabled(appSettings.latexSuggestionsEnabled);
        if (themeChanged) UpdateTheme();
        if (mediaChanged) ApplyMediaToViews();
        if (appearanceChanged) {
            ApplyBarLayout();
            if (!themeChanged) ApplyWorkspaceMaterial();
        }
        if (themeChanged || mediaChanged || appearanceChanged || openingChanged)
            App::Current()->UpdateWindowViewSettings(this, appSettings);
        return std::nullopt;
    }

    std::unique_ptr<ui::TextLayoutControls> MainWindow::BindReadingTextLayout(
        Microsoft::UI::Xaml::Controls::Button const& button, winrt::Folia::DocumentTools& toolbar)
    {
        auto panel = ui::AttachTextLayoutFlyout(button, appSettings.textLayouts.reading,
            [weak = get_weak()](TextLayoutOptions const& value) {
                if (auto window = weak.get(); window && !window->windowClosed)
                    window->ChangeTextLayout(TextLayoutTarget::Reading, value);
            });
        panel->SetTheme(themeProfile);
        auto flyout = button.Flyout();
        flyout.Opening(WindowEvent([&toolbar](auto const&, auto const&) { toolbar.HoldOpen(true); }));
        flyout.Closed(WindowEvent([&toolbar](auto const&, auto const&) { toolbar.HoldOpen(false); }));
        return panel;
    }

    void MainWindow::ChangeTextLayout(TextLayoutTarget target, TextLayoutOptions const& value)
    {
        auto profiles = appSettings.textLayouts;
        (target == TextLayoutTarget::Reading ? profiles.reading : profiles.chat) = value;
        if (profiles == appSettings.textLayouts) return;
        App::Current()->ChangeTextLayouts(*this, profiles);
    }

    void MainWindow::UpdateTextLayouts(TextLayoutProfiles const& value)
    {
        if (appSettings.textLayouts == value) return;
        appSettings.textLayouts = value;
        if (readingTextLayout) readingTextLayout->SetOptions(value.reading);
        if (otherTextLayout) otherTextLayout->SetOptions(value.reading);
        if (settingsView) settingsView->UpdateTextLayouts(value);
        if (textLayoutRefreshQueued) return;
        textLayoutRefreshQueued = DispatcherQueue().TryEnqueue(
            Microsoft::UI::Dispatching::DispatcherQueuePriority::Low, [weak = get_weak()] {
                if (auto window = weak.get(); window && !window->windowClosed) window->RefreshTextLayout();
            });
    }

    void MainWindow::RefreshTextLayout()
    {
        textLayoutRefreshQueued = false;
        auto const& profiles = appSettings.textLayouts;
        if (editorRenderer.SetTextLayout(profiles.reading)) animationController.RequestFrame();
        if (otherEditorRenderer && otherEditorRenderer->SetTextLayout(profiles.reading)) QueueOtherGroupRender();
        for (auto const& [_, chat] : agentChatViews) chat->SetTextLayout(profiles.chat);
    }

    void MainWindow::UpdateMediaSettings(winrt::Folia::MediaDisplaySettings const& settings)
    {
        if (appSettings.media == settings) return;
        appSettings.media = settings;
        ApplyMediaToViews();
        if (settingsView) settingsView->UpdateMediaSettings(settings);
    }

    void MainWindow::ApplyMediaToViews()
    {
        editorRenderer.SetMediaDisplaySettings(appSettings.media);
        if (otherEditorRenderer) otherEditorRenderer->SetMediaDisplaySettings(appSettings.media);
        UpdateAgentChatMedia();
        QueueOtherGroupRender();
        if (!SettingsActive()) RenderEditorSurface();
    }

    void MainWindow::OpenSettingsTab()
    {
        if (closePending || closingDocument || windowClosed) return;
        auto tab = workspaceDocuments.OpenSettings();
        ActivateDocument(tab->id);
        RefreshDocumentTabs();
    }

    void MainWindow::ReturnToDocumentTab()
    {
        if (!SettingsActive()) return;
        auto view = previousDocumentTab ? FindDocumentView(*previousDocumentTab) : 0;
        if (!view) view = editorGroups.Replacement(SelectedEditorView(), -1);
        if (view) ActivateView(view);
        else NewDocument();
    }

    void MainWindow::ShowSettingsPage(bool enabled)
    {
        if (enabled)
        {
            animationController.Focus(false);
            PlaceSettingsView(true);
            if (!settingsView)
            {
                settingsView = std::make_shared<winrt::Folia::SettingsView>(
                    SettingsViewHost(),
                    appSettings,
                    themeCatalog,
                    latexCommandCatalog,
                    CurrentThemeVariant(),
                    WindowHandle(),
                    [this](winrt::Folia::AppSettings const& changed) { return ApplySettings(changed); },
                    [this](TextLayoutTarget target, TextLayoutOptions const& value) { ChangeTextLayout(target, value); });
            }
            DocumentPages().Visibility(Microsoft::UI::Xaml::Visibility::Collapsed);
            SettingsViewHost().Visibility(Microsoft::UI::Xaml::Visibility::Visible);
            ApplyBarLayout();
            Title(Localize(L"WindowTitleSettings"));
            settingsView->Focus();
            return;
        }

        settingsView->CancelShortcutCapture();
        SettingsViewHost().Visibility(Microsoft::UI::Xaml::Visibility::Collapsed);
        DocumentPages().Visibility(Microsoft::UI::Xaml::Visibility::Visible);
        ApplyBarLayout();
    }

    void MainWindow::PlaceSettingsView(bool active) {
        using namespace Microsoft::UI::Xaml::Controls;
        auto view = SettingsViewHost();
        auto target = active ? WorkspaceHost() : OtherDocumentViewport();
        auto parent = view.Parent().try_as<Panel>();
        if (parent && parent != target) {
            std::uint32_t index = 0;
            if (parent.Children().IndexOf(view, index)) parent.Children().RemoveAt(index);
            target.Children().Append(view);
        }
        view.IsHitTestVisible(active);
    }
}
