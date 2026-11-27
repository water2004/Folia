#include "pch.h"
#include "agent/ChatMotion.h"
#include "agent/ChatModelPicker.h"
#include "ui/ControlHelpers.h"
#include <winrt/Microsoft.UI.Xaml.Automation.h>
#include "agent/ChatPalette.h"
#include <wil/resource.h>
#include <winrt/Microsoft.UI.Xaml.Hosting.h>
#include <winrt/Windows.System.h>

namespace winrt::Folia::chat {
    using namespace ui;
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
                                 std::function<void()> configureModels) : selectEffort_(std::move(selectEffort)) {
        chip_ = LoadTemplate<Button>(L"ChatModelChipTemplate");
        chipName_ = chip_.FindName(L"ModelName").as<TextBlock>();
        chipEffort_ = chip_.FindName(L"ModelEffort").as<TextBlock>();
        auto panel = LoadTemplate<Grid>(L"ChatModelPickerTemplate");
        reasoning_ = panel.FindName(L"ReasoningPanel").as<StackPanel>();
        models_ = panel.FindName(L"ModelsPanel").as<StackPanel>();
        missingModels_ = panel.FindName(L"MissingModels").as<StackPanel>();
        list_ = panel.FindName(L"ModelList").as<ListView>();
        efforts_ = panel.FindName(L"Efforts").as<Slider>();
        scale_ = panel.FindName(L"Scale").as<Grid>();
        face_ = panel.FindName(L"ScaleFace").as<Grid>();
        artwork_.Attach(face_);
        effortTitle_ = panel.FindName(L"EffortTitle").as<TextBlock>();
        auto outline = panel.FindName(L"FocusOutline").as<Border>();
        efforts_.GotFocus([outline](auto const& sender, auto const&) {
            outline.Visibility(sender.template as<Slider>().FocusState() == FocusState::Keyboard ? Visibility::Visible : Visibility::Collapsed);
        });
        efforts_.LostFocus([outline](auto const&, auto const&) { outline.Visibility(Visibility::Collapsed); });
        trackSize_ = face_.SizeChanged(winrt::auto_revoke, [this](auto const&, auto const&) { DrawScale(); });
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
        flyout_.FlyoutPresenterStyle(ui::ResourceStyle(L"ChatModelPickerPresenterStyle"));
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
        effortChanged_ = efforts_.ValueChanged(winrt::auto_revoke, [this](auto const&, auto const&) { ChooseEffort(); });
        // An untouched slider has no selected stop. Its first stop can be
        // explicitly chosen even when the native numeric value is already zero.
        tap_ = Input::TappedEventHandler([this](auto const&, auto const&) { if (!selectedEffort_) ChooseEffort(); });
        key_ = Input::KeyEventHandler([this](auto const&, Input::KeyRoutedEventArgs const& args) {
            using enum Windows::System::VirtualKey;
            if (!selectedEffort_ && (args.Key() == Space || args.Key() == Enter || args.Key() == Home ||
                args.Key() == End || args.Key() == Left || args.Key() == Right ||
                args.Key() == Up || args.Key() == Down)) ChooseEffort();
        });
        efforts_.AddHandler(UIElement::TappedEvent(), box_value(tap_), true);
        efforts_.AddHandler(UIElement::KeyDownEvent(), box_value(key_), true);
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

    ChatModelPicker::~ChatModelPicker() {
        efforts_.RemoveHandler(UIElement::TappedEvent(), box_value(tap_));
        efforts_.RemoveHandler(UIElement::KeyDownEvent(), box_value(key_));
    }

    void ChatModelPicker::ChooseEffort() {
        if (updating_ || levels_.empty()) return;
        auto index = static_cast<std::size_t>(std::llround(efforts_.Value()));
        if (index >= levels_.size()) return;
        auto level = levels_[index];
        PresentReasoning(level, selectedEffort_.has_value());
        selectEffort_(std::move(level));
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
            title.Style(ui::ResourceStyle(L"ChatModelPickerLabelStyle"));
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
        const bool changed = show != (models_.Visibility() == Visibility::Visible);
        models_.Visibility(show ? Visibility::Visible : Visibility::Collapsed);
        reasoning_.Visibility(show ? Visibility::Collapsed : Visibility::Visible);
        back_.Visibility(levels_.empty() ? Visibility::Collapsed : Visibility::Visible);
        if (changed && flyout_.IsOpen()) Reveal(show ? models_ : reasoning_, show ? 8.f : -8.f, 0);
        if (show && list_.SelectedItem()) list_.ScrollIntoView(list_.SelectedItem());
    }

    void ChatModelPicker::SetReasoning(std::span<std::string const> choices, std::string_view effective) {
        auto restore = wil::scope_exit([&, previous = std::exchange(updating_, true)] { updating_ = previous; });
        levels_.assign(choices.begin(), choices.end());
        efforts_.Maximum(levels_.empty() ? 0. : static_cast<double>(levels_.size() - 1));
        scale_.Visibility(levels_.empty() ? Visibility::Collapsed : Visibility::Visible);
        PresentReasoning(effective);
    }

    void ChatModelPicker::SetPalette(ChatPalette const& palette) { artwork_.SetPalette(palette); }

    void ChatModelPicker::DrawScale(bool animate) {
        artwork_.Draw(face_, levels_.size(), selectedEffort_, animate);
    }

    void ChatModelPicker::PresentReasoning(std::string_view effective, bool animate) {
        auto restore = wil::scope_exit([&, previous = std::exchange(updating_, true)] { updating_ = previous; });
        auto match = std::ranges::find(levels_, effective);
        selectedEffort_.reset();
        if (match != levels_.end()) selectedEffort_ = static_cast<std::size_t>(match - levels_.begin());
        auto label = EffortLabel(selectedEffort_ ? effective : std::string_view{});
        efforts_.Value(static_cast<double>(selectedEffort_.value_or(0)));
        effortTitle_.Text(label);
        Automation::AutomationProperties::SetHelpText(efforts_, label);
        DrawScale(animate);
        chipEffort_.Text(label);
        chipEffort_.Visibility(levels_.empty() ? Visibility::Collapsed : Visibility::Visible);
        auto summary = chipName_.Text();
        if (!levels_.empty()) summary = summary + L" · " + label;
        Automation::AutomationProperties::SetName(chip_, summary);
        ToolTipService::SetToolTip(chip_, box_value(summary));
    }
}
