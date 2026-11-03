#include "pch.h"
#include "settings/SettingsView.h"
#include "settings/SettingsViewSupport.h"
#include "localization/Localization.h"

namespace winrt::Folia
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;
    using settings_ui::Brush;
    using settings_ui::LanguageIds;
    using settings_ui::LanguageIndex;
    using settings_ui::PageHeading;
    using settings_ui::PagePanel;
    using settings_ui::Text;

    UIElement SettingsView::BuildGeneralPage()
    {
        auto panel = PagePanel();
        panel.Children().Append(settings_ui::PageHeader(Localize(L"General"), Localize(L"GeneralDescription")));

        auto languageSection = settings_ui::SectionPanel();
        auto languageHeading = settings_ui::SectionHeading(Localize(L"Language"));
        languageSection.Children().Append(languageHeading);
        languageCombo_.Header(box_value(Localize(L"ApplicationLanguage")));
        languageCombo_.Style(settings_ui::ResourceStyle(L"SettingsComboBoxStyle"));
        languageCombo_.Items().Append(box_value(Localize(L"FollowWindowsLanguage")));
        languageCombo_.Items().Append(box_value(Localize(L"EnglishLanguage")));
        languageCombo_.Items().Append(box_value(Localize(L"SimplifiedChineseLanguage")));
        languageCombo_.SelectedIndex(LanguageIndex(settings_.languageId));
        languageCombo_.SelectionChanged([this](auto const&, SelectionChangedEventArgs const&)
        {
            if (refreshing_) return;
            auto index = languageCombo_.SelectedIndex();
            if (index < 0 || static_cast<std::size_t>(index) >= LanguageIds.size()) return;
            settings_.languageId = LanguageIds[static_cast<std::size_t>(index)];
            ApplyLanguageSetting();
        });
        languageSection.Children().Append(languageCombo_);
        languageSection.Children().Append(settings_ui::Hint(Localize(L"LanguageRestartDescription")));
        panel.Children().Append(languageSection);

        generalStatus_.TextWrapping(TextWrapping::Wrap);
        panel.Children().Append(generalStatus_);
        return settings_ui::PageScroll(panel);
    }

    void SettingsView::SetGeneralStatus(hstring const& message, bool error)
    {
        settings_ui::SetStatus(generalStatus_, message, error);
    }

    void SettingsView::ApplyLanguageSetting()
    {
        if (detached_ || !applySettings_ || settings_.languageId == appliedSettings_.languageId)
            return;
        auto proposed = appliedSettings_;
        proposed.languageId = settings_.languageId;
        if (auto error = applySettings_(proposed))
        {
            SetGeneralStatus(*error, true);
            refreshing_ = true;
            settings_.languageId = appliedSettings_.languageId;
            languageCombo_.SelectedIndex(LanguageIndex(settings_.languageId));
            refreshing_ = false;
            return;
        }
        appliedSettings_.languageId = proposed.languageId;
        SetGeneralStatus(Localize(L"LanguageSavedRestart"));
    }
}
