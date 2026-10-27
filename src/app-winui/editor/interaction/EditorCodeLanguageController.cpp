#include "pch.h"
import std;
#include "editor/interaction/EditorCodeLanguageController.h"
#include "editor/interaction/CodeLanguageChoices.h"
#include "localization/Localization.h"

import folia.core.command;
import folia.markdown.utf;

namespace winrt::Folia
{
    void EditorCodeLanguageController::Attach(
        EditorSession& session,
        EditorSurfaceRenderer& renderer,
        winrt::Microsoft::UI::Xaml::Controls::SwapChainPanel const& editorSurface,
        winrt::Microsoft::UI::Xaml::Controls::Flyout const& flyout,
        winrt::Microsoft::UI::Xaml::Controls::TextBox const& languageBox,
        winrt::Microsoft::UI::Xaml::Controls::ListView const& languages,
        ExecuteCommand executeCommand)
    {
        Detach();
        session_ = &session;
        renderer_ = &renderer;
        editorSurface_ = editorSurface;
        flyout_ = flyout;
        languageBox_ = languageBox;
        languages_ = languages;
        languageBox_.PlaceholderText(Localize(L"SearchLanguage"));
        executeCommand_ = std::move(executeCommand);
        lifetime_ = std::make_shared<Lifetime>();
        PopulateLanguages();
        textChangedToken_ = languageBox_.TextChanged([this](auto const&, auto const&)
        {
            if (!updating_)
                PopulateLanguages(winrt::to_string(languageBox_.Text()));
        });
        itemClickToken_ = languages_.ItemClick([this](auto const&, auto const& args)
        {
            languages_.SelectedItem(args.ClickedItem());
            Submit();
        });
        closingToken_ = flyout_.Closing([this](auto const&, auto const&) { closing_ = true; });
        closedToken_ = flyout_.Closed([this](auto const&, auto const&) {
            Cancel(); activeBlock_.reset(); closing_ = false;
            auto pending = std::exchange(pendingOpen_, std::nullopt);
            if (pending && editorSurface_) {
                // Closed is raised inside native popup teardown. Reopening
                // belongs to the next dispatcher turn, after that teardown.
                auto lifetime = std::weak_ptr<Lifetime>{lifetime_};
                editorSurface_.DispatcherQueue().TryEnqueue([this, lifetime, owner = *pending] {
                    if (!lifetime.expired()) Open(owner);
                });
            }
        });
        keyDownToken_ = languageBox_.PreviewKeyDown([this](auto const&, auto const& args) { KeyDown(args); });
        listKeyDownToken_ = languages_.PreviewKeyDown([this](auto const&, auto const& args) { KeyDown(args); });
    }

    void EditorCodeLanguageController::Detach()
    {
        lifetime_.reset();
        if (languageBox_)
        {
            if (textChangedToken_.value) languageBox_.TextChanged(textChangedToken_);
            if (keyDownToken_.value) languageBox_.PreviewKeyDown(keyDownToken_);
            languageBox_.Visibility(winrt::Microsoft::UI::Xaml::Visibility::Collapsed);
        }
        if (languages_) {
            if (itemClickToken_.value) languages_.ItemClick(itemClickToken_);
            if (listKeyDownToken_.value) languages_.PreviewKeyDown(listKeyDownToken_);
        }
        if (flyout_) {
            if (closedToken_.value) flyout_.Closed(closedToken_);
            if (closingToken_.value) flyout_.Closing(closingToken_);
            flyout_.Hide();
        }
        activeBlock_.reset();
        executeCommand_ = {};
        languageBox_ = nullptr;
        languages_ = nullptr;
        flyout_ = nullptr;
        editorSurface_ = nullptr;
        renderer_ = nullptr;
        session_ = nullptr;
        textChangedToken_ = {};
        itemClickToken_ = {};
        listKeyDownToken_ = {};
        closedToken_ = {};
        closingToken_ = {};
        closing_ = false;
        pendingOpen_.reset();
        keyDownToken_ = {};
    }

    void EditorCodeLanguageController::PopulateLanguages(std::string_view query)
    {
        if (!languages_) return;
        auto choices = code_language::Filter(query);
        if (query.empty() && activeBlock_) {
            auto current = activeBlock_->language.value_or("text");
            if (std::find(choices.begin(), choices.end(), current) == choices.end()) choices.insert(choices.begin(), current);
        }
        if (choices.empty() && !query.empty())
            if (auto custom = code_language::Parse(query); custom && !custom->empty()) choices.push_back(*custom);
        auto items = winrt::single_threaded_observable_vector<winrt::Windows::Foundation::IInspectable>();
        for (auto const& language : choices)
            items.Append(box_value(winrt::to_hstring(language)));
        languages_.ItemsSource(items);
        int selected = items.Size() ? 0 : -1;
        if (query.empty() && activeBlock_) {
            auto current = activeBlock_->language.value_or("text");
            auto found = std::find(choices.begin(), choices.end(), current);
            if (found != choices.end()) selected = static_cast<int>(found - choices.begin());
        }
        languages_.SelectedIndex(selected);
        if (selected >= 0) languages_.ScrollIntoView(languages_.SelectedItem());
    }

