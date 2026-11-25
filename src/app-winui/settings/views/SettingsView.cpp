#include "pch.h"
#include "settings/views/SettingsView.h"
#include "settings/views/SettingsViewSupport.h"
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
        pageCrumb_.Style(settings_ui::ResourceStyle(L"SettingsCategoryTitleStyle"));
        pageCrumb_.VerticalAlignment(VerticalAlignment::Center);
        crumbs.Children().Append(pageCrumb_);
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
        StackPanel cards;
        cards.Style(settings_ui::ResourceStyle(L"SettingsCategoryListStyle"));
        for (auto const& category : categories) {
            Button card;
            card.Name(L"SettingsCategory" + hstring(category.title));
            card.Style(settings_ui::ResourceStyle(L"SettingsCategoryButtonStyle"));
            Grid row;
            row.Style(settings_ui::ResourceStyle(L"SettingsCategoryRowStyle"));
            ColumnDefinition iconColumn, labelColumn, arrowColumn;
            iconColumn.Width(GridLengthHelper::Auto());
            arrowColumn.Width(GridLengthHelper::Auto());
            row.ColumnDefinitions().Append(iconColumn);
            row.ColumnDefinitions().Append(labelColumn);
            row.ColumnDefinitions().Append(arrowColumn);

            FontIcon icon, arrow;
            icon.Glyph(category.icon);
            icon.Style(settings_ui::ResourceStyle(L"SettingsCategoryIconStyle"));
            arrow.Glyph(L"\xE76C");
            arrow.Style(settings_ui::ResourceStyle(L"SettingsNavigationArrowStyle"));
            auto label = Localize(category.title);
            auto description = Localize(category.description);
            StackPanel copy;
            copy.Spacing(3);
            auto title = settings_ui::Text(label);
            title.Style(settings_ui::ResourceStyle(L"SettingsCategoryTitleStyle"));
            copy.Children().Append(title);
            copy.Children().Append(settings_ui::Hint(description));
            Grid::SetColumn(copy, 1);
            Grid::SetColumn(arrow, 2);
            row.Children().Append(icon);
            row.Children().Append(copy);
            row.Children().Append(arrow);
            card.Content(row);
            Automation::AutomationProperties::SetName(card, label);
            Automation::AutomationProperties::SetHelpText(card, description);
            navigationClicks_.push_back(card.Click(winrt::auto_revoke, [this, page = category.page](auto const&, auto const&) {
                Navigate(page);
                Focus();
            }));
            categoryButtons_.emplace_back(category.page, card);
            cards.Children().Append(card);
        }
        panel.Children().Append(cards);
        return settings_ui::PageScroll(panel);
    }

    void SettingsView::Navigate(Page page)
    {
        if (pageHost_.Content() && page == currentPage_) return;
        if (page != Page::Shortcuts) CancelShortcutCapture();
        switch (page) {
        case Page::Home: pageHost_.Content(homePage_); break;
        case Page::General: pageHost_.Content(generalPage_); break;
        case Page::Themes: pageHost_.Content(themesPage_); break;
        case Page::Completion:
            completionPage_ = BuildCompletionPage();
            pageHost_.Content(completionPage_);
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
