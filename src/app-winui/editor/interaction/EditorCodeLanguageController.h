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
            winrt::Microsoft::UI::Xaml::Controls::Flyout const& flyout,
            winrt::Microsoft::UI::Xaml::Controls::TextBox const& languageBox,
            winrt::Microsoft::UI::Xaml::Controls::ListView const& languages,
            ExecuteCommand executeCommand);
        void Detach();
        void Refresh();
        bool Open(folia::NodeId owner);
        bool Submit();

    private:
        bool Commit();
        void Cancel();
        void Dismiss();
        void PopulateLanguages(std::string_view query = {});
        void KeyDown(winrt::Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const& args);

        EditorSession* session_ = nullptr;
        EditorSurfaceRenderer* renderer_ = nullptr;
        winrt::Microsoft::UI::Xaml::Controls::SwapChainPanel editorSurface_{ nullptr };
        winrt::Microsoft::UI::Xaml::Controls::Flyout flyout_{ nullptr };
        winrt::Microsoft::UI::Xaml::Controls::TextBox languageBox_{ nullptr };
        winrt::Microsoft::UI::Xaml::Controls::ListView languages_{ nullptr };
        ExecuteCommand executeCommand_;
        std::optional<folia::platform::editor::EditorVisualRawBlock> activeBlock_;
        winrt::event_token textChangedToken_{};
        winrt::event_token itemClickToken_{};
        winrt::event_token listKeyDownToken_{};
        winrt::event_token closedToken_{};
        winrt::event_token closingToken_{};
        winrt::event_token keyDownToken_{};
        bool updating_ = false;
        bool closing_ = false;
        std::optional<folia::NodeId> pendingOpen_;
        struct Lifetime {};
        std::shared_ptr<Lifetime> lifetime_;
    };
}