    void EditorCodeLanguageController::Cancel()
    {
        if (!languageBox_) return;
        updating_ = true;
        languageBox_.Text(L"");
        updating_ = false;
        PopulateLanguages();
    }

    void EditorCodeLanguageController::Dismiss()
    {
        if (flyout_ && !closing_) flyout_.Hide();
    }

    void EditorCodeLanguageController::KeyDown(winrt::Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const& args)
    {
        using K = winrt::Windows::System::VirtualKey;
        if (args.Key() == K::Escape) {
            Cancel(); Dismiss();
            if (editorSurface_) editorSurface_.Focus(winrt::Microsoft::UI::Xaml::FocusState::Programmatic);
        } else if (args.Key() == K::Enter) {
            Submit();
        } else if (args.Key() == K::Down || args.Key() == K::Up) {
            auto count = static_cast<int>(languages_.Items().Size());
            if (count > 0) {
                auto next = std::clamp(languages_.SelectedIndex() + (args.Key() == K::Down ? 1 : -1), 0, count - 1);
                languages_.SelectedIndex(next);
                languages_.ScrollIntoView(languages_.SelectedItem());
            }
        } else return;
        args.Handled(true);
    }

    bool EditorCodeLanguageController::Open(folia::NodeId owner)
    {
        using namespace winrt::Microsoft::UI::Xaml;
        using namespace winrt::Microsoft::UI::Xaml::Controls;
        if (!session_ || !renderer_ || !editorSurface_ || !flyout_ || !editorSurface_.XamlRoot()) return false;
        auto control = renderer_->RawBlockFor(owner);
        if (!control || !control->languageEditable || session_->Selection().active.container_id != owner) return false;
        // WinUI cannot show a flyout during its native close transition. Honor
        // a new activation when Closed arrives, rather than adding a timer.
        if (closing_) { pendingOpen_ = owner; return true; }
        activeBlock_ = control;
        Cancel();
        languageBox_.PlaceholderText(Localize(L"SearchLanguage"));
        languageBox_.Visibility(Visibility::Visible);
        Primitives::FlyoutShowOptions options;
        options.Position(winrt::Windows::Foundation::Point{control->headerRect.left, control->headerRect.bottom});
        options.Placement(Primitives::FlyoutPlacementMode::BottomEdgeAlignedLeft);
        flyout_.ShowAt(editorSurface_, options);
        languageBox_.Focus(FocusState::Programmatic);
        return true;
    }

    void EditorCodeLanguageController::Refresh()
    {
        using namespace winrt::Microsoft::UI::Xaml;
        using namespace winrt::Microsoft::UI::Xaml::Controls;
        if (!session_ || !renderer_ || !flyout_ || !flyout_.IsOpen() || !activeBlock_) return;
        auto control = renderer_->RawBlockFor(activeBlock_->contentStart.container_id);
        // The native label scrolls in the same draw as its code block. A picker
        // is transient: close it when the document/anchor moves, never chase
        // scrolling with a separately composed textbox.
        if (!control || session_->Selection().active.container_id != activeBlock_->contentStart.container_id
            || control->headerRect.top != activeBlock_->headerRect.top
            || control->headerRect.left != activeBlock_->headerRect.left
            || control->headerRect.bottom != activeBlock_->headerRect.bottom) {
            Dismiss();
            return;
        }
        if (activeBlock_->language != control->language) {
            activeBlock_ = control;
            Cancel();
        }
    }

    bool EditorCodeLanguageController::Submit()
    {
        if (!Commit()) return false;
        Dismiss();
        if (editorSurface_) editorSurface_.Focus(winrt::Microsoft::UI::Xaml::FocusState::Programmatic);
        return true;
    }

    bool EditorCodeLanguageController::Commit()
    {
        if (updating_ || !session_ || !activeBlock_ || !languageBox_ || !executeCommand_) return false;
        auto value = languages_.SelectedItem() ? unbox_value<winrt::hstring>(languages_.SelectedItem()) : languageBox_.Text();
        auto parsed = code_language::Parse(winrt::to_string(value));
        if (!parsed) return false;
        auto const& language = *parsed;
        auto current = activeBlock_->language.value_or("text");
        if (session_->Selection().active.container_id != activeBlock_->contentStart.container_id) return false;
        if (language == current || (language.empty() && !activeBlock_->language)) return true;

        folia::Command command;
        command.kind = folia::CommandKind::SetCodeBlockLanguage;
        command.lang = language == "text" ? std::u32string{} : folia::utf8_to_cps(language);
        // Execution may synchronously refresh the renderer and replace activeBlock_.
        auto owner = activeBlock_->contentStart.container_id;
        if (!executeCommand_(command)) return false;
        if (activeBlock_ && activeBlock_->contentStart.container_id == owner)
            activeBlock_->language = language.empty() || language == "text"
                ? std::nullopt : std::optional<std::string>{language};
        return true;
    }
}
