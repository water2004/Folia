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
        if (mediaChanged) {
            editorRenderer.SetMediaDisplaySettings(appSettings.media);
        }
        if (appearanceChanged) {
            ApplyBarLayout();
            if (!themeChanged) ApplyWorkspaceMaterial();
        }
        if (mediaChanged || appearanceChanged)
            App::Current()->UpdateWindowViewSettings(this, appSettings.media, appSettings.appearance);
        return std::nullopt;
    }


    void MainWindow::UpdateWorkspaceAppearance(WorkspaceAppearance const& settings)
    {
        if (appSettings.appearance == settings) return;
        appSettings.appearance = settings;
        ApplyBarLayout();
        ApplyWorkspaceMaterial();
        if (settingsView) settingsView->UpdateWorkspaceAppearance(settings);
    }

    void MainWindow::UpdateMediaSettings(winrt::Folia::MediaDisplaySettings const& settings)
    {
        if (appSettings.media == settings) return;
        appSettings.media = settings;
        editorRenderer.SetMediaDisplaySettings(settings);
        if (settingsView) settingsView->UpdateMediaSettings(settings);
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
        auto tab = previousDocumentTab ? workspaceDocuments.Find(*previousDocumentTab) : nullptr;
        if (!tab) tab = workspaceDocuments.Adjacent(workspaceDocuments.Active()->id, -1);
        if (tab == workspaceDocuments.Active()) NewDocument();
        else ActivateDocument(tab->id);
    }

    void MainWindow::ShowSettingsNavigation(bool enabled)
    {
        if (enabled)
        {
            documentSidebarWasOpen = sidebarOpen && !sidebarClosing;
            animationController.Focus(false);
            if (!settingsView)
            {
                settingsView = std::make_shared<winrt::Folia::SettingsView>(
                    SettingsNavigation(), SettingsViewHost(),
                    appSettings,
                    themeCatalog,
                    latexCommandCatalog,
                    CurrentThemeVariant(),
                    WindowHandle(),
                    [this](winrt::Folia::AppSettings const& changed) { return ApplySettings(changed); });
            }
            DocumentNavigation().Visibility(Microsoft::UI::Xaml::Visibility::Collapsed);
            SettingsNavigation().Visibility(Microsoft::UI::Xaml::Visibility::Visible);
            SetSidebarOpen(true);
            UpdateWorkspaceLayout();
            DocumentPages().Visibility(Microsoft::UI::Xaml::Visibility::Collapsed);
            SettingsViewHost().Visibility(Microsoft::UI::Xaml::Visibility::Visible);
            ApplyBarLayout();
            Title(Localize(L"WindowTitleSettings"));
            SettingsNavigation().Focus(Microsoft::UI::Xaml::FocusState::Programmatic);
            return;
        }

        settingsView->CancelShortcutCapture();
        SettingsViewHost().Visibility(Microsoft::UI::Xaml::Visibility::Collapsed);
        SettingsNavigation().Visibility(Microsoft::UI::Xaml::Visibility::Collapsed);
        DocumentNavigation().Visibility(Microsoft::UI::Xaml::Visibility::Visible);
        SetSidebarOpen(documentSidebarWasOpen);
        UpdateWorkspaceLayout();
        DocumentPages().Visibility(Microsoft::UI::Xaml::Visibility::Visible);
        ApplyBarLayout();
    }
}
