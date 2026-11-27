#include "pch.h"
#include "settings/storage/ProviderPresets.h"
#include "settings/storage/ModelCatalog.h"
#include "ui/ControlHelpers.h"
#include "settings/views/ModelEffortChoice.h"
#include "settings/views/SettingsCards.h"
#include "settings/views/SettingsView.h"

namespace winrt::Folia {
    Microsoft::UI::Xaml::UIElement SettingsView::BuildCompletionPage() {
        using namespace ui;
        using namespace models;
        using namespace settings_ui;
        auto panel = PagePanel();
        panel.Style(ResourceStyle(L"SettingsOptionPageStyle"));
        panel.Children().Append(PageHeader(Localize(L"AICompletion"), Localize(L"AICompletionDescription")));
        auto connection = cards::List();
        auto enabled = cards::Switch(appliedSettings_.completion.enabled);
        connection.Children().Append(cards::Setting(
            {Localize(L"AICompletionEnabled"), {}, cards::Icon(WorkspaceIcon::Prediction)}, enabled));
        auto model = Choice();
        cards::ChoiceStyle(model);
        model.PlaceholderText(Localize(L"ChatPickModel"));
        auto choices = AvailableModels();
        int selected = -1;
        std::vector<bool> supportsCache;
        std::vector<std::vector<std::string>> effortOptions;
        for (auto const& choice : choices) {
            model.Items().Append(box_value(to_hstring(choice.label)));
            if (choice.model.id == appliedSettings_.completionModelId) selected = model.Items().Size() - 1;
            effortOptions.push_back(choice.model.efforts);
            supportsCache.push_back(folia::ai::SupportsCacheBreakpoints(Preset(choice.provider.providerType).cache,
                choice.provider.protocol, choice.model.upstream, true));
        }
        model.SelectedIndex(selected);
        connection.Children().Append(cards::Setting(
            {Localize(L"AIModel"), {}, cards::Icon(WorkspaceIcon::Model)}, model, cards::Placement::Wrap));
        auto effort = Choice();
        cards::ChoiceStyle(effort);
        auto effortRow = cards::Setting(
            {Localize(L"AIReasoning"), {}, cards::Icon(WorkspaceIcon::Reasoning)}, effort, cards::Placement::Wrap);
        connection.Children().Append(effortRow);
        panel.Children().Append(connection);

        auto related = cards::Link({Localize(L"ChatManageProviders"), {}, cards::Icon(WorkspaceIcon::ProviderService)},
            [weak = weak_from_this()] {
            if (auto self = weak.lock()) self->Navigate(Page::Providers);
        });

        auto fields = SectionPanel();
        auto eagerness = Choice();
        cards::ChoiceStyle(eagerness);
        for (auto label : {L"AIEagernessConservative", L"AIEagernessBalanced", L"AIEagernessProactive"})
            eagerness.Items().Append(box_value(Localize(label)));
        eagerness.SelectedIndex(static_cast<int>(appliedSettings_.completion.eagerness));
        fields.Children().Append(cards::Setting(
            {Localize(L"AIEagerness"), Localize(L"AIEagernessDescription"), cards::Icon(WorkspaceIcon::Prediction)},
            eagerness, cards::Placement::Wrap));
        TextBox context, output;
        auto field = [&](TextBox const& input, wchar_t const* name, std::uint32_t value) {
            TextField(input, Localize(name), TextInput::Number);
            input.Text(to_hstring(value));
            fields.Children().Append(input);
        };
        field(context, L"AIContextKB", appliedSettings_.completion.contextKB);
        field(output, L"AIOutputTokens", appliedSettings_.completion.outputTokens);
        auto cache = cards::Switch(appliedSettings_.completion.explicitPromptCache);
        auto cacheRow = cards::Setting(
            {Localize(L"AIExplicitCache"), Localize(L"AIExplicitCacheDescription"), cards::Icon(WorkspaceIcon::Cache)}, cache);
        auto update = [cache, cacheRow, effort, effortRow, supportsCache = std::move(supportsCache),
                       effortOptions = std::move(effortOptions)](int index, std::string_view selection) {
            auto visible = index >= 0 && supportsCache.at(index) ? Visibility::Visible : Visibility::Collapsed;
            cache.Visibility(visible);
            cacheRow.Visibility(visible);
            auto options = index >= 0 ? std::span<std::string const>(effortOptions.at(index)) : std::span<std::string const>{};
            SetEffortChoices(effort, options, selection);
            effortRow.Visibility(effort.Visibility());
        };
        update(selected, appliedSettings_.completion.reasoningEffort);
        model.SelectionChanged([update = std::move(update)](auto const& sender, auto const&) {
            update(sender.template as<ComboBox>().SelectedIndex(), {});
        });
        fields.Children().Append(cacheRow);
        panel.Children().Append(cards::Section(Localize(L"AIPredictionBehavior"), fields));

        auto actions = SectionPanel();
        auto save = Action(L"Save");
        save.Style(ResourceStyle(L"SettingsPrimaryButtonStyle"));
        TextBlock status;
        status.TextWrapping(TextWrapping::Wrap);
        status.Style(ResourceStyle(L"SettingsHintTextStyle"));
        actions.Children().Append(save);
        actions.Children().Append(status);
        panel.Children().Append(actions);
        panel.Children().Append(cards::Section(Localize(L"SettingsRelated"), related));
        save.Click([weak = weak_from_this(), enabled, model, choices = std::move(choices), effort, eagerness, context, output, cache, status](auto const&, auto const&) {
            auto self = weak.lock();
            if (!self || self->detached_ || !self->applySettings_) return;
            auto proposed = self->appliedSettings_;
            const auto index = model.SelectedIndex();
            if (enabled.IsOn() && index < 0) { SetStatus(status, Localize(L"ChatPickModel"), true); return; }
            auto parse = [](TextBox const& field, std::uint32_t& value) {
                auto bytes = FieldText(field);
                auto result = std::from_chars(bytes.data(), bytes.data() + bytes.size(), value);
                return result.ec == std::errc{} && result.ptr == bytes.data() + bytes.size();
            };
            auto& config = proposed.completion;
            if (!parse(context, config.contextKB) || !parse(output, config.outputTokens) ||
                config.contextKB < 1 || config.contextKB > 64 || config.outputTokens < 16 || config.outputTokens > 4096) {
                SetStatus(status, Localize(L"AIBudgetInvalid"), true);
                return;
            }
            proposed.completionModelId = index < 0 ? std::string{} : choices.at(index).model.id;
            config.enabled = enabled.IsOn();
            config.reasoningEffort = SelectedEffort(effort);
            config.eagerness = static_cast<folia::completion::Eagerness>(eagerness.SelectedIndex());
            config.explicitPromptCache = cache.Visibility() == Visibility::Visible && cache.IsOn();
            if (auto error = self->applySettings_(proposed)) { SetStatus(status, *error, true); return; }
            self->settings_.completion = config;
            self->settings_.completionModelId = proposed.completionModelId;
            self->appliedSettings_.completion = config;
            self->appliedSettings_.completionModelId = std::move(proposed.completionModelId);
            SetStatus(status, Localize(L"SettingsSaved"), false);
        });
        return PageScroll(panel);
    }
}
