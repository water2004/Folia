#include "pch.h"
#include "settings/SettingsView.h"
#include "settings/SettingsViewSupport.h"
#include "localization/Localization.h"
#include "storage/AssetPaths.h"

namespace winrt::Folia
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;
    using settings_ui::FileUri;
    using settings_ui::PageHeading;
    using settings_ui::PagePanel;
    using settings_ui::ReadUtf8Lines;
    using settings_ui::Text;

    UIElement SettingsView::BuildLicensesPage()
    {
        Grid page;
        page.Style(settings_ui::ResourceStyle(L"SettingsGridPageStyle"));

        for (auto index = 0; index < 4; ++index)
        {
            RowDefinition row;
            row.Height(index == 2
                ? GridLengthHelper::FromValueAndType(1, GridUnitType::Star)
                : GridLengthHelper::Auto());
            page.RowDefinitions().Append(row);
        }

        auto header = settings_ui::PageHeader(Localize(L"Licenses"), Localize(L"LicensesDescription"));
        page.Children().Append(header);

        licenseSelector_ = settings_ui::Choice();
        licenseSelector_.Header(box_value(Localize(L"LicenseDocument")));
        licenseSelector_.Style(settings_ui::ResourceStyle(L"SettingsComboBoxStyle"));
        licenseSelector_.Items().Append(box_value(Localize(L"FoliaLicense")));
        licenseSelector_.Items().Append(box_value(Localize(L"ThirdPartyNotices")));
        licenseSelector_.SelectedIndex(0);
        licenseSelector_.SelectionChanged([this](auto const&, SelectionChangedEventArgs const&)
        {
            LoadSelectedLicense();
        });
        Grid::SetRow(licenseSelector_, 1);
        page.Children().Append(licenseSelector_);

        licenseList_.ItemTemplate(Application::Current().Resources().Lookup(box_value(L"SettingsLicenseLineTemplate")).as<DataTemplate>());
        licenseList_.ItemContainerStyle(settings_ui::ResourceStyle(L"SettingsLicenseLineStyle"));
        licenseList_.SelectionMode(ListViewSelectionMode::None);
        licenseList_.IsItemClickEnabled(false);
        licenseList_.CanDragItems(false);
        licenseList_.CanReorderItems(false);
        licenseList_.HorizontalAlignment(HorizontalAlignment::Stretch);
        licenseList_.VerticalAlignment(VerticalAlignment::Stretch);
        ScrollViewer::SetHorizontalScrollMode(licenseList_, ScrollMode::Disabled);
        ScrollViewer::SetHorizontalScrollBarVisibility(
            licenseList_, ScrollBarVisibility::Disabled);
        ScrollViewer::SetVerticalScrollMode(licenseList_, ScrollMode::Enabled);
        ScrollViewer::SetVerticalScrollBarVisibility(licenseList_, ScrollBarVisibility::Auto);

        Grid::SetRow(licenseList_, 2);
        page.Children().Append(licenseList_);
        licenseStatus_.TextWrapping(TextWrapping::Wrap);
        Grid::SetRow(licenseStatus_, 3);
        page.Children().Append(licenseStatus_);
        return page;
    }

    UIElement SettingsView::BuildAboutPage()
    {
        auto panel = PagePanel();
        panel.Children().Append(PageHeading(Localize(L"AboutApp")));

        Grid identity;
        identity.ColumnSpacing(16);
        ColumnDefinition iconColumn;
        iconColumn.Width(GridLengthHelper::FromPixels(88));
        ColumnDefinition copyColumn;
        copyColumn.Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
        identity.ColumnDefinitions().Append(iconColumn);
        identity.ColumnDefinitions().Append(copyColumn);

        Image logo;
        logo.Width(88);
        logo.Height(88);
        logo.Stretch(Media::Stretch::Uniform);
        logo.Source(Media::Imaging::BitmapImage(
            FileUri(AssetPath(std::filesystem::path(L"branding") / L"Folia.png"))));
        identity.Children().Append(logo);

        StackPanel copy;
        copy.Spacing(8);
        auto name = Text(Localize(L"AppName"));
        name.Style(settings_ui::ResourceStyle(L"SettingsAppNameStyle"));
        copy.Children().Append(name);
        copy.Children().Append(Text(LocalizeFormat(L"Version", {L"0.5.5"})));
        copy.Children().Append(settings_ui::Hint(Localize(L"AppDescription")));
        Grid::SetColumn(copy, 1);
        identity.Children().Append(copy);
        panel.Children().Append(identity);

        Grid openSource;
        openSource.ColumnSpacing(16);
        ColumnDefinition openSourceCopyColumn;
        openSourceCopyColumn.Width(GridLengthHelper::FromValueAndType(
            1, GridUnitType::Star));
        ColumnDefinition openSourceLinkColumn;
        openSourceLinkColumn.Width(GridLengthHelper::Auto());
        openSource.ColumnDefinitions().Append(openSourceCopyColumn);
        openSource.ColumnDefinitions().Append(openSourceLinkColumn);

        StackPanel openSourceCopy;
        openSourceCopy.Spacing(5);
        auto openSourceHeading = settings_ui::SectionHeading(Localize(L"OpenSourceProject"));
        openSourceCopy.Children().Append(openSourceHeading);
        openSourceCopy.Children().Append(settings_ui::Hint(Localize(L"OpenSourceDescription")));
        openSource.Children().Append(openSourceCopy);

        HyperlinkButton repository;
        repository.Content(box_value(L"water2004/Folia  ↗"));
        repository.NavigateUri(
            winrt::Windows::Foundation::Uri(L"https://github.com/water2004/Folia"));
        repository.VerticalAlignment(VerticalAlignment::Center);
        Grid::SetColumn(repository, 1);
        openSource.Children().Append(repository);
        panel.Children().Append(openSource);

        StackPanel technology;
        technology.Spacing(8);
        auto heading = settings_ui::SectionHeading(Localize(L"BuiltForWindows"));
        technology.Children().Append(heading);
        technology.Children().Append(settings_ui::Hint(Localize(L"TechnologyDescription")));
        panel.Children().Append(technology);

        return settings_ui::PageScroll(panel);
    }

    void SettingsView::LoadSelectedLicense()
    {
        auto const index = licenseSelector_.SelectedIndex();
        if (index < 0 || index > 1 || index == loadedLicenseIndex_) return;
        try
        {
            auto const file = index == 0 ? L"LICENSE.txt" : L"THIRD-PARTY-NOTICES.txt";
            auto const path = AssetPath(std::filesystem::path(L"licenses") / file);
            licenseLines_ = ReadUtf8Lines(path);
            licenseList_.ItemsSource(licenseLines_);
            licenseStatus_.Text({});
            loadedLicenseIndex_ = index;
        }
        catch (std::exception const& error)
        {
            licenseLines_ = nullptr;
            licenseList_.ItemsSource(nullptr);
            licenseStatus_.Text(LocalizeFormat(
                L"UnableLoadLicense", {winrt::to_hstring(error.what())}));
        }
    }
}
