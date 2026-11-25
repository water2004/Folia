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
        EditMenu().Visibility(value);
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

    void MainWindow::ApplyBarLayout()
    {
        using Microsoft::UI::Xaml::Controls::Grid;
        auto const& appearance = appSettings.appearance;
        // The bars keep their grid rows. Only the reading viewport extends into
        // an auto-hidden edge, leaving the middle row safe for notifications.
        const int first = appearance.autoHideTop ? 0 : 1;
        const int end = appearance.autoHideBottom && !settingsMode ? 3 : 2;
        Grid::SetRow(TopEdgeViewport(), first);
        Grid::SetRowSpan(TopEdgeViewport(), end - first);
        topBar.Configure(appearance.autoHideTop);
        bottomBar.Configure(appearance.autoHideBottom, !settingsMode);
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
        if (!settingsMode) RenderEditorSurface();
    }

    void MainWindow::SetSettingsMode(bool enabled)
    {
        if (settingsMode == enabled)
        {
            return;
        }

        settingsMode = enabled;
        if (inlineCompletion) inlineCompletion->SetFocused(false);
        latexCompletionController.Cancel();
        SetDocumentCommandsVisible(!enabled && HasActiveEditor());
        if (!enabled) ReloadButton().Visibility(Microsoft::UI::Xaml::Visibility::Visible);
        if (enabled)
        {
            documentSidebarWasOpen = DocumentWorkspace().IsPaneOpen();
            animationController.Focus(false);
            HideFindBar();
            if (!settingsView)
            {
                settingsView = std::make_shared<winrt::Folia::SettingsView>(
                    SettingsNavigation(), SettingsViewHost(),
                    appSettings,
                    themeCatalog,
                    latexCommandCatalog,
                    CurrentThemeVariant(),
                    WindowHandle(),
                    [this](winrt::Folia::AppSettings const& changed) { return ApplySettings(changed); },
                    [weak = get_weak()] {
                        if (auto window = weak.get(); window && !window->windowClosed) window->SetSettingsMode(false);
                    });
            }
            else
            {
                settingsView->Reset(appSettings, CurrentThemeVariant());
            }
            DocumentNavigation().Visibility(Microsoft::UI::Xaml::Visibility::Collapsed);
            SettingsNavigation().Visibility(Microsoft::UI::Xaml::Visibility::Visible);
            DocumentWorkspace().IsPaneOpen(true);
            UpdateWorkspaceLayout();
            DocumentPages().Visibility(Microsoft::UI::Xaml::Visibility::Collapsed);
            SettingsViewHost().Visibility(Microsoft::UI::Xaml::Visibility::Visible);
            ApplyBarLayout();
            Title(Localize(L"WindowTitleSettings"));
            return;
        }

        SettingsViewHost().Visibility(Microsoft::UI::Xaml::Visibility::Collapsed);
        SettingsNavigation().Visibility(Microsoft::UI::Xaml::Visibility::Collapsed);
        DocumentNavigation().Visibility(Microsoft::UI::Xaml::Visibility::Visible);
        DocumentWorkspace().IsPaneOpen(documentSidebarWasOpen);
        UpdateWorkspaceLayout();
        if (settingsView) settingsView->Reset(appSettings, CurrentThemeVariant());
        DocumentPages().Visibility(Microsoft::UI::Xaml::Visibility::Visible);
        ApplyBarLayout();
        UpdateDocumentInfo();
        RenderEditorSurface();
        if (HasActiveEditor()) EditorSurface().Focus(Microsoft::UI::Xaml::FocusState::Programmatic);
    }
}
