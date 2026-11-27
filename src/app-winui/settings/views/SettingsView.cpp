#include "pch.h"
#include "settings/views/SettingsView.h"
#include "settings/views/SettingsCards.h"
#include "localization/Localization.h"

namespace winrt::Folia
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;
    using settings_ui::LanguageIndex;

    namespace {
        using Page = SettingsView::Page;
        struct Category {
            Page page;
            wchar_t const* title;
            wchar_t const* description;
            wchar_t const* icon;
        };
        constexpr Category categories[] = {
            {Page::General, L"General", L"GeneralDescription", L"\xE713"},
            {Page::Themes, L"Themes", L"ThemesDescription", L"\xE790"},
            {Page::Shortcuts, L"Shortcuts", L"ShortcutsDescription", L"\xE765"},
            {Page::Latex, L"LatexCommands", L"LatexCommandsDescription", L"\xE943"},
            {Page::Completion, L"AICompletion", L"AICompletionDescription", L"\xE8D4"},
            {Page::Providers, L"ChatProviders", L"ChatProvidersDescription", L"\xE774"},
            {Page::Licenses, L"Licenses", L"LicensesDescription", L"\xE8A5"},
            {Page::About, L"About", L"AppDescription", L"\xE946"},
        };
    }

    SettingsView::SettingsView(
        ContentControl const& host,
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
          applySettings_(std::move(applySettings))
    {
        auto root = Application::Current().Resources().Lookup(box_value(L"SettingsRootTemplate"))
            .as<DataTemplate>().LoadContent().as<Grid>();

        root.Children().InsertAt(0, breadcrumb_.Element());

        pageHost_ = root.FindName(L"SettingsPageContent").as<ContentControl>();
        host.HorizontalContentAlignment(HorizontalAlignment::Stretch);
        host.VerticalContentAlignment(VerticalAlignment::Stretch);
        host.Content(root);
        shortcutModel_.Reset(settings_.shortcutBindings);
        generalPage_ = BuildGeneralPage();
        themesPage_ = BuildThemesPage();
        homePage_ = BuildHomePage();
        Navigate(Page::Home);
    }

    UIElement SettingsView::BuildHomePage()
    {
        auto panel = settings_ui::PagePanel();
        panel.Children().Append(settings_ui::PageHeading(Localize(L"Settings")));
        using namespace settings_ui;
        auto categoriesList = cards::List();
        for (auto const& category : categories) {
            auto card = cards::Link({Localize(category.title), Localize(category.description), cards::Icon(category.icon)});
            card.Name(L"SettingsCategory" + hstring(category.title));
            navigationClicks_.push_back(card.Click(winrt::auto_revoke, [this, page = category.page](auto const&, auto const&) {
                Navigate(page);
                Focus();
            }));
            categoryButtons_.emplace_back(category.page, card);
            categoriesList.Children().Append(card);
        }
        panel.Children().Append(categoriesList);
        return settings_ui::PageScroll(panel);
    }

    void SettingsView::Navigate(Page page)
    {
        if (pageHost_.Content() && page == currentPage_) return;
        currentPage_ = page;
        SetNavigationPath();
        if (page != Page::Shortcuts) CancelShortcutCapture();
        switch (page) {
        case Page::Home: pageHost_.Content(homePage_); break;
        case Page::General: pageHost_.Content(generalPage_); break;
        case Page::Themes: pageHost_.Content(themesPage_); break;
        case Page::Completion:
            completionPage_ = BuildCompletionPage();
            pageHost_.Content(completionPage_);
            break;
        case Page::Providers:
            pageHost_.Content(BuildProvidersPage());
            break;
        case Page::Latex:
            if (!latexPage_) latexPage_ = BuildLatexPage();
            pageHost_.Content(latexPage_);
            RefreshLatexCommandList();
            break;
        case Page::Shortcuts:
            if (!shortcutsPage_) shortcutsPage_ = BuildShortcutsPage();
            pageHost_.Content(shortcutsPage_);
            RefreshShortcutList();
            break;
        case Page::Licenses:
            if (!licensesPage_) licensesPage_ = BuildLicensesPage();
            pageHost_.Content(licensesPage_);
            LoadSelectedLicense();
            break;
        case Page::About:
            if (!aboutPage_) aboutPage_ = BuildAboutPage();
            pageHost_.Content(aboutPage_);
            break;
        }
    }

    void SettingsView::SetNavigationPath(settings_ui::SettingsBreadcrumb::Path children, std::function<void()> openCategory)
    {
        auto category = std::ranges::find(categories, currentPage_, &Category::page);
        if (category == std::end(categories)) {
            breadcrumb_.SetPath({});
            return;
        }
        auto home = [this] {
            auto previous = currentPage_;
            Navigate(Page::Home);
            auto category = std::ranges::find(categoryButtons_, previous, &std::pair<Page, Button>::first);
            if (category != categoryButtons_.end()) category->second.Focus(FocusState::Programmatic);
        };
        settings_ui::SettingsBreadcrumb::Path path{
            {Localize(L"Settings"), std::move(home)}, {Localize(category->title), std::move(openCategory)}
        };
        path.append_range(std::move(children) | std::views::as_rvalue);
        breadcrumb_.SetPath(std::move(path));
    }

    void SettingsView::Focus()
    {
        if (currentPage_ == Page::Home) categoryButtons_.front().second.Focus(FocusState::Programmatic);
        else breadcrumb_.Focus();
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
        breadcrumb_.Detach();
        navigationClicks_.clear();
        detached_ = true;
        shortcutRows_.revoke();
        latexRows_.revoke();
        applySettings_ = {};
        owner_ = nullptr;
    }
}
