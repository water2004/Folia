#pragma once
#include "agent/ChatStore.h"
#include "agent/ChatTurnView.h"

namespace winrt::Folia::chat {
    class ChatTranscript : public std::enable_shared_from_this<ChatTranscript> {
    public:
        static std::shared_ptr<ChatTranscript> Create(std::string, folia::ThemeProfile const&, MediaDisplaySettings const&);
        Microsoft::UI::Xaml::Controls::Grid Element() const { return root_; }
        void SetTheme(folia::ThemeProfile const&);
        void SetMediaDisplaySettings(MediaDisplaySettings const&);
        void SetInset(double);
        void Refresh(bool latest);
        void Begin();
        void Progress(folia::ai::agent::ProgressKind, hstring const&);
        void Finish();
        void Latest();
        auto Header() const { return header_; }
        void SetTitle(hstring const& text) { title_.Text(text); }
        using Documents = std::vector<folia::ai::agent::DocumentReference>;
        std::function<void(std::string const&, Documents const&)> openDocument;
        std::function<void()> retry;
        void AddReference(folia::ai::agent::DocumentReference ref) { if (liveTurn_) liveTurn_->AddReference(std::move(ref)); }
        bool Empty() const { return page_.entries.empty(); }
        bool EndsWithCompaction() const { return !page_.compactions.empty() && page_.compactions.back() == page_.entries.size(); }
        Entry const* LastReply() const;
        Entry const* LastQuestion() const;
    private:
        void Build();
        void Show(bool navigate);
        void Follow();
        void Append(Entry const&, bool latest);
        void UpdateNavigation();
        std::string id_;
        void RefreshAppearance();
        std::shared_ptr<ChatMarkdownAppearance> appearance_ = std::make_shared<ChatMarkdownAppearance>();
        HistoryPage page_;
        std::vector<std::uintmax_t> boundaries_;
        std::vector<std::shared_ptr<ChatTurnView>> turns_;
        std::shared_ptr<ChatTurnView> MakeTurn();
        Microsoft::UI::Xaml::Controls::Grid root_, header_;
        Microsoft::UI::Xaml::Controls::ScrollViewer scroll_;
        Microsoft::UI::Xaml::Controls::StackPanel column_, messages_, live_, nav_;
        Microsoft::UI::Xaml::Controls::Button older_, newer_, latest_, retryButton_{nullptr};
        Microsoft::UI::Xaml::Controls::TextBlock pageLabel_, working_, title_;
        std::shared_ptr<ChatTurnView> liveTurn_;
        bool finishing_ = false;
        bool following_ = true, posted_ = false, busy_ = false;
        double offset_ = 0;
    };
}
