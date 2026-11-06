#include "pch.h"
#include "MainWindow.xaml.h"
#include "localization/Localization.h"

namespace winrt::Folia::implementation {
void MainWindow::ConfigureInlineCompletion(winrt::Folia::CompletionService::Transport transport) {
    inlineCompletion.reset();
    UpdateCompletionStatus(winrt::Folia::CompletionState::Idle);
    if (!appSettings.completion.enabled) return;
    inlineCompletion = std::make_shared<winrt::Folia::InlineCompletionController>(editorSession(), appSettings.completion,
        DispatcherQueue(), [this](auto const& command) {
            if (command.paste_range) keyboardController.PrepareSourceEdit(*command.paste_range);
            return ExecuteEditorCommand(command);
        },
        [this] { RenderEditorSurface(); }, [this](auto state, std::string const& text) { UpdateCompletionStatus(state, text); },
        std::move(transport));
    inlineCompletion->SetFocused(EditorSurface().FocusState() != Microsoft::UI::Xaml::FocusState::Unfocused && !settingsMode);
    textInputController.SetCompositionChanged([this](bool composing) {
        if (inlineCompletion) { inlineCompletion->CompositionChanged(composing); RenderEditorSurface(); }
    });
}
void MainWindow::UpdateCompletionStatus(winrt::Folia::CompletionState state, std::string const& error) {
    if (windowClosed) return;
    using winrt::Folia::CompletionState;
    using Microsoft::UI::Xaml::Visibility;
    LatexSuggestionHint().Text(Localize(state == CompletionState::Ready
        ? L"LatexSuggestionHintWithAI" : L"LatexSuggestionHint"));
    auto busy = state == CompletionState::Requesting;
    CompletionProgressRing().IsActive(busy);
    CompletionProgressRing().Visibility(busy ? Visibility::Visible : Visibility::Collapsed);
    CompletionStatusPanel().Visibility(state == CompletionState::Idle ? Visibility::Collapsed : Visibility::Visible);
    auto key = state == CompletionState::Requesting ? L"AIRequesting"
        : state == CompletionState::Ready ? L"AIReady"
        : state == CompletionState::Empty ? L"AINoSuggestion"
        : state == CompletionState::BudgetExhausted ? L"AIBudgetExhausted"
        : state == CompletionState::Failed ? L"AIRequestFailed" : L"AICompletion";
    auto text = Localize(key);
    if (!error.empty()) text = text + L": " + winrt::to_hstring(error);
    CompletionStatusText().Text(text);
    Microsoft::UI::Xaml::Controls::ToolTipService::SetToolTip(CompletionStatusPanel(),
        winrt::box_value(error.empty() ? text : winrt::to_hstring(error)));
}
bool MainWindow::HandleCompletionKey(folia::platform::editor::EditorKeyGesture gesture) {
    using namespace folia::platform::editor;
    if (inlineCompletion && inlineCompletion->Active()) inlineCompletion->Validate();
    auto owner = CompletionKeyOwner(gesture, {textInputController.IsComposing(), latexCompletionController.Active(),
        keyboardController.HasSnippetSession(), inlineCompletion && inlineCompletion->Active()});
    if (owner == EditorCompletionKeyOwner::Latex)
        return latexCompletionController.HandleKey(static_cast<Windows::System::VirtualKey>(gesture.key));
    if (owner == EditorCompletionKeyOwner::Snippet) {
        latexCompletionController.Cancel();
        if (gesture.key == EditorKey::Escape) { keyboardController.CancelSnippetSession(); RenderEditorSurface(); return true; }
        return keyboardController.Key(gesture);
    }
    if (owner == EditorCompletionKeyOwner::Inline) {
        if (gesture.key == EditorKey::Escape) { inlineCompletion->Cancel(); RenderEditorSurface(); return true; }
        // The command list's replacement range becomes stale after accepting
        // an AI edit. Dismiss that list, while keeping word acceptance available.
        latexCompletionController.Cancel();
        inlineCompletion->Accept(gesture.key == EditorKey::Right);
        return true; // A rejected completion must not turn into a tab insertion.
    }
    return false;
}
}
