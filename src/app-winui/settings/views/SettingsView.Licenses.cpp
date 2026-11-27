#include "pch.h"
#include "settings/views/SettingsView.h"
#include "settings/views/SettingsCards.h"
#include "localization/Localization.h"
#include "storage/AssetPaths.h"
#include <winrt/Microsoft.UI.Text.h>
#include <winrt/Microsoft.UI.Xaml.Automation.h>
#include <wil/resource.h>

namespace winrt::Folia
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;
    using settings_ui::FileUri;
    using settings_ui::PageHeading;
    using settings_ui::PagePanel;
    using settings_ui::Text;

    UIElement SettingsView::BuildLicensesPage()
    {
        using namespace settings_ui;
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
        cards::ChoiceStyle(licenseSelector_);
        licenseSelector_.Items().Append(box_value(Localize(L"FoliaLicense")));
        licenseSelector_.Items().Append(box_value(Localize(L"ThirdPartyNotices")));
        licenseSelector_.SelectedIndex(0);
        licenseSelector_.SelectionChanged([this](auto const&, SelectionChangedEventArgs const&)
        {
            LoadSelectedLicense();
        });
        auto selector = cards::Setting(
            {Localize(L"LicenseDocument"), {}, cards::Icon(WorkspaceIcon::Document)}, licenseSelector_, cards::Placement::Wrap);
        Grid::SetRow(selector, 1);
        page.Children().Append(selector);

        // Present the notice as a document in both XAML and UI Automation.
        licenseText_.Document().UndoLimit(0);
        licenseText_.Style(settings_ui::ResourceStyle(L"SettingsLicenseDocumentStyle"));
        Automation::AutomationProperties::SetName(licenseText_, Localize(L"LicenseDocument"));
        Grid::SetRow(licenseText_, 2);
        page.Children().Append(licenseText_);
        licenseStatus_.TextWrapping(TextWrapping::Wrap);
        Grid::SetRow(licenseStatus_, 3);
        page.Children().Append(licenseStatus_);
        return settings_ui::PageViewport(page);
    }

    UIElement SettingsView::BuildAboutPage()
    {
        using namespace settings_ui;
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
        copy.Children().Append(Text(LocalizeFormat(L"Version", {L"0.8.1"})));
        copy.Children().Append(settings_ui::Hint(Localize(L"AppDescription")));
        Grid::SetColumn(copy, 1);
        identity.Children().Append(copy);
        panel.Children().Append(identity);

        HyperlinkButton repository;
        repository.Content(box_value(L"water2004/Folia  ↗"));
        repository.NavigateUri(
            winrt::Windows::Foundation::Uri(L"https://github.com/water2004/Folia"));
        panel.Children().Append(cards::Setting(
            {Localize(L"OpenSourceProject"), Localize(L"OpenSourceDescription"), cards::Icon(L"\xE774")},
            repository, cards::Placement::Wrap));

        return settings_ui::PageScroll(panel);
    }

    void SettingsView::LoadSelectedLicense()
    {
        using Microsoft::UI::Text::TextSetOptions;
        auto const index = licenseSelector_.SelectedIndex();
        if (index < 0 || index > 1 || index == loadedLicenseIndex_) return;
        auto showError = [&](hstring const& detail) {
            licenseText_.Visibility(Visibility::Collapsed);
            loadedLicenseIndex_ = -1;
            licenseStatus_.Text(LocalizeFormat(L"UnableLoadLicense", {detail}));
        };
        try
        {
            auto const file = index == 0 ? L"LICENSE.txt" : L"THIRD-PARTY-NOTICES.txt";
            std::ifstream stream(AssetPath(std::filesystem::path(L"licenses") / file), std::ios::binary);
            if (!stream) throw std::runtime_error("cannot open text asset");
            const std::string source{std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
            std::string_view utf8 = source;
            if (utf8.starts_with("\xEF\xBB\xBF")) utf8.remove_prefix(3);
            const auto text = winrt::to_hstring(utf8);
            // The native text object rejects programmatic writes in read-only mode.
            licenseText_.IsReadOnly(false);
            auto restoreReadOnly = wil::scope_exit([&] { licenseText_.IsReadOnly(true); });
            auto document = licenseText_.Document();
            document.SetText(TextSetOptions::None, text);
            document.Selection().SetRange(0, 0);
            licenseText_.Visibility(Visibility::Visible);
            licenseStatus_.Text({});
            loadedLicenseIndex_ = index;
        }
        catch (hresult_error const& error)
        {
            showError(error.message());
        }
        catch (std::exception const& error)
        {
            showError(winrt::to_hstring(error.what()));
        }
    }
}
