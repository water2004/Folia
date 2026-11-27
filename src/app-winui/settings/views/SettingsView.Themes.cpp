#include "pch.h"
#include "settings/views/SettingsView.h"
#include "settings/views/SettingsCards.h"
#include "localization/Localization.h"

namespace winrt::Folia
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;
    UIElement SettingsView::BuildThemesPage()
    {
        using namespace settings_ui;
        auto page = settings_ui::PagePanel();
        page.Children().Append(settings_ui::PageHeader(Localize(L"Themes"), Localize(L"ThemesDescription")));
        auto controls = settings_ui::SectionPanel();
        themePicker_ = settings_ui::Choice();
        cards::ChoiceStyle(themePicker_);
        themePicker_.SelectionChanged([this](auto const&, SelectionChangedEventArgs const&)
        {
            auto index = themePicker_.SelectedIndex();
            if (index < 0 || static_cast<std::size_t>(index) >= themeIds_.size()) return;
            if (refreshing_) return;
            settings_.themeId = themeIds_[static_cast<std::size_t>(index)];
            UpdateThemePreview();
        });
        controls.Children().Append(cards::Setting(
            {Localize(L"PreviewTheme"), {}, cards::Icon(L"\xE790")}, themePicker_, cards::Placement::Wrap));

        auto buttons = ButtonRow();
        buttons.HorizontalAlignment(HorizontalAlignment::Right);
        applyThemeButton_.Content(box_value(Localize(L"Apply")));
        applyThemeButton_.Click([this](auto const&, auto const&) { ApplyPendingTheme(); });
        applyThemeButton_.Style(settings_ui::ResourceStyle(L"SettingsPrimaryButtonStyle"));
        auto importButton = SecondaryAction(Localize(L"Import"));
        importButton.Click([this](auto const&, auto const&) { ImportThemeAsync(); });
        removeThemeButton_.Content(box_value(Localize(L"Remove")));
        removeThemeButton_.Style(settings_ui::ResourceStyle(L"SettingsSecondaryButtonStyle"));
        removeThemeButton_.Click([this](auto const&, auto const&) { RemoveSelectedTheme(); });
        buttons.Children().Append(applyThemeButton_);
        buttons.Children().Append(importButton);
        buttons.Children().Append(removeThemeButton_);
        controls.Children().Append(buttons);
        controls.Children().Append(themeStatus_);
        page.Children().Append(controls);

        Border preview;
        preview.Style(settings_ui::ResourceStyle(L"SettingsThemePreviewStyle"));
        themePreview_.Stretch(Media::Stretch::Uniform);
        themePreview_.ImageFailed([this](auto const&, auto const&) {
            SetThemeStatus(LocalizeFormat(L"IncompatibleThemePackage", {winrt::to_hstring(settings_.themeId)}), true);
        });
        preview.Child(themePreview_);
        page.Children().Append(preview);
        RefreshThemeList();
        return settings_ui::PageScroll(page);
    }

    void SettingsView::RefreshThemeList()
    {
        refreshing_ = true;
        struct ResetRefreshing { bool& value; ~ResetRefreshing() { value = false; } } reset{refreshing_};
        catalog_->Refresh();
        themeIds_.clear();
        themePicker_.Items().Clear();

        auto append = [&](hstring const& name, hstring const& detail, std::string id)
        {
            ComboBoxItem item;
            item.Content(box_value(name));
            Automation::AutomationProperties::SetHelpText(item, detail);
            themeIds_.push_back(std::move(id));
            themePicker_.Items().Append(item);
        };

        append(Localize(L"FollowWindowsTheme"), Localize(L"AutomaticThemeDescription"), "system");
        for (auto const& entry : catalog_->Entries())
        {
            append(
                winrt::to_hstring(entry.profile.name),
                entry.builtIn ? Localize(L"BuiltIn") : Localize(L"Custom"),
                entry.profile.id);
        }

        auto found = std::ranges::find(themeIds_, settings_.themeId);
        if (found == themeIds_.end())
        {
            themePicker_.SelectedIndex(-1);
        }
        else
        {
            themePicker_.SelectedIndex(
                static_cast<std::int32_t>(std::distance(themeIds_.begin(), found)));
        }
        UpdateThemePreview();
    }

    void SettingsView::UpdateThemePreview()
    {
        auto path = catalog_->Preview(settings_.themeId, systemVariant_);
        themePreview_.Source(path.empty() ? nullptr : Media::Imaging::BitmapImage(settings_ui::FileUri(path)));
        UpdateThemeActions();
        if (path.empty()) SetThemeStatus(LocalizeFormat(L"IncompatibleThemePackage", {winrt::to_hstring(settings_.themeId)}), true);
        else if (!catalog_->Diagnostics().empty())
            SetThemeStatus(catalog_->Diagnostics().front(), true);
        else
            SetThemeStatus({});
    }

    void SettingsView::SetThemeStatus(hstring const& message, bool error)
    {
        settings_ui::SetStatus(themeStatus_, message, error);
    }

    fire_and_forget SettingsView::ImportThemeAsync()
    {
        auto lifetime = shared_from_this();
        if (detached_) co_return;
        try
        {
            Windows::Storage::Pickers::FileOpenPicker picker;
            picker.SuggestedStartLocation(
                Windows::Storage::Pickers::PickerLocationId::DocumentsLibrary);
            picker.FileTypeFilter().Append(L".json");
            auto initialize = picker.as<::IInitializeWithWindow>();
            check_hresult(initialize->Initialize(owner_));
            auto file = co_await picker.PickSingleFileAsync();
            if (detached_ || !file) co_return;
            std::string id;
            if (auto error = catalog_->Import(std::filesystem::path(file.Path().c_str()), id))
            {
                SetThemeStatus(*error, true);
                co_return;
            }
            settings_.themeId = std::move(id);
            RefreshThemeList();
            SetThemeStatus(Localize(L"ThemeImported"));
        }
        catch (hresult_error const& error)
        {
            if (detached_) co_return;
            SetThemeStatus(LocalizeFormat(L"ThemeImportFailed", {error.message()}), true);
        }
        catch (std::exception const& error)
        {
            if (detached_) co_return;
            SetThemeStatus(LocalizeFormat(
                L"ThemeImportFailed", {winrt::to_hstring(error.what())}), true);
        }
    }

    void SettingsView::RemoveSelectedTheme()
    {
        auto index = themePicker_.SelectedIndex();
        if (index <= 0 || static_cast<std::size_t>(index) >= themeIds_.size()) return;
        auto id = themeIds_[static_cast<std::size_t>(index)];
        if (id == appliedSettings_.themeId)
        {
            SetThemeStatus(Localize(L"ApplyThemeBeforeRemove"), true);
            return;
        }
        if (auto error = catalog_->Remove(id))
        {
            SetThemeStatus(*error, true);
            return;
        }
        if (settings_.themeId == id) settings_.themeId = appliedSettings_.themeId;
        RefreshThemeList();
        SetThemeStatus(Localize(L"CustomThemeRemoved"));
    }

    void SettingsView::ApplyPendingTheme()
    {
        if (detached_ || !applySettings_ || settings_.themeId == appliedSettings_.themeId)
            return;
        auto proposed = appliedSettings_;
        proposed.themeId = settings_.themeId;
        if (auto error = applySettings_(proposed))
        {
            SetThemeStatus(*error, true);
            return;
        }
        appliedSettings_.themeId = proposed.themeId;
        SetThemeStatus(Localize(L"ThemeApplied"));
        UpdateThemeActions();
    }

    void SettingsView::UpdateThemeActions()
    {
        applyThemeButton_.IsEnabled(themePicker_.SelectedIndex() >= 0 && settings_.themeId != appliedSettings_.themeId);
        auto entry = catalog_->Find(settings_.themeId);
        removeThemeButton_.IsEnabled(entry && !entry->builtIn && settings_.themeId != appliedSettings_.themeId);
    }
}
