#pragma once

#include "settings/storage/AppSettings.h"
#include "settings/latex/LatexCommandCatalog.h"
#include "settings/views/LatexCommandSettingsComponents.h"
#include "settings/views/SettingsBreadcrumb.h"
#include "theme/ThemeCatalog.h"
#include "ui/TextLayoutControls.h"

import folia.platform.editor_shortcut_settings;

namespace winrt::Folia
{
    class SettingsView : public std::enable_shared_from_this<SettingsView>
    {
    public:
        using ApplySettings = std::function<std::optional<winrt::hstring>(AppSettings const&)>;
        using ChangeTextLayout = std::function<void(TextLayoutTarget, TextLayoutOptions const&)>;
        enum class Page { Home, General, TextLayout, Shortcuts, Latex, Completion, Providers, Themes, Licenses, About };

        SettingsView(
            Microsoft::UI::Xaml::Controls::ContentControl const& host,
            AppSettings settings,
            std::shared_ptr<ThemeCatalog> catalog,
            std::shared_ptr<LatexCommandCatalog> latexCatalog,
            folia::Theme systemVariant,
            HWND owner,
            ApplySettings applySettings,
            ChangeTextLayout changeTextLayout = {});

        void Navigate(Page page);
        void Focus();
        winrt::Microsoft::UI::Xaml::UIElement PageContent() const { return pageHost_.Content().as<winrt::Microsoft::UI::Xaml::UIElement>(); }
        void Reset(AppSettings settings, folia::Theme systemVariant);
        void SetSystemVariant(folia::Theme variant);
        void CancelShortcutCapture();
        void UpdateWorkspaceAppearance(WorkspaceAppearance const& appearance);
        void UpdateMediaSettings(MediaDisplaySettings const& media);
        void UpdateTextLayouts(TextLayoutProfiles const&);
        void UpdateAppliedTheme(std::string const& id);
        void UpdateFileOpening(bool newWindow);
        void Detach();

    private:
        winrt::Microsoft::UI::Xaml::UIElement BuildHomePage();
        winrt::Microsoft::UI::Xaml::UIElement BuildGeneralPage();
        winrt::Microsoft::UI::Xaml::UIElement BuildTextLayoutPage();
        winrt::Microsoft::UI::Xaml::UIElement BuildCompletionPage();
        winrt::Microsoft::UI::Xaml::UIElement BuildProvidersPage();
        winrt::Microsoft::UI::Xaml::UIElement BuildShortcutsPage();
        winrt::Microsoft::UI::Xaml::UIElement BuildLatexPage();
        winrt::Microsoft::UI::Xaml::UIElement BuildThemesPage();
        winrt::Microsoft::UI::Xaml::UIElement BuildLicensesPage();
        winrt::Microsoft::UI::Xaml::UIElement BuildAboutPage();
        void SetNavigationPath(settings_ui::SettingsBreadcrumb::Path children = {}, std::function<void()> openCategory = {});
        void LoadSelectedLicense();
        void RefreshThemeList();
        void UpdateThemePreview();
        void ApplyLatexSetting();
        void ApplyLanguageSetting();
        void ApplyAppearanceSetting();
        void ApplyMediaSetting(MediaDisplaySettings const& media);
        void RefreshMediaControls();
        bool ApplyShortcutSettings();
        void ApplyPendingTheme();
        void UpdateThemeActions();
        void SetGeneralStatus(winrt::hstring const& message, bool error = false);
        void SetThemeStatus(winrt::hstring const& message, bool error = false);
        void SetShortcutStatus(winrt::hstring const& message, bool error = false);
        void SetLatexStatus(winrt::hstring const& message, bool error = false);
        void RefreshShortcutList();
        winrt::Microsoft::UI::Xaml::FrameworkElement BuildShortcutRow(std::size_t index);
        void BeginShortcutCapture(std::size_t index, winrt::Microsoft::UI::Xaml::Controls::Button const& button);
        void CaptureShortcut(
            std::size_t index,
            winrt::Microsoft::UI::Xaml::Controls::Button const& button,
            winrt::Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const& args);
        void ClearShortcut(std::size_t index);
        void ResetShortcut(std::size_t index);
        void ChangeShortcutScope(std::size_t index, std::int32_t selectedIndex);
        winrt::fire_and_forget ShowSnippetEditorAsync(std::optional<std::size_t> index);
        void RemoveSnippet(std::size_t index);
        void RefreshLatexCommandList();
        winrt::fire_and_forget ShowLatexCommandEditorAsync(std::optional<std::string> id);
        bool SaveLatexCommand(LatexCommandFormSubmission submission);
        void RemoveLatexCommand(std::string id);
        void ResetLatexUsage();
        bool HandleShortcutResult(
            folia::platform::editor::ShortcutSettingsResult const& result,
            std::optional<folia::platform::editor::EditorKeyGesture> gesture = std::nullopt);
        winrt::fire_and_forget ImportThemeAsync();
        void RemoveSelectedTheme();

