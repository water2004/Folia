#include "pch.h"
#include "agent/ChatModelPicker.h"
#include "agent/ChatControls.h"
#include "agent/ChatPalette.h"
#include <wil/resource.h>

namespace winrt::Folia::chat {
    namespace {
        void NameAction(Button const& button, wchar_t const* key) {
            auto label = Localize(key);
            Automation::AutomationProperties::SetName(button, label);
            ToolTipService::SetToolTip(button, box_value(label));
        }

        hstring EffortLabel(std::string_view level) {
            constexpr std::pair<std::string_view, wchar_t const*> labels[]{
                {"", L"AIReasoningDefault"}, {"none", L"ChatEffortNone"}, {"minimal", L"ChatEffortMinimal"},
                {"low", L"ChatEffortLow"}, {"medium", L"ChatEffortMedium"}, {"high", L"ChatEffortHigh"},
                {"xhigh", L"ChatEffortExtraHigh"}, {"max", L"ChatEffortMax"}};
            auto found = std::ranges::find(labels, level, &std::pair<std::string_view, wchar_t const*>::first);
            return found == std::end(labels) ? to_hstring(level) : Localize(found->second);
        }

        template<class Element>
        Element LoadTemplate(wchar_t const* name) {
            return Application::Current().Resources().Lookup(box_value(name)).as<DataTemplate>().LoadContent().as<Element>();
        }
    }

    ChatModelPicker::ChatModelPicker(std::function<void(int)> selectModel, std::function<void(std::string)> selectEffort,
                                 std::function<void()> configureModels) {
        chip_ = LoadTemplate<Button>(L"ChatModelChipTemplate");
        chipName_ = chip_.FindName(L"ModelName").as<TextBlock>();
        chipEffort_ = chip_.FindName(L"ModelEffort").as<TextBlock>();
        auto panel = LoadTemplate<Grid>(L"ChatModelPickerTemplate");
        reasoning_ = panel.FindName(L"ReasoningPanel").as<StackPanel>();
        models_ = panel.FindName(L"ModelsPanel").as<StackPanel>();
        missingModels_ = panel.FindName(L"MissingModels").as<StackPanel>();
        list_ = panel.FindName(L"ModelList").as<ListView>();
        efforts_ = panel.FindName(L"Efforts").as<Slider>();
        track_ = panel.FindName(L"Track").as<Grid>();
        scale_ = panel.FindName(L"Scale").as<Grid>();
        fill_ = panel.FindName(L"Fill").as<Border>();
        knob_ = panel.FindName(L"Knob").as<Border>();
        rail_ = panel.FindName(L"Rail").as<Border>();
        ticks_ = panel.FindName(L"Ticks").as<Canvas>();
        effortTitle_ = panel.FindName(L"EffortTitle").as<TextBlock>();
        auto outline = panel.FindName(L"FocusOutline").as<Border>();
        efforts_.GotFocus([outline](auto const& sender, auto const&) {
            outline.Visibility(sender.template as<Slider>().FocusState() == FocusState::Keyboard ? Visibility::Visible : Visibility::Collapsed);
        });
        efforts_.LostFocus([outline](auto const&, auto const&) { outline.Visibility(Visibility::Collapsed); });
        trackSize_ = track_.SizeChanged(winrt::auto_revoke, [this](auto const&, auto const&) { DrawScale(); });
        currentModel_ = panel.FindName(L"CurrentModel").as<TextBlock>();
        modelLink_ = panel.FindName(L"ChooseModel").as<Button>();
        back_ = panel.FindName(L"BackToReasoning").as<Button>();
        configure_ = panel.FindName(L"ConfigureModels").as<Button>();
        panel.FindName(L"ChooseModelTitle").as<TextBlock>().Text(Localize(L"ChatPickModel"));
        panel.FindName(L"MissingModelsMessage").as<TextBlock>().Text(Localize(L"ChatNeedsProvider"));
        configure_.Content(box_value(Localize(L"ChatManageProviders")));
        NameAction(modelLink_, L"ChatPickModel");
        NameAction(back_, L"AIReasoning");
        Automation::AutomationProperties::SetName(list_, Localize(L"ChatPickModel"));
        Automation::AutomationProperties::SetName(efforts_, Localize(L"AIReasoning"));

        flyout_.Placement(Primitives::FlyoutPlacementMode::TopEdgeAlignedRight);
        flyout_.FlyoutPresenterStyle(settings_ui::ResourceStyle(L"ChatModelPickerPresenterStyle"));
        flyout_.Content(panel);
        chip_.Flyout(flyout_);
        opening_ = flyout_.Opening(winrt::auto_revoke, [this](auto const&, auto const&) { ShowModels(levels_.empty()); });
        pickClick_ = modelLink_.Click(winrt::auto_revoke, [this](auto const&, auto const&) {
            ShowModels(true);
            list_.Focus(FocusState::Programmatic);
        });
        backClick_ = back_.Click(winrt::auto_revoke, [this](auto const&, auto const&) {
            ShowModels(false);
            modelLink_.Focus(FocusState::Programmatic);
        });
        effortChanged_ = efforts_.ValueChanged(winrt::auto_revoke, [this, selectEffort = std::move(selectEffort)](auto const&, auto const&) {
            auto index = static_cast<std::size_t>(std::llround(efforts_.Value()));
            if (updating_ || index >= levels_.size()) return;
            auto level = levels_.at(index);
            PresentReasoning(level);
            selectEffort(level);
        });
        modelChanged_ = list_.SelectionChanged(winrt::auto_revoke, [this, selectModel = std::move(selectModel)](auto const&, auto const&) {
            if (updating_ || SelectedIndex() < 0) return;
            RefreshSelection();
            selectModel(SelectedIndex());
            if (flyout_.IsOpen() && !levels_.empty()) {
                ShowModels(false);
                modelLink_.Focus(FocusState::Programmatic);
            }
        });
        settingsClick_ = configure_.Click(winrt::auto_revoke, [this, configureModels = std::move(configureModels)](auto const&, auto const&) {
            flyout_.Hide();
            configureModels();
        });
    }

