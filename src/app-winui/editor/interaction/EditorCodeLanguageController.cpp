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
        winrt::Microsoft::UI::Xaml::Controls::Canvas const& overlay,
        winrt::Microsoft::UI::Xaml::Controls::AutoSuggestBox const& languageBox,
        ExecuteCommand executeCommand)
    {
        Detach();
        session_ = &session;
        renderer_ = &renderer;
        editorSurface_ = editorSurface;
        overlay_ = overlay;
        languageBox_ = languageBox;
        executeCommand_ = std::move(executeCommand);
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
            if (!Commit()) return;
            languageBox_.IsSuggestionListOpen(false);
            if (editorSurface_) editorSurface_.Focus(
                winrt::Microsoft::UI::Xaml::FocusState::Programmatic);
        });
        gotFocusToken_ = languageBox_.GotFocus([this](auto const&, auto const&)
        {
            PopulateLanguages();
            languageBox_.IsSuggestionListOpen(true);
        });
        lostFocusToken_ = languageBox_.LostFocus([this](auto const&, auto const&)
        {
            // Moving between the textbox and popup is not leaving the control.
            if (!HasFocus() && !languageBox_.IsSuggestionListOpen()) Cancel();
        });
        keyDownToken_ = languageBox_.PreviewKeyDown([this](auto const&, auto const& args)
        {
            if (args.Key() != winrt::Windows::System::VirtualKey::Escape) return;
            Cancel();
            if (editorSurface_) editorSurface_.Focus(
                winrt::Microsoft::UI::Xaml::FocusState::Programmatic);
            args.Handled(true);
        });
    }

    void EditorCodeLanguageController::Detach()
    {
        if (languageBox_)
        {
            if (textChangedToken_.value) languageBox_.TextChanged(textChangedToken_);
            if (suggestionChosenToken_.value) languageBox_.SuggestionChosen(suggestionChosenToken_);
            if (querySubmittedToken_.value) languageBox_.QuerySubmitted(querySubmittedToken_);
            if (gotFocusToken_.value) languageBox_.GotFocus(gotFocusToken_);
            if (lostFocusToken_.value) languageBox_.LostFocus(lostFocusToken_);
            if (keyDownToken_.value) languageBox_.PreviewKeyDown(keyDownToken_);
            languageBox_.IsSuggestionListOpen(false);
            languageBox_.Visibility(winrt::Microsoft::UI::Xaml::Visibility::Collapsed);
        }
        if (overlay_) overlay_.IsHitTestVisible(false);
        activeBlock_.reset();
        executeCommand_ = {};
        languageBox_ = nullptr;
        overlay_ = nullptr;
        editorSurface_ = nullptr;
        renderer_ = nullptr;
        session_ = nullptr;
        textChangedToken_ = {};
        suggestionChosenToken_ = {};
        querySubmittedToken_ = {};
        gotFocusToken_ = {};
        lostFocusToken_ = {};
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

    bool EditorCodeLanguageController::HasFocus() const
    {
        using namespace winrt::Microsoft::UI::Xaml;
        if (!languageBox_) return false;
        if (languageBox_.FocusState() != FocusState::Unfocused) return true;
        auto root = languageBox_.XamlRoot();
        if (!root) return false;
        auto focused = Input::FocusManager::GetFocusedElement(root).try_as<DependencyObject>();
        while (focused)
        {
            if (focused == languageBox_) return true;
            focused = Media::VisualTreeHelper::GetParent(focused);
        }
        return false;
    }

    void EditorCodeLanguageController::Refresh()
    {
        using namespace winrt::Microsoft::UI::Xaml;
        using namespace winrt::Microsoft::UI::Xaml::Controls;
        if (!session_ || !renderer_ || !editorSurface_ || !overlay_ || !languageBox_) return;
        auto control = renderer_->RawBlockFor(session_->Selection().active.container_id);
        if (!control
            || control->kind != folia::platform::editor::EditorRawBlockKind::Code
            || !control->languageEditable)
        {
            Cancel();
            activeBlock_.reset();
            languageBox_.Visibility(Visibility::Collapsed);
            overlay_.IsHitTestVisible(false);
            return;
        }

        auto sameOwner = activeBlock_
            && activeBlock_->contentStart.container_id == control->contentStart.container_id;
        auto languageChanged = sameOwner && activeBlock_->language != control->language;
        activeBlock_ = control;
        if (!sameOwner || languageChanged || (!HasFocus() && !languageBox_.IsSuggestionListOpen()))
            Cancel();

        auto transform = editorSurface_.TransformToVisual(overlay_);
        auto topLeft = transform.TransformPoint({control->headerRect.left, control->headerRect.top});
        auto width = (std::max)(0.0, (std::min)(190.0,
            static_cast<double>(control->headerRect.right - control->headerRect.left - 16.0f)));
        languageBox_.Width(width);
        Canvas::SetLeft(languageBox_, static_cast<double>(topLeft.X) + 8.0);
        Canvas::SetTop(languageBox_, static_cast<double>(topLeft.Y) + 2.0);
        languageBox_.Visibility(Visibility::Visible);
        overlay_.IsHitTestVisible(true);
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
