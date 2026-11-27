#include "pch.h"
#include "ui/ControlHelpers.h"
#include "settings/views/ModelEffortChoice.h"
#include "settings/views/ProviderModelForm.h"
#include "settings/views/SettingsCards.h"

namespace winrt::Folia::settings_ui {
    using namespace ui;
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    namespace {

        bool ReadLimit(TextBox const& field, std::optional<std::uint32_t>& result) {
            auto text = FieldText(field);
            if (text.empty()) { result.reset(); return true; }
            std::uint32_t count{};
            auto end = text.data() + text.size();
            auto parsed = std::from_chars(text.data(), end, count);
            if (parsed.ec != std::errc{} || parsed.ptr != end || !count) return false;
            result = count;
            return true;
        }

        hstring FormatLimit(std::optional<std::uint32_t> value) {
            return value ? to_hstring(*value) : hstring{};
        }

        std::vector<std::string_view> EffortPresets(models::ProviderRoute const& route) {
            static constexpr std::string_view openAI[]{"none", "minimal", "low", "medium", "high", "xhigh", "max"};
            static constexpr std::string_view messages[]{"low", "medium", "high", "xhigh", "max"};
            if (!route.effortChoices.empty()) return {route.effortChoices.begin(), route.effortChoices.end()};
            if (route.protocol == models::ApiProtocol::AnthropicMessages) return {std::begin(messages), std::end(messages)};
            return {std::begin(openAI), std::end(openAI)};
        }
    }

    ProviderModelForm::ProviderModelForm(models::ConfiguredModel draft, models::ModelProfile const& connection)
        : connectionFormat_(connection.webFormat), draft_(std::move(draft)),
          route_(*models::Preset(connection.providerType).Find(connection.protocol)) {}

    std::shared_ptr<ProviderModelForm> ProviderModelForm::Create(models::ConfiguredModel model, models::ModelProfile const& connection,
        Save save, Command cancel, Command remove, Command fetch) {
        auto form = std::shared_ptr<ProviderModelForm>(new ProviderModelForm(std::move(model), connection));
        form->Build(std::move(save), std::move(cancel), std::move(remove), std::move(fetch));
        return form;
    }

