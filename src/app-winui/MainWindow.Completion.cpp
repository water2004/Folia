#include "pch.h"
#include "MainWindow.xaml.h"

namespace winrt::Folia::implementation {
void MainWindow::ConfigureInlineCompletion() {
    inlineCompletion.reset();
    if (!appSettings.completion.enabled) return;
    inlineCompletion = std::make_shared<winrt::Folia::InlineCompletionController>(editorSession(), appSettings.completion,
        DispatcherQueue(), [this](auto const& command) { return ExecuteEditorCommand(command); },
        [this] { RenderEditorSurface(); }, [this](std::string const& text) { SetStatus(winrt::to_hstring(text)); });
    inlineCompletion->SetFocused(EditorSurface().FocusState() != Microsoft::UI::Xaml::FocusState::Unfocused && !settingsMode);
    textInputController.SetCompositionChanged([this](bool composing) {
        if (inlineCompletion) { inlineCompletion->CompositionChanged(composing); RenderEditorSurface(); }
    });
}
bool MainWindow::HandleInlineCompletionKey(Windows::System::VirtualKey key) {
    using Windows::System::VirtualKey;
    if (!inlineCompletion || !inlineCompletion->Active() || textInputController.IsComposing()) return false;
    auto down = [](VirtualKey key) { return (static_cast<unsigned>(Microsoft::UI::Input::InputKeyboardSource::GetKeyStateForCurrentThread(key)) & 1) != 0; };
    if (down(VirtualKey::Menu) || down(VirtualKey::Shift)) return false;
    if (key == VirtualKey::Escape) { inlineCompletion->Cancel(); RenderEditorSurface(); return true; }
    if (key == VirtualKey::Tab && !down(VirtualKey::Control) && !keyboardController.HasSnippetSession()) return inlineCompletion->Accept(false);
    if (key == VirtualKey::Right && down(VirtualKey::Control)) return inlineCompletion->Accept(true);
    return false;
}
}
