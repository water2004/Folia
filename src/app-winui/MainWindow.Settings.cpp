#include "pch.h"
#include "App.xaml.h"
#include "MainWindow.xaml.h"
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

    std::optional<winrt::hstring> MainWindow::ApplySettings(winrt::Folia::AppSettings const& settings)
    {
        if (auto saveError = winrt::Folia::SaveAppSettings(settings))
        {
            ShowError(*saveError);
            return saveError;
        }
        auto themeChanged = appSettings.themeId != settings.themeId;
        auto appearanceChanged = appSettings.appearance != settings.appearance;
        auto mediaChanged = appSettings.media != settings.media;
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
        if (themeChanged || mediaChanged || appearanceChanged)
            App::Current()->UpdateWindowViewSettings(this, appSettings);
        return std::nullopt;
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
                    [this](winrt::Folia::AppSettings const& changed) { return ApplySettings(changed); });
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