    int ChatModelPicker::SelectedIndex() const { return list_.SelectedIndex(); }

    void ChatModelPicker::SetEnabled(bool enabled) {
        if (!enabled && flyout_.IsOpen()) flyout_.Hide();
        chip_.IsEnabled(enabled);
    }

    void ChatModelPicker::SetModels(std::span<hstring const> names, int selected) {
        auto restore = wil::scope_exit([&, previous = std::exchange(updating_, true)] { updating_ = previous; });
        list_.Items().Clear();
        checks_.clear();
        checked_ = -1;
        for (auto const& name : names) {
            Grid content;
            Columns(content, {true, false});
            content.ColumnDefinitions().GetAt(1).Width(GridLengthHelper::FromPixels(16));
            content.ColumnSpacing(10);
            TextBlock title;
            title.Style(settings_ui::ResourceStyle(L"ChatModelPickerLabelStyle"));
            title.Text(name);
            Put(content, title, 0);
            FontIcon check;
            check.Glyph(L"\uE73E");
            check.FontSize(13);
            check.Visibility(Visibility::Collapsed);
            Put(content, check, 0, 1);
            checks_.push_back(check);

            ListViewItem item;
            item.Content(content);
            Automation::AutomationProperties::SetName(item, name);
            ToolTipService::SetToolTip(item, box_value(name));
            list_.Items().Append(item);
        }
        list_.SelectedIndex(selected);
        RefreshSelection();
        missingModels_.Visibility(names.empty() ? Visibility::Visible : Visibility::Collapsed);
        list_.Visibility(names.empty() ? Visibility::Collapsed : Visibility::Visible);
    }

    void ChatModelPicker::RefreshSelection() {
        if (checked_ >= 0) checks_.at(checked_).Visibility(Visibility::Collapsed);
        checked_ = SelectedIndex();
        if (checked_ >= 0) checks_.at(checked_).Visibility(Visibility::Visible);
        auto selected = list_.SelectedItem().try_as<ListViewItem>();
        currentModel_.Text(selected ? Automation::AutomationProperties::GetName(selected) : hstring{});
        chipName_.Text(selected ? currentModel_.Text() : Localize(L"ChatPickModel"));
    }

