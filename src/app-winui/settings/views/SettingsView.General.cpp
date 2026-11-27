#include "pch.h"
#include "settings/views/SettingsView.h"
#include "settings/views/SettingsCards.h"
#include "localization/Localization.h"
#include "windows/media/mermaid/MermaidRenderer.h"

namespace winrt::Folia
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;
    using settings_ui::LanguageIds;
    using settings_ui::LanguageIndex;
    using settings_ui::PagePanel;

    UIElement SettingsView::BuildGeneralPage()
    {
        using namespace settings_ui;
        auto panel = PagePanel();
        panel.Style(settings_ui::ResourceStyle(L"SettingsOptionPageStyle"));
        panel.Children().Append(settings_ui::PageHeader(Localize(L"General"), Localize(L"GeneralDescription")));

        languageCombo_ = settings_ui::Choice();
        cards::ChoiceStyle(languageCombo_);
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
        panel.Children().Append(cards::Setting(
            {Localize(L"ApplicationLanguage"), Localize(L"LanguageRestartDescription"), cards::Icon(L"\xE8C1")},
            languageCombo_, cards::Placement::Wrap));

        newWindowToggle_ = cards::Switch(appliedSettings_.openFilesInNewWindow);
        newWindowToggle_.Toggled([this](auto const&, auto const&) {
            if (refreshing_ || detached_ || !applySettings_) return;
            auto proposed = appliedSettings_;
            proposed.openFilesInNewWindow = newWindowToggle_.IsOn();
            if (auto error = applySettings_(proposed)) {
                UpdateFileOpening(appliedSettings_.openFilesInNewWindow);
                SetGeneralStatus(*error, true);
            } else {
                UpdateFileOpening(proposed.openFilesInNewWindow);
                SetGeneralStatus({});
            }
        });
        panel.Children().Append(cards::Setting({Localize(L"OpenFilesInNewWindow"), Localize(L"OpenFilesInNewWindowDescription"),
            cards::Icon(WorkspaceIcon::Document)}, newWindowToggle_));

        auto appearance = cards::List();
        auto materialChoice = [&](wchar_t const* label, wchar_t const* glyph, bool allowInherit) {
            auto combo = settings_ui::Choice();
            cards::ChoiceStyle(combo);
            if (allowInherit) combo.Items().Append(box_value(Localize(L"MaterialFollowWindow")));
            for (auto const& choice : windowMaterials) combo.Items().Append(box_value(Localize(choice.label)));
            appearance.Children().Append(cards::Setting({Localize(label), {}, cards::Icon(glyph)}, combo, cards::Placement::Wrap));
            return combo;
        };
        materialCombo_ = materialChoice(L"WindowMaterial", L"\xE737", false);
        readingMaterialCombo_ = materialChoice(L"ReadingMaterial", L"\xE8A5", true);
        autoHideTools_ = cards::Switch();
        appearance.Children().Append(cards::Setting(
            {Localize(L"AutoHideDocumentTools"), {}, cards::Icon(L"\xE712")}, autoHideTools_));
        UpdateWorkspaceAppearance(settings_.appearance);
        autoHideTools_.Toggled([this](auto const&, auto const&) { ApplyAppearanceSetting(); });
        auto changed = [this](auto const&, auto const&) { ApplyAppearanceSetting(); };
        materialCombo_.SelectionChanged(changed);
        readingMaterialCombo_.SelectionChanged(changed);
        panel.Children().Append(cards::Section(Localize(L"WindowAppearance"), appearance));

        auto mediaSection = cards::List();
        imageWidthCombo_ = settings_ui::Choice();
        cards::ChoiceStyle(imageWidthCombo_);
        for (auto width : MediaDisplaySettings::imageWidths)
            imageWidthCombo_.Items().Append(box_value(hstring(std::to_wstring(width) + L"%")));
        mediaSection.Children().Append(cards::Setting(
            {Localize(L"ImageMaximumWidth"), Localize(L"ImageMaximumWidthDescription"), cards::Icon(WorkspaceIcon::Image)},
            imageWidthCombo_, cards::Placement::Wrap));

        mermaidThemeCombo_ = settings_ui::Choice();
        cards::ChoiceStyle(mermaidThemeCombo_);
        mermaidThemeCombo_.Items().Append(box_value(Localize(L"MermaidFollowFolia")));
        for (auto const& name : folia::platform::MermaidRenderer::ThemeNames())
            mermaidThemeCombo_.Items().Append(box_value(winrt::to_hstring(name)));
        mediaSection.Children().Append(cards::Setting(
            {Localize(L"MermaidTheme"), Localize(L"MermaidThemeDescription"), cards::Icon(L"\xE9D9")},
            mermaidThemeCombo_, cards::Placement::Wrap));
        panel.Children().Append(cards::Section(Localize(L"DocumentMedia"), mediaSection));

        RefreshMediaControls();
        imageWidthCombo_.SelectionChanged([this](auto const&, SelectionChangedEventArgs const&)
        {
            if (refreshing_) return;
            auto index = imageWidthCombo_.SelectedIndex();
            auto const& widths = MediaDisplaySettings::imageWidths;
            if (index < 0 || static_cast<std::size_t>(index) >= widths.size()) return;
            auto media = appliedSettings_.media;
            media.imageMaxWidthPercent = widths[static_cast<std::size_t>(index)];
            ApplyMediaSetting(media);
        });
        mermaidThemeCombo_.SelectionChanged([this](auto const&, SelectionChangedEventArgs const&)
        {
            if (refreshing_) return;
            auto index = mermaidThemeCombo_.SelectedIndex();
            auto names = folia::platform::MermaidRenderer::ThemeNames();
            if (index < 0 || static_cast<std::size_t>(index) > names.size()) return;
            auto media = appliedSettings_.media;
            media.mermaidTheme = index == 0 ? std::string{} : names[static_cast<std::size_t>(index - 1)];
            ApplyMediaSetting(media);
        });

        generalStatus_.TextWrapping(TextWrapping::Wrap);
        panel.Children().Append(generalStatus_);
        return settings_ui::PageScroll(panel);
    }

    void SettingsView::UpdateFileOpening(bool newWindow)
    {
        settings_.openFilesInNewWindow = appliedSettings_.openFilesInNewWindow = newWindow;
        auto wasRefreshing = std::exchange(refreshing_, true);
        if (newWindowToggle_) newWindowToggle_.IsOn(newWindow);
        refreshing_ = wasRefreshing;
    }

    void SettingsView::UpdateWorkspaceAppearance(WorkspaceAppearance const& appearance)
    {
        auto wasRefreshing = std::exchange(refreshing_, true);
        settings_.appearance = appliedSettings_.appearance = appearance;
        auto choice = std::ranges::find(windowMaterials, appearance.material, &WindowMaterialChoice::value);
        materialCombo_.SelectedIndex(static_cast<int>(choice - windowMaterials.begin()));
        auto reading = appearance.readingMaterial
            ? std::ranges::find(windowMaterials, *appearance.readingMaterial, &WindowMaterialChoice::value)
            : windowMaterials.end();
        readingMaterialCombo_.SelectedIndex(reading == windowMaterials.end() ? 0 : 1 + static_cast<int>(reading - windowMaterials.begin()));
        autoHideTools_.IsOn(appearance.autoHideTools);
        refreshing_ = wasRefreshing;
    }

    void SettingsView::ApplyAppearanceSetting()
    {
        if (detached_ || refreshing_ || !applySettings_) return;
        auto index = materialCombo_.SelectedIndex();
        auto reading = readingMaterialCombo_.SelectedIndex();
        if (index < 0 || static_cast<size_t>(index) >= windowMaterials.size()
            || reading < 0 || static_cast<size_t>(reading) > windowMaterials.size()) return;
        auto proposed = appliedSettings_;
        proposed.appearance = {autoHideTools_.IsOn(),
            windowMaterials[index].value, reading ? std::optional{windowMaterials[reading - 1].value} : std::nullopt};
        if (proposed.appearance == appliedSettings_.appearance) return;
        if (auto error = applySettings_(proposed)) {
            UpdateWorkspaceAppearance(appliedSettings_.appearance);
            SetGeneralStatus(*error, true);
            return;
        }
        UpdateWorkspaceAppearance(proposed.appearance);
        SetGeneralStatus({});
    }

    void SettingsView::RefreshMediaControls()
    {
        auto const& widths = MediaDisplaySettings::imageWidths;
        auto width = std::ranges::find(widths, settings_.media.imageMaxWidthPercent);
        imageWidthCombo_.SelectedIndex(static_cast<int>(width == widths.end() ? widths.size() - 1 : width - widths.begin()));
        auto names = folia::platform::MermaidRenderer::ThemeNames();
        auto theme = std::ranges::find(names, settings_.media.mermaidTheme);
        mermaidThemeCombo_.SelectedIndex(theme == names.end() ? 0 : 1 + static_cast<int>(theme - names.begin()));
    }

    void SettingsView::UpdateMediaSettings(MediaDisplaySettings const& media)
    {
        settings_.media = appliedSettings_.media = media;
        auto wasRefreshing = std::exchange(refreshing_, true);
        RefreshMediaControls();
        refreshing_ = wasRefreshing;
    }

    void SettingsView::ApplyMediaSetting(MediaDisplaySettings const& media)
    {
        if (detached_ || !applySettings_ || media == appliedSettings_.media) return;
        auto proposed = appliedSettings_;
        proposed.media = media;
        if (auto error = applySettings_(proposed))
        {
            SetGeneralStatus(*error, true);
            UpdateMediaSettings(appliedSettings_.media);
            return;
        }
        UpdateMediaSettings(media);
        SetGeneralStatus({});
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
