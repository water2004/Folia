#pragma once
#include "agent/ChatStore.h"
#include "agent/ChatActivityView.h"
#include "agent/ChatReplySurface.h"

namespace winrt::Folia::chat {
    // Adjacent activity shares one disclosure. An assistant text passage ends it.
    class ChatTurnView {
    public:
        using Documents = std::vector<folia::ai::agent::DocumentReference>;
        using OpenDocument = std::function<void(std::string const&, Documents const&)>;
        ChatTurnView(folia::ThemeProfile const&, OpenDocument);
        auto Element() const { return root_; }
        void SetTheme(folia::ThemeProfile const&);
        void AddReference(folia::ai::agent::DocumentReference ref) { documents_->push_back(std::move(ref)); }
        void Progress(folia::ai::agent::ProgressKind, hstring const&);
        void Finish(Entry const&);
    private:
        ChatActivityView& Activity();
        void Text(std::string_view, bool initial = false);
        void Replay(Entry const&);
        void SealText();
        void CloseActivity();
        Microsoft::UI::Xaml::Controls::StackPanel root_;
        folia::ThemeProfile theme_;
        OpenDocument open_;
        std::shared_ptr<Documents> documents_ = std::make_shared<Documents>();
        std::vector<std::shared_ptr<ChatReplySurface>> surfaces_;
        std::vector<std::shared_ptr<ChatActivityView>> activities_;
        std::shared_ptr<ChatReplySurface> surface_;
        std::shared_ptr<ChatActivityView> activity_, lastReason_;
        std::map<std::string, std::pair<std::shared_ptr<ChatActivityView>, std::size_t>> searches_;
        std::string markdown_, stepReason_;
        std::size_t stepStart_ = 0, reasonStart_ = 0;
        bool inReason_ = false, received_ = false;
    };
}
