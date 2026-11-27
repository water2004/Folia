#include "pch.h"

import std;

#include "settings/views/SettingsView.h"
#include "settings/views/SettingsViewSupport.h"

namespace winrt::Folia
{
    Microsoft::UI::Xaml::UIElement SettingsView::BuildTextLayoutPage()
    {
        auto page = settings_ui::PagePanel();
        page.Style(settings_ui::ResourceStyle(L"SettingsOptionPageStyle"));
        auto build = [this](wchar_t const* title, TextLayoutTarget target, TextLayoutOptions const& options) {
            return std::make_unique<ui::TextLayoutControls>(
                Localize(title), options, [this, target](TextLayoutOptions const& value) {
                    if (changeTextLayout_) changeTextLayout_(target, value);
                }, false);
        };
        readingLayout_ = build(L"TextLayoutReading", TextLayoutTarget::Reading, settings_.textLayouts.reading);
        chatLayout_ = build(L"TextLayoutChat", TextLayoutTarget::Chat, settings_.textLayouts.chat);
        readingLayout_->Element().Name(L"ReadingTextLayout");
        chatLayout_->Element().Name(L"ChatTextLayout");
        page.Children().Append(readingLayout_->Element());
        page.Children().Append(chatLayout_->Element());
        return settings_ui::PageScroll(page);
    }

    void SettingsView::UpdateTextLayouts(TextLayoutProfiles const& value)
    {
        settings_.textLayouts = appliedSettings_.textLayouts = value;
        if (readingLayout_) readingLayout_->SetOptions(value.reading);
        if (chatLayout_) chatLayout_->SetOptions(value.chat);
    }
}
