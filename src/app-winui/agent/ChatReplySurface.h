#pragma once
#include "editor/rendering/EditorSurfaceRenderer.h"
#include "editor/session/EditorSession.h"

namespace winrt::Folia::chat {
    struct ChatMarkdownAppearance {
        void SetTheme(folia::ThemeProfile const&);
        folia::ThemeProfile theme;
        MediaDisplaySettings media;
        TextLayoutOptions textLayout;
        std::uint64_t themeRevision = 0;
    };

    // A reply occupies its full document height in the native scroll stream. Only
    // the visible slice plus overscan needs a Direct2D composition surface.
    class ChatReplySurface : public std::enable_shared_from_this<ChatReplySurface> {
    public:
        static std::shared_ptr<ChatReplySurface> Create(std::string const&, std::shared_ptr<ChatMarkdownAppearance const>);
        Microsoft::UI::Xaml::Controls::Canvas Element() const { return paper_; }
        void AppendMarkdown(std::string_view);
        void Seal();
        std::function<void(std::string const&)> openDocument;
        void RefreshAppearance() { Draw(true); }
    private:
        void Draw(bool dirty);
        void BindInput();
        bool CopySelection();
        struct HoverHit { bool text = false; std::string link; };
        HoverHit ContentAt(Windows::Foundation::Point point) const;
        void UpdateCursor();
        std::optional<Windows::Foundation::Point> pointerInRoot_;
        std::optional<Microsoft::UI::Input::InputSystemCursorShape> cursorShape_;
        std::string pressedLink_;
        Microsoft::UI::Xaml::Controls::Canvas paper_;
        Microsoft::UI::Xaml::Controls::Grid slice_;
        EditorSession session_;
        std::optional<folia::MarkdownStream> stream_;
        std::string markdown_;
        std::unique_ptr<EditorSurfaceRenderer> renderer_;
        std::shared_ptr<ChatMarkdownAppearance const> appearance_;
        std::uint64_t appliedTheme_ = 0;
        float top_ = 0, bottom_ = 0;
        std::optional<folia::TextPosition> anchor_;
        std::optional<std::uint32_t> pointer_;
        bool drawing_ = false, needsRedraw_ = true;
    };
}