    void ChatModelPicker::ShowModels(bool show) {
        models_.Visibility(show ? Visibility::Visible : Visibility::Collapsed);
        reasoning_.Visibility(show ? Visibility::Collapsed : Visibility::Visible);
        back_.Visibility(levels_.empty() ? Visibility::Collapsed : Visibility::Visible);
        if (show && list_.SelectedItem()) list_.ScrollIntoView(list_.SelectedItem());
    }

    void ChatModelPicker::SetReasoning(std::span<std::string const> choices, std::string_view defaultEffort,
                                    std::string_view overrideEffort) {
        levels_.assign(choices.begin(), choices.end());
        defaultEffort_ = defaultEffort;
        if (!levels_.empty() && defaultEffort_.empty()) levels_.emplace(levels_.begin());
        {
            auto restore = wil::scope_exit([&, previous = std::exchange(updating_, true)] { updating_ = previous; });
            efforts_.Maximum(levels_.size() > 1 ? static_cast<double>(levels_.size() - 1) : 1.);
            scale_.Visibility(levels_.size() > 1 ? Visibility::Visible : Visibility::Collapsed);
            ticks_.Children().Clear();
            for (std::size_t i = 0; i < levels_.size(); ++i) {
                Border tick;
                tick.Width(3); tick.Height(3); tick.CornerRadius({2,2,2,2}); tick.Opacity(.42);
                ticks_.Children().Append(tick);
            }
        }
        PresentReasoning(overrideEffort);
    }

    void ChatModelPicker::SetPalette(ChatPalette const& palette) {
        fill_.Background(palette.accent);
        rail_.Background(palette.track);
        rail_.BorderBrush(palette.border);
        knob_.Background(palette.surface);
        knob_.BorderBrush(palette.border);
        onAccent_ = palette.onAccent;
        muted_ = palette.muted;
        DrawScale();
    }

    void ChatModelPicker::DrawScale() {
        if (levels_.size() < 2 || !track_.ActualWidth()) return;
        auto w = track_.ActualWidth();
        constexpr double inset = 12;
        auto step = (w - 2 * inset) / static_cast<double>(levels_.size() - 1);
        auto center = inset + efforts_.Value() * step;
        auto value = efforts_.Value();
        fill_.Width(value == 0 ? 0. : value == efforts_.Maximum() ? w : center);
        Canvas::SetLeft(knob_, track_.Margin().Left + center - knob_.Width() / 2);
        for (unsigned i = 0; i < ticks_.Children().Size(); ++i) {
            auto tick = ticks_.Children().GetAt(i).as<Border>();
            tick.Background(i < value ? onAccent_ : muted_);
            tick.Visibility(i == value ? Visibility::Collapsed : Visibility::Visible);
            Canvas::SetLeft(tick, inset + step*i - 1.5);
            Canvas::SetTop(tick, 8.5);
        }
    }

    void ChatModelPicker::PresentReasoning(std::string_view overrideEffort) {
        auto restore = wil::scope_exit([&, previous = std::exchange(updating_, true)] { updating_ = previous; });
        if (!std::ranges::contains(levels_, overrideEffort)) overrideEffort = {};
        auto effective = overrideEffort.empty() ? std::string_view(defaultEffort_) : overrideEffort;
        auto label = EffortLabel(effective);
        auto match = std::ranges::find(levels_, effective);
        efforts_.Value(match == levels_.end() ? 0. : static_cast<double>(match - levels_.begin()));
        effortTitle_.Text(label);
        Automation::AutomationProperties::SetHelpText(efforts_, label);
        DrawScale();
        chipEffort_.Text(label);
        chipEffort_.Visibility(levels_.empty() ? Visibility::Collapsed : Visibility::Visible);
        auto summary = chipName_.Text();
        if (!levels_.empty()) summary = summary + L" · " + label;
        Automation::AutomationProperties::SetName(chip_, summary);
        ToolTipService::SetToolTip(chip_, box_value(summary));
    }
}
