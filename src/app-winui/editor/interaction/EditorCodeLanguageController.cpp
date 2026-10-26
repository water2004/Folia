#include "pch.h"
#include "editor/interaction/EditorCodeLanguageController.h"
#include "editor/interaction/CodeLanguageChoices.h"

import folia.core.command;
import folia.core.utf;

namespace winrt::Folia
{
    void EditorCodeLanguageController::Attach(
        EditorSession& session,
        EditorSurfaceRenderer& renderer,
        winrt::Microsoft::UI::Xaml::Controls::SwapChainPanel const& editorSurface,
        winrt::Microsoft::UI::Xaml::Controls::Flyout const& flyout,
        winrt::Microsoft::UI::Xaml::Controls::AutoSuggestBox const& languageBox,
        ExecuteCommand executeCommand)
    {
        Detach();
        session_ = &session;
        renderer_ = &renderer;
        editorSurface_ = editorSurface;
        flyout_ = flyout;
        languageBox_ = languageBox;
        executeCommand_ = std::move(executeCommand);
        lifetime_ = std::make_shared<Lifetime>();
        PopulateLanguages();
        textChangedToken_ = languageBox_.TextChanged([this](auto const&, auto const& args)
        {
            using namespace winrt::Microsoft::UI::Xaml::Controls;
            if (!updating_ && args.Reason() == AutoSuggestionBoxTextChangeReason::UserInput)
                PopulateLanguages(winrt::to_string(languageBox_.Text()));
        });
        suggestionChosenToken_ = languageBox_.SuggestionChosen([this](auto const&, auto const& args)
        {
            // Arrow-key navigation previews a choice; only QuerySubmitted accepts it.
            languageBox_.Text(unbox_value<winrt::hstring>(args.SelectedItem()));
        });
        querySubmittedToken_ = languageBox_.QuerySubmitted([this](auto const&, auto const&)
        {
            Submit();
        });
        gotFocusToken_ = languageBox_.GotFocus([this](auto const&, auto const&)
        {
            PopulateLanguages();
            languageBox_.IsSuggestionListOpen(true);
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
        keyDownToken_ = languageBox_.PreviewKeyDown([this](auto const&, auto const& args)
        {
            if (args.Key() != winrt::Windows::System::VirtualKey::Escape) return;
            Cancel();
            Dismiss();
            if (editorSurface_) editorSurface_.Focus(
                winrt::Microsoft::UI::Xaml::FocusState::Programmatic);
            args.Handled(true);
        });
    }

    void EditorCodeLanguageController::Detach()
    {
        lifetime_.reset();
        if (languageBox_)
        {
            if (textChangedToken_.value) languageBox_.TextChanged(textChangedToken_);
            if (suggestionChosenToken_.value) languageBox_.SuggestionChosen(suggestionChosenToken_);
            if (querySubmittedToken_.value) languageBox_.QuerySubmitted(querySubmittedToken_);
            if (gotFocusToken_.value) languageBox_.GotFocus(gotFocusToken_);
            if (keyDownToken_.value) languageBox_.PreviewKeyDown(keyDownToken_);
            languageBox_.IsSuggestionListOpen(false);
            languageBox_.Visibility(winrt::Microsoft::UI::Xaml::Visibility::Collapsed);
        }
        if (flyout_) {
            if (closedToken_.value) flyout_.Closed(closedToken_);
            if (closingToken_.value) flyout_.Closing(closingToken_);
            flyout_.Hide();
        }
        activeBlock_.reset();
        executeCommand_ = {};
        languageBox_ = nullptr;
        flyout_ = nullptr;
        editorSurface_ = nullptr;
        renderer_ = nullptr;
        session_ = nullptr;
        textChangedToken_ = {};
        suggestionChosenToken_ = {};
        querySubmittedToken_ = {};
        gotFocusToken_ = {};
        closedToken_ = {};
        closingToken_ = {};
        closing_ = false;
        pendingOpen_.reset();
        keyDownToken_ = {};
    }

    void EditorCodeLanguageController::PopulateLanguages(std::string_view query)
    {
        if (!languageBox_) return;
        auto items = winrt::single_threaded_observable_vector<winrt::Windows::Foundation::IInspectable>();
        for (auto const& language : code_language::Filter(query))
            items.Append(box_value(winrt::to_hstring(language)));
        languageBox_.ItemsSource(items);
    }

    void EditorCodeLanguageController::Cancel()
    {
        if (!languageBox_) return;
        updating_ = true;
        languageBox_.IsSuggestionListOpen(false);
        languageBox_.Text(winrt::to_hstring(activeBlock_ ? activeBlock_->language.value_or("text") : "text"));
        updating_ = false;
    }

    void EditorCodeLanguageController::Dismiss()
    {
        if (languageBox_) languageBox_.IsSuggestionListOpen(false);
        if (flyout_ && !closing_) flyout_.Hide();
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
        auto theme = renderer_->Theme();
        languageBox_.FontFamily(Media::FontFamily(winrt::to_hstring(theme.typography.code.family)));
        languageBox_.FontSize(theme.typography.code.size);
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
        auto parsed = code_language::Parse(winrt::to_string(languageBox_.Text()));
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
