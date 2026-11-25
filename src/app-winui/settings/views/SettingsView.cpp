#include "pch.h"
#include "settings/views/SettingsView.h"
#include "settings/views/SettingsViewSupport.h"
#include "localization/Localization.h"

namespace winrt::Folia
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;
    using settings_ui::LanguageIndex;
    using settings_ui::NavigationItem;

    SettingsView::SettingsView(
        ListView const& navigation, ContentControl const& pageHost,
        AppSettings settings,
        std::shared_ptr<ThemeCatalog> catalog,
        std::shared_ptr<LatexCommandCatalog> latexCatalog,
        folia::Theme systemVariant,
        HWND owner,
        ApplySettings applySettings)
        : settings_(std::move(settings)),
          appliedSettings_(settings_),
          catalog_(std::move(catalog)),
          latexCatalog_(std::move(latexCatalog)),
          systemVariant_(systemVariant),
          owner_(owner),
          applySettings_(std::move(applySettings)), navigation_(navigation), pageHost_(pageHost)
    {
        pageHost_.HorizontalContentAlignment(HorizontalAlignment::Stretch);
        pageHost_.VerticalContentAlignment(VerticalAlignment::Stretch);
        shortcutModel_.Reset(settings_.shortcutBindings);
        Build();
    }

    void SettingsView::Build()
    {
        auto general = NavigationItem(Localize(L"General"), L"general", L"\xE713");
        auto shortcuts = NavigationItem(Localize(L"Shortcuts"), L"shortcuts", L"\xE765");
        auto latex = NavigationItem(Localize(L"LatexCommands"), L"latex", L"\xE943");
        auto themes = NavigationItem(Localize(L"Themes"), L"themes", L"\xE790");
        auto licenses = NavigationItem(Localize(L"Licenses"), L"licenses", L"\xE8A5");
        auto about = NavigationItem(Localize(L"About"), L"about", L"\xE946");
        navigation_.Items().Append(general);
        navigation_.Items().Append(shortcuts);
        navigation_.Items().Append(latex);
        navigation_.Items().Append(NavigationItem(Localize(L"AICompletion"), L"completion", L"\xE8D4"));
        navigation_.Items().Append(themes);
        navigation_.Items().Append(licenses);
        navigation_.Items().Append(about);

        generalPage_ = BuildGeneralPage();
        themesPage_ = BuildThemesPage();
        licensesPage_ = BuildLicensesPage();
        aboutPage_ = BuildAboutPage();
        navigationChanged_ = navigation_.SelectionChanged(winrt::auto_revoke, [this](auto const&, auto const&)
        {
            auto item = navigation_.SelectedItem().try_as<ListViewItem>();
            if (!item || !item.Tag()) return;
            Navigate(unbox_value<hstring>(item.Tag()));
        });
        navigation_.SelectedItem(general);
    }

    void SettingsView::Navigate(hstring const& page)
    {
        if (page != L"shortcuts") CancelShortcutCapture();
        if (page == L"themes") pageHost_.Content(themesPage_);
        else if (page == L"completion") { completionPage_ = BuildCompletionPage(); pageHost_.Content(completionPage_); }
        else if (page == L"latex")
        {
            if (!latexPage_) latexPage_ = BuildLatexPage();
            pageHost_.Content(latexPage_);
            RefreshLatexCommandList();
        }
        else if (page == L"shortcuts")
        {
            if (!shortcutsPage_) shortcutsPage_ = BuildShortcutsPage();
            pageHost_.Content(shortcutsPage_);
            RefreshShortcutList();
        }
        else if (page == L"licenses")
        {
            pageHost_.Content(licensesPage_);
            LoadSelectedLicense();
        }
        else if (page == L"about") pageHost_.Content(aboutPage_);
        else pageHost_.Content(generalPage_);
    }

    void SettingsView::Reset(AppSettings settings, folia::Theme systemVariant)
    {
        CancelShortcutCapture();
        settings_ = std::move(settings);
        appliedSettings_ = settings_;
        shortcutModel_.Reset(settings_.shortcutBindings);
        systemVariant_ = systemVariant;
        refreshing_ = true;
        latexSuggestionsToggle_.IsOn(settings_.latexSuggestionsEnabled);
        languageCombo_.SelectedIndex(LanguageIndex(settings_.languageId));
        UpdateWorkspaceAppearance(settings_.appearance);
        RefreshMediaControls();
        refreshing_ = false;
        if (shortcutsPage_) RefreshShortcutList();
        if (latexPage_) RefreshLatexCommandList();
        RefreshThemeList();
        SetGeneralStatus({});
        if (completionPage_ && pageHost_.Content() == completionPage_) {
            completionPage_ = BuildCompletionPage(); pageHost_.Content(completionPage_);
        }
    }

    void SettingsView::SetSystemVariant(folia::Theme variant)
    {
        if (systemVariant_ == variant) return;
        systemVariant_ = variant;
        if (settings_.themeId == "system") UpdateThemePreview();
    }

    void SettingsView::Detach()
    {
        navigationChanged_.revoke();
        detached_ = true;
        shortcutRows_.revoke();
        latexRows_.revoke();
        applySettings_ = {};
        owner_ = nullptr;
    }
}
