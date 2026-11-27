#pragma once
#include "editor/rendering/EditorSurfaceRenderer.h"
#include "editor/session/EditorSession.h"

namespace winrt::Folia::chat {
    // A reply occupies its full document height in the native scroll stream. Only
    // the visible slice plus overscan needs a Direct2D composition surface.
    class ChatReplySurface : public std::enable_shared_from_this<ChatReplySurface> {
    public:
        static std::shared_ptr<ChatReplySurface> Create(std::string const&, folia::ThemeProfile const&);
        Microsoft::UI::Xaml::Controls::Canvas Element() const { return paper_; }
        void SetTheme(folia::ThemeProfile const&);
    private:
        void Draw(bool dirty);
        void BindInput();
        bool CopySelection();
        Microsoft::UI::Xaml::Controls::Canvas paper_;
        Microsoft::UI::Xaml::Controls::Grid slice_;
        EditorSession session_;
        std::unique_ptr<EditorSurfaceRenderer> renderer_;
        folia::ThemeProfile theme_;
        float top_ = 0, bottom_ = 0;
        std::optional<folia::TextPosition> anchor_;
        std::optional<std::uint32_t> pointer_;
        bool drawing_ = false;
    };
}
