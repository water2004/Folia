#include "pch.h"
#include "settings/SettingsView.h"
#include "settings/SettingsViewSupport.h"
#include "localization/Localization.h"

namespace winrt::Folia {
Microsoft::UI::Xaml::UIElement SettingsView::BuildCompletionPage() {
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;
    using namespace settings_ui;
    auto panel = PagePanel();
    panel.Children().Append(PageHeading(Localize(L"AICompletion")));
    panel.Children().Append(Text(Localize(L"AICompletionDescription")));
    StackPanel fields; fields.Spacing(16);
    ToggleSwitch enabled; enabled.Header(box_value(Localize(L"AICompletionEnabled")));
    enabled.IsOn(appliedSettings_.completion.enabled); fields.Children().Append(enabled);
    ComboBox protocol; protocol.Header(box_value(Localize(L"AIProtocol")));
    for (auto label : {L"OpenAI Chat Completions", L"OpenAI Responses", L"Anthropic Messages"}) protocol.Items().Append(box_value(label));
    protocol.SelectedIndex(appliedSettings_.completion.protocol); fields.Children().Append(protocol);
    TextBox endpoint; endpoint.Header(box_value(Localize(L"AIEndpoint")));
    endpoint.PlaceholderText(L"https://api.example.com/v1/chat/completions");
    endpoint.Text(winrt::to_hstring(appliedSettings_.completion.endpoint)); fields.Children().Append(endpoint);
    TextBox model; model.Header(box_value(Localize(L"AIModel")));
    model.Text(winrt::to_hstring(appliedSettings_.completion.model)); fields.Children().Append(model);
    PasswordBox key; key.Header(box_value(Localize(L"AIKey")));
    key.PlaceholderText(Localize(L"AIKeyUnchanged")); fields.Children().Append(key);
    CheckBox clearKey; clearKey.Content(box_value(Localize(L"AIClearKey"))); fields.Children().Append(clearKey);
    fields.Children().Append(Text(Localize(L"AICompletionBudget")));
    TextBlock status; status.TextWrapping(TextWrapping::Wrap);
    Button save; save.Content(box_value(Localize(L"Save")));
    save.Click([weak = weak_from_this(), enabled, protocol, endpoint, model, key, clearKey, status](auto const&, auto const&) {
        auto self = weak.lock(); if (!self || self->detached_ || !self->applySettings_) return;
        auto proposed = self->appliedSettings_;
        proposed.completion = {enabled.IsOn(), winrt::to_string(endpoint.Text()), winrt::to_string(model.Text()), protocol.SelectedIndex()};
        if (proposed.completion.enabled) if (auto error = ValidateCompletionConfig(proposed.completion); !error.empty()) {
            status.Text(winrt::to_hstring(error)); return;
        }
        try {
            if (clearKey.IsChecked().GetBoolean()) StoreCompletionKey({});
            else if (!key.Password().empty()) StoreCompletionKey(key.Password());
            if (auto error = self->applySettings_(proposed)) { status.Text(*error); return; }
            self->settings_.completion = proposed.completion;
            self->appliedSettings_.completion = proposed.completion;
            key.Password(L""); clearKey.IsChecked(false); status.Text(Localize(L"SettingsSaved"));
        } catch (winrt::hresult_error const& error) { status.Text(error.message()); }
        catch (std::exception const& error) { status.Text(winrt::to_hstring(error.what())); }
    });
    fields.Children().Append(save); fields.Children().Append(status); panel.Children().Append(Card(fields));
    ScrollViewer scroll; scroll.Content(panel); return scroll;
}
}
