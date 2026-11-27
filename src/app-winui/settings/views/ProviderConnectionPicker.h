#pragma once

#include "settings/storage/ProviderPresets.h"

namespace winrt::Folia::settings_ui {
    class ProviderConnectionPicker : public std::enable_shared_from_this<ProviderConnectionPicker> {
    public:
        using Changed = std::function<void(models::ProviderPreset const&, models::ProviderRoute const&)>;
        void Build(Microsoft::UI::Xaml::Controls::StackPanel const&, std::string_view type,
                   folia::ai::ApiProtocol protocol, Changed);
        models::ProviderPreset const& Type() const { return *type_; }
        models::ProviderRoute const& Route() const { return *route_; }

    private:
        void SetType(models::ProviderPreset const&, folia::ai::ApiProtocol);
        Microsoft::UI::Xaml::Controls::ComboBox types_{nullptr}, protocols_{nullptr};
        models::ProviderPreset const* type_{};
        models::ProviderRoute const* route_{};
    };
}
