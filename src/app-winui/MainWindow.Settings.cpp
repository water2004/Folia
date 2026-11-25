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
            SetStatus(*saveError);
            return saveError;
        }
        auto themeChanged = appSettings.themeId != settings.themeId;
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
            App::Current()->UpdateWindowMediaSettings(this, appSettings.media);
        }
        return std::nullopt;
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
            animationController.Focus(false);
            HideFindBar();
            if (!settingsView)
            {
                settingsView = std::make_shared<winrt::Folia::SettingsView>(
                    appSettings,
                    themeCatalog,
                    latexCommandCatalog,
                    CurrentThemeVariant(),
                    WindowHandle(),
                    [this](winrt::Folia::AppSettings const& changed) { return ApplySettings(changed); },
                    [weak = get_weak()] {
                        if (auto window = weak.get(); window && !window->windowClosed) window->SetSettingsMode(false);
                    });
                SettingsViewHost().Children().Append(settingsView->Root());
            }
            else
            {
                settingsView->Reset(appSettings, CurrentThemeVariant());
            }
            settingsView->SetNavigationWidth(themeProfile.layout.navigation_open_width);

            DocumentWorkspace().Visibility(Microsoft::UI::Xaml::Visibility::Collapsed);
            SettingsViewHost().Visibility(Microsoft::UI::Xaml::Visibility::Visible);
            StatusBar().Visibility(Microsoft::UI::Xaml::Visibility::Collapsed);
            Title(Localize(L"WindowTitleSettings"));
            return;
        }

        SettingsViewHost().Visibility(Microsoft::UI::Xaml::Visibility::Collapsed);
        if (settingsView) settingsView->Reset(appSettings, CurrentThemeVariant());
        DocumentWorkspace().Visibility(Microsoft::UI::Xaml::Visibility::Visible);
        StatusBar().Visibility(Microsoft::UI::Xaml::Visibility::Visible);
        UpdateDocumentInfo();
        RenderEditorSurface();
        if (HasActiveEditor()) EditorSurface().Focus(Microsoft::UI::Xaml::FocusState::Programmatic);
    }
}
