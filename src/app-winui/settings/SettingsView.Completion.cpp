#include "pch.h"
#include "settings/SettingsView.h"
#include "settings/SettingsViewSupport.h"
#include "localization/Localization.h"
#include "completion/CompletionHttpTransport.h"

namespace winrt::Folia {
namespace {
using namespace Microsoft::UI::Xaml::Controls;
struct ModelQuery {
    std::stop_source stop;
    std::uint64_t generation = 0;
    std::vector<CompletionModel> models;
    std::optional<winrt::hstring> activePassword;
    bool activeClearKey = false;
    ~ModelQuery() { stop.request_stop(); }
    void Cancel() { stop.request_stop(); stop = {}; activePassword.reset(); ++generation; }
};
std::string SelectedValue(ComboBox const& box) {
    if (auto selected = box.SelectedItem().try_as<ComboBoxItem>())
        return winrt::to_string(winrt::unbox_value<winrt::hstring>(selected.Tag()));
    return {};
}
void SetModelChoices(ComboBox const& box, std::vector<CompletionModel> const& models, std::string const& selected) {
    box.Items().Clear();
    for (auto const& model : models) {
        ComboBoxItem item; item.Tag(winrt::box_value(winrt::to_hstring(model.id)));
        auto label = model.name.empty() || model.name == model.id ? model.id : model.name + " (" + model.id + ")";
        TextBlock text; text.Text(winrt::to_hstring(label));
        text.TextTrimming(Microsoft::UI::Xaml::TextTrimming::CharacterEllipsis);
        item.Content(text); ToolTipService::SetToolTip(item, winrt::box_value(winrt::to_hstring(label)));
        box.Items().Append(item);
        if (model.id == selected) box.SelectedItem(item);
    }
    // Refresh keeps a still-available selection. Otherwise require an explicit
    // choice instead of silently switching the user's model/provider cost.
}
void SetReasoningChoices(ComboBox const& box, std::vector<std::string> const& levels, std::string const& selected) {
    box.Items().Clear();
    ComboBoxItem fallback; fallback.Tag(winrt::box_value(L""));
    fallback.Content(winrt::box_value(Localize(L"AIReasoningDefault"))); box.Items().Append(fallback);
    box.SelectedIndex(0);
    for (auto const& level : levels) {
        ComboBoxItem item; item.Tag(winrt::box_value(winrt::to_hstring(level)));
        item.Content(winrt::box_value(winrt::to_hstring(level))); box.Items().Append(item);
        if (level == selected) box.SelectedItem(item);
    }
}
winrt::fire_and_forget ReadModelChoices(CompletionConfig config, std::wstring key, bool useSavedKey,
    std::stop_token stop, Microsoft::UI::Dispatching::DispatcherQueue queue,
    std::function<void(std::vector<CompletionModel>, std::string)> deliver) {
    co_await winrt::resume_background();
    std::vector<CompletionModel> models; std::string error;
    try {
        if (useSavedKey) key = LoadCompletionKey();
        models = FetchCompletionModels(config, stop, std::move(key));
    } catch (std::exception const& e) { error = e.what(); }
    catch (winrt::hresult_error const& e) { error = winrt::to_string(e.message()); }
    if (!stop.stop_requested()) queue.TryEnqueue([deliver = std::move(deliver), models = std::move(models), error = std::move(error)]() mutable {
        deliver(std::move(models), std::move(error));
    });
}
}
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
    auto eagerness = Choice(); eagerness.Header(box_value(Localize(L"AIEagerness")));
    eagerness.Style(ResourceStyle(L"SettingsComboBoxStyle"));
    for (auto label : {L"AIEagernessConservative", L"AIEagernessBalanced", L"AIEagernessProactive"})
        eagerness.Items().Append(box_value(Localize(label)));
    eagerness.SelectedIndex(static_cast<int>(appliedSettings_.completion.eagerness));
    prediction.Children().Append(eagerness);
    prediction.Children().Append(Hint(Localize(L"AIEagernessDescription")));
    TextBox contextKB; contextKB.Header(box_value(Localize(L"AIContextKB")));
    contextKB.Style(ResourceStyle(L"SettingsTextBoxStyle"));
    contextKB.Text(winrt::to_hstring(appliedSettings_.completion.contextKB)); prediction.Children().Append(contextKB);
    TextBox outputTokens; outputTokens.Header(box_value(Localize(L"AIOutputTokens")));
    outputTokens.Style(ResourceStyle(L"SettingsTextBoxStyle"));
    outputTokens.Text(winrt::to_hstring(appliedSettings_.completion.outputTokens)); prediction.Children().Append(outputTokens);
    panel.Children().Append(prediction);
    auto service = SectionPanel();
    service.Children().Append(SectionHeading(Localize(L"AIService")));
    auto protocol = Choice(); protocol.Header(box_value(Localize(L"AIProtocol")));
    protocol.Style(ResourceStyle(L"SettingsComboBoxStyle"));
    for (auto label : {L"OpenAI Chat Completions", L"OpenAI Responses", L"Anthropic Messages"}) protocol.Items().Append(box_value(label));
    protocol.SelectedIndex(appliedSettings_.completion.protocol); service.Children().Append(protocol);
    TextBox baseUrl; baseUrl.Header(box_value(Localize(L"AIBaseUrl")));
    baseUrl.Style(ResourceStyle(L"SettingsTextBoxStyle"));
    baseUrl.PlaceholderText(L"https://api.example.com/v1");
    baseUrl.Text(winrt::to_hstring(appliedSettings_.completion.baseUrl)); service.Children().Append(baseUrl);
    service.Children().Append(Hint(Localize(L"AIBaseUrlDescription")));
    auto model = Choice(); model.Header(box_value(Localize(L"AIModel")));
    model.Style(ResourceStyle(L"SettingsComboBoxStyle")); model.PlaceholderText(Localize(L"AIModelPlaceholder"));
    if (!appliedSettings_.completion.model.empty()) SetModelChoices(model,
        {{appliedSettings_.completion.model, appliedSettings_.completion.model, {}}}, appliedSettings_.completion.model);
    Button fetch; fetch.Content(box_value(Localize(L"AIFetchModels")));
    fetch.Style(ResourceStyle(L"SettingsSecondaryButtonStyle")); fetch.VerticalAlignment(VerticalAlignment::Bottom);
    Grid modelRow; modelRow.ColumnSpacing(Metric(L"SettingsSectionGap") / 2);
    ColumnDefinition fieldColumn, actionColumn; actionColumn.Width(GridLengthHelper::Auto());
    modelRow.ColumnDefinitions().Append(fieldColumn); modelRow.ColumnDefinitions().Append(actionColumn);
    Grid::SetColumn(fetch, 1); modelRow.Children().Append(model); modelRow.Children().Append(fetch);
    service.Children().Append(modelRow);
    auto reasoning = Choice(); reasoning.Header(box_value(Localize(L"AIReasoning")));
    reasoning.Style(ResourceStyle(L"SettingsComboBoxStyle"));
    auto savedEffort = appliedSettings_.completion.reasoningEffort;
    SetReasoningChoices(reasoning, savedEffort.empty() ? std::vector<std::string>{} : std::vector<std::string>{savedEffort}, savedEffort);
    service.Children().Append(reasoning);
    service.Children().Append(Hint(Localize(L"AIReasoningDescription")));
    TextBlock fetchStatus; fetchStatus.Style(ResourceStyle(L"SettingsHintTextStyle")); service.Children().Append(fetchStatus);
    panel.Children().Append(service);
    auto credentials = SectionPanel();
    credentials.Children().Append(SectionHeading(Localize(L"AICredentials")));
    PasswordBox key; key.Header(box_value(Localize(L"AIKey")));
    key.Style(ResourceStyle(L"SettingsPasswordBoxStyle"));
    key.PlaceholderText(Localize(L"AIKeyUnchanged")); credentials.Children().Append(key);
    CheckBox clearKey; clearKey.Content(box_value(Localize(L"AIClearKey"))); credentials.Children().Append(clearKey);
    panel.Children().Append(credentials);
    auto query = std::make_shared<ModelQuery>();
    auto cancelQuery = [query, weakFetch = winrt::make_weak(fetch), weakStatus = winrt::make_weak(fetchStatus)] {
        query->Cancel();
        if (auto button = weakFetch.get()) { button.IsEnabled(true); button.Content(box_value(Localize(L"AIFetchModels"))); }
        if (auto text = weakStatus.get()) SetStatus(text, {}, false);
    };
    auto resetQuery = [query, model, reasoning, cancelQuery] {
        cancelQuery(); query->models.clear(); model.Items().Clear(); SetReasoningChoices(reasoning, {}, {});
    };
    baseUrl.TextChanged([resetQuery](auto const&, auto const&) { resetQuery(); });
    protocol.SelectionChanged([resetQuery](auto const&, auto const&) { resetQuery(); });
    key.PasswordChanged([query, cancelQuery](auto const& sender, auto const&) {
        // WinUI can deliver PasswordChanged after the property assignment.
        // Retire only a request whose actual credential snapshot has changed.
        if (query->activePassword && *query->activePassword != sender.template as<PasswordBox>().Password()) cancelQuery();
    });
    auto clearChanged = [query, cancelQuery](auto const& sender, auto const&) {
        if (query->activePassword && query->activeClearKey != sender.template as<CheckBox>().IsChecked().GetBoolean()) cancelQuery();
    };
    clearKey.Checked(clearChanged); clearKey.Unchecked(clearChanged);
    panel.Unloaded([cancelQuery](auto const&, auto const&) { cancelQuery(); });
    model.SelectionChanged([query, weakReasoning = winrt::make_weak(reasoning)](auto const& sender, auto const&) {
        if (auto box = weakReasoning.get()) {
            auto id = SelectedValue(sender.template as<ComboBox>());
            auto found = std::ranges::find(query->models, id, &CompletionModel::id);
            SetReasoningChoices(box, found == query->models.end() ? std::vector<std::string>{} : found->reasoningEfforts, {});
        }
    });
    fetch.Click([weak = weak_from_this(), query, baseUrl, model, protocol, key, clearKey,
        weakFetch = winrt::make_weak(fetch), weakReasoning = winrt::make_weak(reasoning), weakStatus = winrt::make_weak(fetchStatus)](auto const&, auto const&) {
        auto self = weak.lock(); if (!self || self->detached_) return;
        CompletionConfig config{true, winrt::to_string(baseUrl.Text()), {}, protocol.SelectedIndex()};
        if (auto error = ValidateCompletionService(config); !error.empty()) {
            if (auto text = weakStatus.get()) SetStatus(text, winrt::to_hstring(error), true); return;
        }
        query->Cancel(); auto generation = query->generation;
        if (auto button = weakFetch.get()) { button.IsEnabled(false); button.Content(box_value(Localize(L"AIFetchModelsBusy"))); }
        auto password = key.Password();
        query->activePassword = password; query->activeClearKey = clearKey.IsChecked().GetBoolean();
        ReadModelChoices(config, std::wstring(password.c_str(), password.size()), password.empty() && !clearKey.IsChecked().GetBoolean(),
            query->stop.get_token(), Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread(),
            [weak, weakQuery = std::weak_ptr(query), generation, weakModel = winrt::make_weak(model), weakFetch, weakReasoning, weakStatus](auto models, auto error) mutable {
                auto self = weak.lock(); auto state = weakQuery.lock();
                if (!self || self->detached_ || !state || state->generation != generation) return;
                state->activePassword.reset();
                auto button = weakFetch.get(); auto box = weakReasoning.get(); auto text = weakStatus.get(); auto picker = weakModel.get();
                if (!button || !box || !text || !picker) return;
                button.IsEnabled(true); button.Content(box_value(Localize(L"AIFetchModels")));
                if (!error.empty()) { SetStatus(text, winrt::to_hstring(error), true); return; }
                auto previousModel = SelectedValue(picker); auto previousEffort = SelectedValue(box);
                state->models = std::move(models); SetModelChoices(picker, state->models, previousModel);
                if (auto found = std::ranges::find(state->models, SelectedValue(picker), &CompletionModel::id); found != state->models.end())
                    SetReasoningChoices(box, found->reasoningEfforts, previousEffort);
                else SetReasoningChoices(box, {}, {});
                SetStatus(text, state->models.empty() ? Localize(L"AINoModels")
                    : LocalizeFormat(L"AIModelsFetched", {winrt::to_hstring(state->models.size())}), false);
            });
    });
    TextBlock status; status.TextWrapping(TextWrapping::Wrap);
    status.Style(ResourceStyle(L"SettingsHintTextStyle"));
    Button save; save.Content(box_value(Localize(L"Save")));
    save.Style(ResourceStyle(L"SettingsPrimaryButtonStyle"));
    save.Click([weak = weak_from_this(), enabled, protocol, baseUrl, model, reasoning, eagerness, contextKB, outputTokens, key, clearKey, status](auto const&, auto const&) {
        auto self = weak.lock(); if (!self || self->detached_ || !self->applySettings_) return;
        auto proposed = self->appliedSettings_;
        proposed.completion = {enabled.IsOn(), winrt::to_string(baseUrl.Text()), SelectedValue(model), protocol.SelectedIndex()};
        if (auto selected = reasoning.SelectedItem().try_as<ComboBoxItem>())
            proposed.completion.reasoningEffort = winrt::to_string(unbox_value<winrt::hstring>(selected.Tag()));
        proposed.completion.eagerness = static_cast<folia::completion::Eagerness>(eagerness.SelectedIndex());
        std::uint32_t context{}, output{};
        auto parse = [](winrt::hstring const& text, std::uint32_t& number) {
            auto value = winrt::to_string(text);
            auto result = std::from_chars(value.data(), value.data() + value.size(), number);
            return result.ec == std::errc{} && result.ptr == value.data() + value.size();
        };
        if (!parse(contextKB.Text(), context) || !parse(outputTokens.Text(), output)
            || context < 1 || context > 64 || output < 16 || output > 4096) {
            SetStatus(status, Localize(L"AIBudgetInvalid"), true); return;
        }
        proposed.completion.contextKB = context;
        proposed.completion.outputTokens = output;
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