    void ProviderModelForm::Build(Save save, Command cancel, Command remove, Command fetch) {
        panel_ = PagePanel();
        panel_.Style(ResourceStyle(L"SettingsOptionPageStyle"));
        auto weak = weak_from_this();
        auto command = [](Button const& button, Command action) {
            button.Click([action = std::move(action)](auto const&, auto const&) { action(); });
        };
        auto catalog = cards::List();
        choices_ = Choice();
        cards::ChoiceStyle(choices_);
        choices_.IsEnabled(false);
        choices_.PlaceholderText(Localize(L"ChatNoFetchedModels"));
        fetch_ = Action(L"AIFetchModels");
        fetch_.Style(ResourceStyle(L"SettingsSecondaryButtonStyle"));
        command(fetch_, std::move(fetch));
        catalog.Children().Append(cards::Setting({Localize(L"ChatChooseUpstream"), {}, cards::Icon(WorkspaceIcon::Model)},
            choices_, cards::Placement::Wrap));
        catalog.Children().Append(queryStatus_);
        QueryStatus({});
        panel_.Children().Append(cards::Section(Localize(L"ChatModelSource"), catalog, fetch_));
        choices_.SelectionChanged([weak](auto const&, auto const&) {
            auto self = weak.lock();
            if (!self || self->choices_.SelectedIndex() < 0) return;
            auto const& metadata = self->catalog_.at(self->choices_.SelectedIndex());
            models::ConfiguredModel model;
            model.upstream = metadata.id;
            model.name = metadata.name;
            model.efforts = metadata.reasoningEfforts;
            model.maxInputTokens = metadata.maxInputTokens;
            model.maxOutputTokens = metadata.maxOutputTokens;
            model.vision = metadata.vision;
            model.webSearch = metadata.webSearch;
            model.webFetch = metadata.webFetch;
            model.searchFormat = metadata.searchFormat;
            self->Fill(model);
        });

        auto identity = SectionPanel();
        TextField(name_, Localize(L"ChatModelName"));
        TextField(upstream_, Localize(L"ChatUpstreamModel"));
        TextField(context_, Localize(L"ChatModelContextLimit"), TextInput::Number);
        TextField(output_, Localize(L"ChatModelOutputLimit"), TextInput::Number);
        for (auto const& field : {name_, upstream_, context_, output_})
            identity.Children().Append(field);
        panel_.Children().Append(cards::Section(Localize(L"ChatModelDetails"), identity));

        auto reasoning = cards::List();
        efforts_.Style(ResourceStyle(L"SettingsModelEffortsStyle"));
        reasoning.Children().Append(cards::Setting(
            {Localize(L"ChatAllowedEfforts"), Localize(L"ChatModelEffortHint"), cards::Icon(WorkspaceIcon::Reasoning)},
            efforts_, cards::Placement::Below));
        defaultEffort_ = Choice();
        cards::ChoiceStyle(defaultEffort_);
        reasoning.Children().Append(cards::Setting({Localize(L"ChatDefaultEffort"), {}, cards::Icon(WorkspaceIcon::Reasoning)},
            defaultEffort_, cards::Placement::Wrap));
        panel_.Children().Append(cards::Section(Localize(L"AIReasoning"), reasoning));

        auto capabilities = cards::List();
        vision_ = cards::Switch();
        capabilities.Children().Append(cards::Setting({Localize(L"ChatModelVision"), {}, cards::Icon(WorkspaceIcon::Image)}, vision_));
        if (!route_.search.empty()) {
            search_ = cards::Switch();
            capabilities.Children().Append(cards::Setting(
                {Localize(route_.Pages(connectionFormat_ ? connectionFormat_ : draft_.searchFormat) == folia::ai::PageAccess::WithSearch ? L"ChatSearchAndBrowse" : L"ChatModelWebSearch"), Localize(L"ChatModelWebSearchHint"), cards::Icon(WorkspaceIcon::Network)},
                search_, cards::Placement::Wrap));
        }
        if (route_.HasPageTool()) {
            readPages_ = cards::Switch();
            readPagesRow_ = cards::Setting({Localize(L"ChatModelPageRead"), {}, cards::Icon(WorkspaceIcon::Document)}, readPages_);
            capabilities.Children().Append(readPagesRow_);
        }
        panel_.Children().Append(cards::Section(Localize(L"ChatModelCapabilities"), capabilities));

        auto buttons = ButtonRow();
        auto apply = Action(L"ChatSaveModel");
        apply.Style(ResourceStyle(L"SettingsPrimaryButtonStyle"));
        command(apply, [weak, save = std::move(save)] { if (auto self = weak.lock()) self->Apply(save); });
        buttons.Children().Append(apply);
        auto close = Action(L"Cancel");
        command(close, std::move(cancel));
        buttons.Children().Append(close);
        if (remove) {
            auto deleteModel = Action(L"Remove");
            command(deleteModel, [weak, remove = std::move(remove)] {
                if (auto self = weak.lock()) {
                    try { remove(); }
                    catch (std::exception const& e) { cards::Status(self->validation_, to_hstring(e.what()), true); }
                    catch (hresult_error const& e) { cards::Status(self->validation_, e.message(), true); }
                }
            });
            buttons.Children().Append(deleteModel);
        }
        panel_.Children().Append(buttons);
        panel_.Children().Append(validation_);
        cards::Status(validation_, {});
        Fill(draft_);
    }

    void ProviderModelForm::RefreshPageCapability() {
        if (!readPagesRow_) return;
        auto format = connectionFormat_ ? connectionFormat_ : draft_.searchFormat;
        bool show = folia::ai::SeparatePageTool(route_.Pages(format)) || (!format && route_.requiresSearchFormat);
        readPagesRow_.Visibility(show ? Visibility::Visible : Visibility::Collapsed);
    }