        AppSettings settings_;
        AppSettings appliedSettings_;
        std::shared_ptr<ThemeCatalog> catalog_;
        std::shared_ptr<LatexCommandCatalog> latexCatalog_;
        folia::Theme systemVariant_ = folia::Theme::Dark;
        HWND owner_ = nullptr;
        ApplySettings applySettings_;
        ChangeTextLayout changeTextLayout_;
        winrt::Microsoft::UI::Xaml::Controls::ContentControl pageHost_{nullptr};
        settings_ui::SettingsBreadcrumb breadcrumb_;
        std::vector<std::pair<Page, winrt::Microsoft::UI::Xaml::Controls::Button>> categoryButtons_;
        std::vector<winrt::Microsoft::UI::Xaml::Controls::Button::Click_revoker> navigationClicks_;
        Page currentPage_ = Page::Home;
        winrt::Microsoft::UI::Xaml::UIElement homePage_{nullptr};
        winrt::Microsoft::UI::Xaml::UIElement generalPage_{ nullptr };
        winrt::Microsoft::UI::Xaml::UIElement textLayoutPage_{nullptr};
        std::unique_ptr<ui::TextLayoutControls> readingLayout_, chatLayout_;
        winrt::Microsoft::UI::Xaml::UIElement completionPage_{ nullptr };
        winrt::Microsoft::UI::Xaml::UIElement shortcutsPage_{ nullptr };
        winrt::Microsoft::UI::Xaml::UIElement latexPage_{ nullptr };
        winrt::Microsoft::UI::Xaml::UIElement themesPage_{ nullptr };
        winrt::Microsoft::UI::Xaml::UIElement licensesPage_{ nullptr };
        winrt::Microsoft::UI::Xaml::UIElement aboutPage_{ nullptr };
        winrt::Microsoft::UI::Xaml::Controls::ToggleSwitch latexSuggestionsToggle_;
        winrt::Microsoft::UI::Xaml::Controls::ToggleSwitch autoHideTools_;
        winrt::Microsoft::UI::Xaml::Controls::ToggleSwitch newWindowToggle_{nullptr};
        winrt::Microsoft::UI::Xaml::Controls::ComboBox materialCombo_{nullptr}, readingMaterialCombo_{nullptr};
        winrt::Microsoft::UI::Xaml::Controls::ComboBox languageCombo_;
        winrt::Microsoft::UI::Xaml::Controls::ComboBox imageWidthCombo_{nullptr};
        winrt::Microsoft::UI::Xaml::Controls::ComboBox mermaidThemeCombo_{nullptr};
        winrt::Microsoft::UI::Xaml::Controls::ComboBox themePicker_{nullptr};
        winrt::Microsoft::UI::Xaml::Controls::Image themePreview_;
        winrt::Microsoft::UI::Xaml::Controls::ComboBox licenseSelector_;
        winrt::Microsoft::UI::Xaml::Controls::RichEditBox licenseText_;
        winrt::Microsoft::UI::Xaml::Controls::TextBlock licenseStatus_;
        winrt::Microsoft::UI::Xaml::Controls::Button applyThemeButton_;
        winrt::Microsoft::UI::Xaml::Controls::Button removeThemeButton_;
        winrt::Microsoft::UI::Xaml::Controls::TextBlock generalStatus_;
        winrt::Microsoft::UI::Xaml::Controls::TextBlock themeStatus_;
        winrt::Microsoft::UI::Xaml::Controls::ListView shortcutList_;
        winrt::Microsoft::UI::Xaml::Controls::TextBlock shortcutStatus_;
        winrt::Microsoft::UI::Xaml::Controls::ListView latexCommandList_;
        winrt::Microsoft::UI::Xaml::Controls::TextBlock latexStatus_;
        winrt::Microsoft::UI::Xaml::Controls::ListViewBase::ContainerContentChanging_revoker shortcutRows_;
        winrt::Microsoft::UI::Xaml::Controls::ListViewBase::ContainerContentChanging_revoker latexRows_;
        std::vector<folia::platform::editor::EditorShortcutBinding> displayedShortcuts_;
        std::optional<std::uint64_t> displayedLatexRevision_;
        winrt::weak_ref<winrt::Microsoft::UI::Xaml::Controls::Button> captureButton_;
        folia::platform::editor::ShortcutSettingsModel shortcutModel_;
        std::vector<std::string> themeIds_;
        std::int32_t loadedLicenseIndex_ = -1;
        std::optional<std::size_t> capturingShortcut_;
        bool editorDialogOpen_ = false;
        bool refreshing_ = false;
        bool detached_ = false;
    };
}
