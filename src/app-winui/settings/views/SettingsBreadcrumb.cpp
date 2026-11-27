#include "pch.h"
#include "settings/views/SettingsBreadcrumb.h"
#include "localization/Localization.h"
#include <winrt/Microsoft.UI.Xaml.Automation.h>

namespace winrt::Folia::settings_ui {
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    SettingsBreadcrumb::SettingsBreadcrumb() {
        root_ = Application::Current().Resources().Lookup(box_value(L"SettingsBreadcrumbTemplate"))
            .as<DataTemplate>().LoadContent().as<Grid>();
        back_ = root_.FindName(L"SettingsBackButton").as<Button>();
        auto bar = root_.FindName(L"SettingsBreadcrumbBar").as<BreadcrumbBar>();
        auto backLabel = Localize(L"SettingsBack");
        Automation::AutomationProperties::SetName(back_, backLabel);
        ToolTipService::SetToolTip(back_, box_value(backLabel));
        Automation::AutomationProperties::SetName(bar, Localize(L"Settings"));
        bar.ItemsSource(labels_);
        itemClick_ = bar.ItemClicked(auto_revoke, [this](auto const&, BreadcrumbBarItemClickedEventArgs const& args) {
            Open(args.Index());
        });
        backClick_ = back_.Click(auto_revoke, [this](auto const&, auto const&) {
            if (path_.size() >= 2) Open(path_.size() - 2);
        });
    }

    void SettingsBreadcrumb::SetPath(Path path) {
        auto common = std::ranges::mismatch(path_, path, {}, &Item::title, &Item::title);
        const auto firstChanged = static_cast<std::size_t>(common.in1 - path_.begin());
        path_ = std::move(path);
        while (labels_.Size() > firstChanged) labels_.RemoveAtEnd();
        for (auto i = firstChanged; i < path_.size(); ++i) labels_.Append(box_value(path_[i].title));
        root_.Visibility(path_.empty() ? Visibility::Collapsed : Visibility::Visible);
    }

    void SettingsBreadcrumb::Open(std::size_t index) const {
        if (index >= path_.size() || index + 1 == path_.size()) return;
        // Navigation replaces the path, including the callback being invoked.
        auto open = path_[index].open;
        if (open) open();
    }

    void SettingsBreadcrumb::Focus() const { back_.Focus(FocusState::Programmatic); }

    void SettingsBreadcrumb::Detach() {
        backClick_.revoke();
        itemClick_.revoke();
        path_.clear();
    }
}