    void ProviderModelForm::Fill(models::ConfiguredModel const& model) {
        name_.Text(to_hstring(model.name));
        upstream_.Text(to_hstring(model.upstream));
        context_.Text(FormatLimit(model.maxInputTokens));
        output_.Text(FormatLimit(model.maxOutputTokens));
        vision_.IsOn(model.vision);
        search_.IsOn(model.webSearch);
        draft_.searchFormat = model.searchFormat;
        readPages_.IsOn(model.webFetch);
        RefreshPageCapability();
        levels_.clear();
        efforts_.Children().Clear();
        auto presets = EffortPresets(route_);
        for (auto level : presets)
            AddEffort(level, std::ranges::contains(model.efforts, level));
        for (auto const& level : model.efforts)
            if (!std::ranges::contains(presets, level)) AddEffort(level, true);
        RefreshDefault(model.effort);
    }

    void ProviderModelForm::AddEffort(std::string_view level, bool checked) {
        CheckBox check;
        check.Style(ResourceStyle(L"SettingsModelEffortToggleStyle"));
        check.Content(box_value(to_hstring(level)));
        check.IsChecked(checked);
        auto changed = [weak = weak_from_this()](auto const&, auto const&) {
            if (auto self = weak.lock()) self->RefreshDefault(SelectedEffort(self->defaultEffort_));
        };
        check.Checked(changed);
        check.Unchecked(changed);
        efforts_.Children().Append(check);
        levels_.emplace_back(level, check);
    }

    std::vector<std::string> ProviderModelForm::CheckedEfforts() const {
        std::vector<std::string> levels;
        for (auto const& [level, check] : levels_)
            if (check.IsChecked().GetBoolean()) levels.push_back(level);
        return levels;
    }

    void ProviderModelForm::RefreshDefault(std::string_view selection) {
        auto efforts = CheckedEfforts();
        SetEffortChoices(defaultEffort_, efforts, selection, Localize(L"AIReasoningDefault"));
        defaultEffort_.Visibility(Visibility::Visible);
        defaultEffort_.IsEnabled(!efforts.empty());
    }

    void ProviderModelForm::Apply(Save const& save) {
        auto model = draft_;
        model.name = FieldText(name_);
        model.upstream = FieldText(upstream_);
        model.efforts = CheckedEfforts();
        if (model.upstream.empty() || model.upstream.size() > 128 || model.name.size() > 256 ||
            model.efforts.size() > 32 || !ReadLimit(context_, model.maxInputTokens) || !ReadLimit(output_, model.maxOutputTokens)) {
            cards::Status(validation_, Localize(L"ChatInvalidModel"), true);
            return;
        }
        model.effort = SelectedEffort(defaultEffort_);
        model.vision = vision_.IsOn();
        model.webSearch = !route_.search.empty() && search_.IsOn();
        model.webFetch = readPagesRow_ && readPagesRow_.Visibility() == Visibility::Visible && readPages_.IsOn();
        try { save(std::move(model)); }
        catch (std::exception const& e) { cards::Status(validation_, to_hstring(e.what()), true); }
        catch (hresult_error const& e) { cards::Status(validation_, e.message(), true); }
    }

    void ProviderModelForm::SetCatalog(std::vector<folia::platform::completion::CompletionModel> models) {
        choices_.SelectedIndex(-1);
        choices_.Items().Clear();
        catalog_ = std::move(models);
        for (auto const& model : catalog_) {
            auto caption = model.name.empty() || model.name == model.id ? model.id : model.name + " · " + model.id;
            choices_.Items().Append(box_value(to_hstring(caption)));
        }
        choices_.IsEnabled(!catalog_.empty());
        choices_.PlaceholderText(Localize(catalog_.empty() ? L"ChatNoFetchedModels" : L"ChatChooseUpstream"));
    }

    void ProviderModelForm::QueryStatus(hstring const& text, bool busy, bool error) {
        fetch_.IsEnabled(!busy);
        cards::Status(queryStatus_, text, error);
    }
}
