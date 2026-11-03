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
    panel.Children().Append(PageHeader(Localize(L"AICompletion"), Localize(L"AICompletionDescription")));
    auto prediction = SectionPanel();
    ToggleSwitch enabled; enabled.Header(box_value(Localize(L"AICompletionEnabled")));
    enabled.IsOn(appliedSettings_.completion.enabled); prediction.Children().Append(enabled);
    prediction.Children().Append(Hint(Localize(L"AICompletionBudget")));
    panel.Children().Append(prediction);
    auto service = SectionPanel();
    service.Children().Append(SectionHeading(Localize(L"AIService")));
    ComboBox protocol; protocol.Header(box_value(Localize(L"AIProtocol")));
    protocol.Style(ResourceStyle(L"SettingsComboBoxStyle"));
    for (auto label : {L"OpenAI Chat Completions", L"OpenAI Responses", L"Anthropic Messages"}) protocol.Items().Append(box_value(label));
    protocol.SelectedIndex(appliedSettings_.completion.protocol); service.Children().Append(protocol);
    TextBox endpoint; endpoint.Header(box_value(Localize(L"AIEndpoint")));
    endpoint.Style(ResourceStyle(L"SettingsTextBoxStyle"));
    endpoint.PlaceholderText(L"https://api.example.com/v1/chat/completions");
    endpoint.Text(winrt::to_hstring(appliedSettings_.completion.endpoint)); service.Children().Append(endpoint);
    TextBox model; model.Header(box_value(Localize(L"AIModel")));
    model.Style(ResourceStyle(L"SettingsTextBoxStyle"));
    model.Text(winrt::to_hstring(appliedSettings_.completion.model)); service.Children().Append(model);
    panel.Children().Append(service);
    auto credentials = SectionPanel();
    credentials.Children().Append(SectionHeading(Localize(L"AICredentials")));
    PasswordBox key; key.Header(box_value(Localize(L"AIKey")));
    key.Style(ResourceStyle(L"SettingsPasswordBoxStyle"));
    key.PlaceholderText(Localize(L"AIKeyUnchanged")); credentials.Children().Append(key);
    CheckBox clearKey; clearKey.Content(box_value(Localize(L"AIClearKey"))); credentials.Children().Append(clearKey);
    panel.Children().Append(credentials);
    TextBlock status; status.TextWrapping(TextWrapping::Wrap);
    status.Style(ResourceStyle(L"SettingsHintTextStyle"));
    Button save; save.Content(box_value(Localize(L"Save")));
    save.Style(ResourceStyle(L"SettingsPrimaryButtonStyle"));
    save.Click([weak = weak_from_this(), enabled, protocol, endpoint, model, key, clearKey, status](auto const&, auto const&) {
        auto self = weak.lock(); if (!self || self->detached_ || !self->applySettings_) return;
        auto proposed = self->appliedSettings_;
        proposed.completion = {enabled.IsOn(), winrt::to_string(endpoint.Text()), winrt::to_string(model.Text()), protocol.SelectedIndex()};
        if (proposed.completion.enabled) if (auto error = ValidateCompletionConfig(proposed.completion); !error.empty()) {
            SetStatus(status, winrt::to_hstring(error), true); return;
        }
        try {
            if (clearKey.IsChecked().GetBoolean()) StoreCompletionKey({});
            else if (!key.Password().empty()) StoreCompletionKey(key.Password());
            if (auto error = self->applySettings_(proposed)) { SetStatus(status, *error, true); return; }
            self->settings_.completion = proposed.completion;
            self->appliedSettings_.completion = proposed.completion;
            key.Password(L""); clearKey.IsChecked(false); SetStatus(status, Localize(L"SettingsSaved"), false);
        } catch (winrt::hresult_error const& error) { SetStatus(status, error.message(), true); }
        catch (std::exception const& error) { SetStatus(status, winrt::to_hstring(error.what()), true); }
    });
    auto actions = SectionPanel(); actions.Children().Append(save); actions.Children().Append(status);
    panel.Children().Append(actions);
    return PageScroll(panel);
}
}
