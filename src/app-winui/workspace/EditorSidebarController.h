#pragma once

#include "editor/session/EditorSession.h"
#include "editor/rendering/EditorSurfaceRenderer.h"
#include "editor/interaction/EditorTextInputController.h"

namespace winrt::Folia
{
    struct EditorSidebarController
    {
        using Render = std::function<void()>;

        void Attach(
            EditorSession& session,
            EditorSurfaceRenderer& renderer,
            EditorTextInputController& textInput,
            winrt::Microsoft::UI::Xaml::Controls::ListView const& outline,
            Render render);
        void Detach();
        void Refresh();
        void SyncViewport();
        void SelectOutline(winrt::Windows::Foundation::IInspectable const& selectedItem);

    private:
        void RefreshOutline();

        EditorSession* session_ = nullptr;
        EditorSurfaceRenderer* renderer_ = nullptr;
        EditorTextInputController* textInput_ = nullptr;
        winrt::Microsoft::UI::Xaml::Controls::ListView outline_{ nullptr };
        winrt::Windows::Foundation::Collections::IObservableVector<
            winrt::Windows::Foundation::IInspectable> outlineItems_{ nullptr };
        Render render_;
        std::vector<folia::TextPosition> outlinePositions_;
        std::vector<std::uint8_t> outlineLevels_;
        std::optional<std::uint64_t> outlineContentKey_;
        winrt::Microsoft::UI::Xaml::Controls::ListViewBase::ContainerContentChanging_revoker containerChanging_;
        bool updatingOutline_ = false;
    };
}
