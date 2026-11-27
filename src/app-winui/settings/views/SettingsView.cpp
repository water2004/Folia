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

        StackPanel backLabel;
        backLabel.Orientation(Orientation::Horizontal);
        backLabel.Spacing(8);
        FontIcon backIcon;
        backIcon.Glyph(L"\xE72B");
        backIcon.Style(settings_ui::ResourceStyle(L"SettingsNavigationArrowStyle"));
        backLabel.Children().Append(backIcon);
        backLabel.Children().Append(settings_ui::Text(Localize(L"Settings")));
        homeButton_.Name(L"SettingsHomeButton");
        homeButton_.Content(backLabel);
        homeButton_.Style(settings_ui::ResourceStyle(L"SettingsHomeButtonStyle"));
        Automation::AutomationProperties::SetName(homeButton_, Localize(L"Settings"));
        navigationClicks_.push_back(homeButton_.Click(winrt::auto_revoke, [this](auto const&, auto const&) {
            const auto previous = currentPage_;
            Navigate(Page::Home);
            auto category = std::ranges::find(categoryButtons_, previous, &std::pair<Page, Button>::first);
            if (category != categoryButtons_.end()) category->second.Focus(FocusState::Programmatic);
        }));
        StackPanel crumbs;
        crumbs.Orientation(Orientation::Horizontal);
        crumbs.Style(settings_ui::ResourceStyle(L"SettingsBreadcrumbStyle"));
        crumbs.Children().Append(homeButton_);
        FontIcon separator;
        separator.Glyph(L"\xE76C");
        separator.Style(settings_ui::ResourceStyle(L"SettingsNavigationArrowStyle"));
        crumbs.Children().Append(separator);
        pageCrumb_.Style(settings_ui::ResourceStyle(L"SettingsCardTitleStyle"));
        pageCrumb_.VerticalAlignment(VerticalAlignment::Center);
        crumbs.Children().Append(pageCrumb_);
        subpageCrumbs_.Orientation(Orientation::Horizontal);
        subpageCrumbs_.Style(settings_ui::ResourceStyle(L"SettingsSubpageBreadcrumbStyle"));
        categoryCrumb_.Style(settings_ui::ResourceStyle(L"SettingsHomeButtonStyle"));
        navigationClicks_.push_back(categoryCrumb_.Click(winrt::auto_revoke, [this](auto const&, auto const&) {
            auto back = subpageBack_;
            if (back) back();
        }));
        subpageCrumbs_.Children().Append(categoryCrumb_);
        FontIcon next;
        next.Glyph(L"\xE76C");
        next.Style(settings_ui::ResourceStyle(L"SettingsNavigationArrowStyle"));
        subpageCrumbs_.Children().Append(next);
        subpageTitle_.Style(settings_ui::ResourceStyle(L"SettingsSubpageTitleStyle"));
        subpageCrumbs_.Children().Append(subpageTitle_);
        childCrumbs_.Orientation(Orientation::Horizontal);
        childCrumbs_.Style(settings_ui::ResourceStyle(L"SettingsSubpageBreadcrumbStyle"));
        parentCrumb_.Style(settings_ui::ResourceStyle(L"SettingsHomeButtonStyle"));
        navigationClicks_.push_back(parentCrumb_.Click(winrt::auto_revoke, [this](auto const&, auto const&) {
            auto back = parentBack_;
            if (back) back();
        }));
        childCrumbs_.Children().Append(parentCrumb_);
        FontIcon childSeparator;
        childSeparator.Glyph(L"\xE76C");
        childSeparator.Style(settings_ui::ResourceStyle(L"SettingsNavigationArrowStyle"));
        childCrumbs_.Children().Append(childSeparator);
        childTitle_.Style(settings_ui::ResourceStyle(L"SettingsSubpageTitleStyle"));
        childCrumbs_.Children().Append(childTitle_);
        subpageCrumbs_.Children().Append(childCrumbs_);
        crumbs.Children().Append(subpageCrumbs_);
        SetSubpage();
        pageNavigation_ = settings_ui::PageViewport(crumbs);
        root.Children().InsertAt(0, pageNavigation_);

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
        SetSubpage();
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
        currentPage_ = page;
        pageNavigation_.Visibility(page == Page::Home ? Visibility::Collapsed : Visibility::Visible);
        if (auto category = std::ranges::find(categories, page, &Category::page); category != std::end(categories))
            pageCrumb_.Text(Localize(category->title));
    }

    void SettingsView::SetSubpage(hstring const& title, std::function<void()> back,
        hstring const& child, std::function<void()> parent)
    {
        subpageBack_ = std::move(back);
        auto expanded = static_cast<bool>(subpageBack_);
        pageCrumb_.Visibility(expanded ? Visibility::Collapsed : Visibility::Visible);
        subpageCrumbs_.Visibility(expanded ? Visibility::Visible : Visibility::Collapsed);
        parentBack_ = std::move(parent);
        auto nested = static_cast<bool>(parentBack_);
        subpageTitle_.Visibility(nested ? Visibility::Collapsed : Visibility::Visible);
        subpageTitle_.Text(title);
        childCrumbs_.Visibility(nested ? Visibility::Visible : Visibility::Collapsed);
        childTitle_.Text(child);
        if (nested) {
            auto label = settings_ui::Text(title);
            label.Style(settings_ui::ResourceStyle(L"SettingsSubpageTitleStyle"));
            parentCrumb_.Content(label);
            Automation::AutomationProperties::SetName(parentCrumb_, title);
        }
        if (expanded) categoryCrumb_.Content(box_value(pageCrumb_.Text()));
    }

    void SettingsView::Focus()
    {
        auto button = currentPage_ == Page::Home ? categoryButtons_.front().second : homeButton_;
        button.Focus(FocusState::Programmatic);
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
        navigationClicks_.clear();
        detached_ = true;
        shortcutRows_.revoke();
        latexRows_.revoke();
        applySettings_ = {};
        owner_ = nullptr;
    }
}
