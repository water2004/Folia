#pragma once
#include "agent/ChatStore.h"
#include "agent/ChatReplySurface.h"

namespace winrt::Folia::chat {
    class ChatTranscript : public std::enable_shared_from_this<ChatTranscript> {
    public:
        static std::shared_ptr<ChatTranscript> Create(std::string, folia::ThemeProfile const&);
        Microsoft::UI::Xaml::Controls::Grid Element() const { return root_; }
        void SetTheme(folia::ThemeProfile const&);
        void SetInset(double);
        void Refresh(bool latest);
        void Begin();
        void Progress(folia::ai::agent::ProgressKind, hstring const&);
        void Finish();
        bool Empty() const { return page_.entries.empty(); }
        Entry const* LastReply() const;
    private:
        void Build();
        void Show(bool navigate);
        void Follow();
        void Latest();
        void Append(Entry const&);
        void UpdateNavigation();
        std::string id_;
        folia::ThemeProfile theme_;
        HistoryPage page_;
        std::vector<std::uintmax_t> boundaries_;
        std::vector<std::shared_ptr<ChatReplySurface>> surfaces_;
        Microsoft::UI::Xaml::Controls::Grid root_;
        Microsoft::UI::Xaml::Controls::ScrollViewer scroll_;
        Microsoft::UI::Xaml::Controls::StackPanel column_, messages_, live_, nav_;
        Microsoft::UI::Xaml::Controls::Button older_, newer_, latest_;
        Microsoft::UI::Xaml::Controls::TextBlock pageLabel_, working_, liveText_{nullptr}, thinking_{nullptr};
        Microsoft::UI::Xaml::Controls::StackPanel liveTool_{nullptr};
        Microsoft::UI::Xaml::FrameworkElement thinkingRow_{nullptr};
        bool following_ = true, posted_ = false, busy_ = false;
        double offset_ = 0;
    };
}
