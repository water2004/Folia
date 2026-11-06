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
    bool fetched = false;
    std::optional<winrt::hstring> activePassword;
    bool activeClearKey = false;
    ~ModelQuery() { stop.request_stop(); }
    void Cancel() { stop.request_stop(); stop = {}; activePassword.reset(); ++generation; }
};
void SetModelChoices(ListView const& list, DropDownButton const& button, std::span<const CompletionModel> models) {
    auto items = winrt::single_threaded_observable_vector<winrt::Windows::Foundation::IInspectable>();
    for (auto const& model : models) {
        items.Append(winrt::box_value(winrt::to_hstring(model.id)));
    }
    list.ItemsSource(items);
    button.IsEnabled(!models.empty());
}
void SetReasoningChoices(MenuFlyout const& menu, TextBox const& field, TextBlock const& hint,
    std::span<const std::string> levels, bool hasModel) {
    menu.Items().Clear();
    auto add = [&](winrt::hstring label, winrt::hstring value) {
        MenuFlyoutItem item;
        item.Text(label);
        item.Click([weak = winrt::make_weak(field), value = std::move(value)](auto const&, auto const&) {
            if (auto input = weak.get()) input.Text(value);
        });
        menu.Items().Append(item);
    };
    add(Localize(L"AIReasoningDefault"), {});
    for (auto const& level : levels) {
        auto label = winrt::to_hstring(level);
        add(label, label);
    }
    hint.Text(Localize(!hasModel ? L"AIReasoningSelectModel" : levels.empty() ? L"AIReasoningMissingMetadata" : L"AIReasoningDescription"));
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
    TextBox model;
    model.Header(box_value(Localize(L"AIModel")));
    model.Style(ResourceStyle(L"SettingsTextBoxStyle"));
    model.PlaceholderText(Localize(L"AIModelPlaceholder"));
    model.Text(winrt::to_hstring(appliedSettings_.completion.model));
    DropDownButton modelOptions;
    modelOptions.Content(box_value(Localize(L"AIModelChoices")));
    modelOptions.VerticalAlignment(VerticalAlignment::Bottom);
    modelOptions.IsEnabled(false);
    Flyout modelMenu;
    ListView modelList;
    modelList.MaxHeight(Metric(L"SettingsModelPickerMaxHeight"));
    modelList.Width(Metric(L"SettingsModelPickerWidth"));
    modelList.SingleSelectionFollowsFocus(false);
    modelMenu.Content(modelList);
    modelOptions.Flyout(modelMenu);
    modelList.SelectionChanged([field = winrt::make_weak(model), menu = winrt::make_weak(modelMenu)](auto const& sender, auto const&) {
        auto list = sender.template as<ListView>();
        if (auto selected = list.SelectedItem()) {
            if (auto input = field.get()) input.Text(winrt::unbox_value<winrt::hstring>(selected));
            list.SelectedIndex(-1);
            if (auto flyout = menu.get()) flyout.Hide();
        }
    });
    Button fetch; fetch.Content(box_value(Localize(L"AIFetchModels")));
    fetch.Style(ResourceStyle(L"SettingsSecondaryButtonStyle")); fetch.VerticalAlignment(VerticalAlignment::Bottom);
    Grid modelRow; modelRow.ColumnSpacing(Metric(L"SettingsSectionGap") / 2);
    ColumnDefinition fieldColumn, modelChoicesColumn, actionColumn;
    modelChoicesColumn.Width(GridLengthHelper::Auto()); actionColumn.Width(GridLengthHelper::Auto());
    modelRow.ColumnDefinitions().Append(fieldColumn); modelRow.ColumnDefinitions().Append(modelChoicesColumn);
    modelRow.ColumnDefinitions().Append(actionColumn);
    Grid::SetColumn(modelOptions, 1); Grid::SetColumn(fetch, 2);
    modelRow.Children().Append(model); modelRow.Children().Append(modelOptions); modelRow.Children().Append(fetch);
    service.Children().Append(modelRow);
    TextBox reasoning;
    reasoning.Header(box_value(Localize(L"AIReasoning")));
    reasoning.Style(ResourceStyle(L"SettingsTextBoxStyle"));
    reasoning.PlaceholderText(Localize(L"AIReasoningDefault"));
    reasoning.Text(winrt::to_hstring(appliedSettings_.completion.reasoningEffort));
    DropDownButton reasoningOptions;
    reasoningOptions.Content(box_value(Localize(L"AIReasoningChoices")));
    reasoningOptions.VerticalAlignment(VerticalAlignment::Bottom);
    MenuFlyout reasoningMenu;
    reasoningOptions.Flyout(reasoningMenu);
    auto reasoningHint = Hint(L"");
    SetReasoningChoices(reasoningMenu, reasoning, reasoningHint, {}, false);
    Grid reasoningRow; reasoningRow.ColumnSpacing(Metric(L"SettingsSectionGap") / 2);
    ColumnDefinition effortColumn, choicesColumn; choicesColumn.Width(GridLengthHelper::Auto());
    reasoningRow.ColumnDefinitions().Append(effortColumn); reasoningRow.ColumnDefinitions().Append(choicesColumn);
    Grid::SetColumn(reasoningOptions, 1); reasoningRow.Children().Append(reasoning); reasoningRow.Children().Append(reasoningOptions);
    service.Children().Append(reasoningRow);
    service.Children().Append(reasoningHint);
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
    modelList.ContainerContentChanging([query](auto const&, ContainerContentChangingEventArgs const& args) {
        if (args.InRecycleQueue()) return;
        const auto index = static_cast<std::size_t>(args.ItemIndex());
        if (index >= query->models.size()) return;
        auto const& entry = query->models[index];
        auto label = entry.name.empty() || entry.name == entry.id ? entry.id : entry.name + " (" + entry.id + ")";
        ToolTipService::SetToolTip(args.ItemContainer(), winrt::box_value(winrt::to_hstring(label)));
    });
    auto cancelQuery = [query, weakFetch = winrt::make_weak(fetch), weakStatus = winrt::make_weak(fetchStatus)] {
        query->Cancel();
        if (auto button = weakFetch.get()) { button.IsEnabled(true); button.Content(box_value(Localize(L"AIFetchModels"))); }
        if (auto text = weakStatus.get()) SetStatus(text, {}, false);
    };
    auto resetQuery = [query, model, modelList, modelOptions, reasoning, reasoningMenu, reasoningHint, cancelQuery] {
        cancelQuery(); query->models.clear(); query->fetched = false;
        SetModelChoices(modelList, modelOptions, {}); model.Text(L"");
        reasoning.Text(L"");
        SetReasoningChoices(reasoningMenu, reasoning, reasoningHint, {}, false);
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
    auto updateReasoningOptions = [weakReasoning = winrt::make_weak(reasoning), weakMenu = winrt::make_weak(reasoningMenu),
        weakHint = winrt::make_weak(reasoningHint)](ModelQuery const& state, std::string const& id) {
        auto hint = weakHint.get();
        auto field = weakReasoning.get();
        auto menu = weakMenu.get();
        if (!field || !menu || !hint) return;
        auto found = std::ranges::find(state.models, id, &CompletionModel::id);
        auto levels = found == state.models.end() ? std::span<const std::string>{} : std::span{found->reasoningEfforts};
        SetReasoningChoices(menu, field, hint, levels, state.fetched && !id.empty());
    };
    model.TextChanged([query, updateReasoningOptions](auto const& sender, auto const&) {
        updateReasoningOptions(*query, winrt::to_string(sender.template as<TextBox>().Text()));
    });
    fetch.Click([weak = weak_from_this(), query, baseUrl, model, protocol, key, clearKey,
        weakFetch = winrt::make_weak(fetch), weakStatus = winrt::make_weak(fetchStatus),
        weakList = winrt::make_weak(modelList), weakOptions = winrt::make_weak(modelOptions), updateReasoningOptions](auto const&, auto const&) {
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
            [weak, weakQuery = std::weak_ptr(query), generation, weakModel = winrt::make_weak(model),
                weakFetch, weakStatus, weakList, weakOptions, updateReasoningOptions](auto models, auto error) mutable {
                auto self = weak.lock(); auto state = weakQuery.lock();
                if (!self || self->detached_ || !state || state->generation != generation) return;
                state->activePassword.reset();
                auto button = weakFetch.get(); auto text = weakStatus.get(); auto picker = weakModel.get();
                auto list = weakList.get(); auto options = weakOptions.get();
                if (!button || !text || !picker || !list || !options) return;
                button.IsEnabled(true); button.Content(box_value(Localize(L"AIFetchModels")));
                if (!error.empty()) { SetStatus(text, winrt::to_hstring(error), true); return; }
                state->models = std::move(models); state->fetched = true; SetModelChoices(list, options, state->models);
                updateReasoningOptions(*state, winrt::to_string(picker.Text()));
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
        proposed.completion.enabled = enabled.IsOn();
        proposed.completion.baseUrl = winrt::to_string(baseUrl.Text());
        proposed.completion.model = winrt::to_string(model.Text());
        proposed.completion.protocol = protocol.SelectedIndex();
        proposed.completion.reasoningEffort = winrt::to_string(reasoning.Text());
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
