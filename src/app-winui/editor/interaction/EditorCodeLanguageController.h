#pragma once

#include "editor/session/EditorSession.h"
#include "editor/rendering/EditorSurfaceRenderer.h"

namespace winrt::Folia
{
    class EditorCodeLanguageController
    {
    public:
        using ExecuteCommand = std::function<bool(folia::Command const&)>;

        void Attach(
            EditorSession& session,
            EditorSurfaceRenderer& renderer,
            winrt::Microsoft::UI::Xaml::Controls::SwapChainPanel const& editorSurface,
            winrt::Microsoft::UI::Xaml::Controls::Canvas const& overlay,
            winrt::Microsoft::UI::Xaml::Controls::AutoSuggestBox const& languageBox,
            ExecuteCommand executeCommand);
        void Detach();
        void Refresh();

    private:
        bool Commit();
        void Cancel();
        bool HasFocus() const;
        void PopulateLanguages(std::string_view query = {});

        EditorSession* session_ = nullptr;
        EditorSurfaceRenderer* renderer_ = nullptr;
        winrt::Microsoft::UI::Xaml::Controls::SwapChainPanel editorSurface_{ nullptr };
        winrt::Microsoft::UI::Xaml::Controls::Canvas overlay_{ nullptr };
        winrt::Microsoft::UI::Xaml::Controls::AutoSuggestBox languageBox_{ nullptr };
        ExecuteCommand executeCommand_;
        std::optional<folia::platform::editor::EditorVisualRawBlock> activeBlock_;
        winrt::event_token textChangedToken_{};
        winrt::event_token suggestionChosenToken_{};
        winrt::event_token querySubmittedToken_{};
        winrt::event_token gotFocusToken_{};
        winrt::event_token lostFocusToken_{};
        winrt::event_token keyDownToken_{};
        bool updating_ = false;
    };
}
