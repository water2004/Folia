#include "pch.h"
#include "settings/views/ProviderConnectionPicker.h"
#include "agent/ChatControls.h"
#include "settings/views/SettingsCards.h"

namespace winrt::Folia::settings_ui {
    using namespace chat;
    using namespace Microsoft::UI::Xaml::Controls;
    using folia::ai::ApiProtocol;

    namespace {
        hstring ProtocolLabel(ApiProtocol protocol) {
            switch (protocol) {
            case ApiProtocol::Responses: return L"OpenAI Responses";
            case ApiProtocol::ChatCompletions: return L"OpenAI Chat Completions";
            case ApiProtocol::AnthropicMessages: return L"Anthropic Messages";
            }
            throw hresult_invalid_argument();
        }
    }

    void ProviderConnectionPicker::SetType(models::ProviderPreset const& type, ApiProtocol protocol) {
        type_ = &type;
        route_ = type.Find(protocol);
        if (!route_) route_ = &type.routes.front();
        protocols_.Items().Clear();
        int selection = 0;
        for (auto const& route : type.routes) {
            auto index = protocols_.Items().Size();
            protocols_.Items().Append(box_value(ProtocolLabel(route.protocol)));
            if (route.protocol == route_->protocol) selection = static_cast<int>(index);
        }
        protocols_.SelectedIndex(selection);
        protocols_.IsEnabled(type.routes.size() > 1);
    }

    void ProviderConnectionPicker::Build(StackPanel const& panel, std::string_view typeId, ApiProtocol protocol, Changed changed) {
        types_ = Choice();
        protocols_ = Choice();
        cards::ChoiceStyle(types_);
        cards::ChoiceStyle(protocols_);
        auto presets = models::ProviderPresets();
        auto const& initial = models::Preset(typeId);
        for (auto const& preset : presets)
            types_.Items().Append(box_value(preset.name.empty() ? Localize(L"AIProviderCustom") : to_hstring(preset.name)));
        types_.SelectedIndex(static_cast<int>(&initial - presets.data()));
        SetType(initial, protocol);
        panel.Children().Append(cards::Setting({Localize(L"AIProviderType"), {}, cards::Icon(WorkspaceIcon::ProviderService)}, types_));
        panel.Children().Append(cards::Setting({Localize(L"AIProtocol"), {}, cards::Icon(WorkspaceIcon::Source)}, protocols_));

        auto weak = weak_from_this();
        types_.SelectionChanged([weak, changed](auto const&, auto const&) {
            auto self = weak.lock();
            if (!self || self->types_.SelectedIndex() < 0) return;
            auto const& previousType = self->Type();
            auto const& previousRoute = self->Route();
            auto const& type = models::ProviderPresets()[self->types_.SelectedIndex()];
            if (&type == &previousType) return;
            self->SetType(type, type.routes.front().protocol);
            changed(previousType, previousRoute);
        });
        protocols_.SelectionChanged([weak, changed](auto const&, auto const&) {
            auto self = weak.lock();
            if (!self || self->protocols_.SelectedIndex() < 0) return;
            auto const& route = self->Type().routes.at(self->protocols_.SelectedIndex());
            auto const& previous = self->Route();
            if (&route == &previous) return;
            self->route_ = &route;
            changed(self->Type(), previous);
        });
    }
}
