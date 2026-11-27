#pragma once

#include "agent/Agent.h"

namespace winrt::Folia::chat {
    using folia::ai::agent::Entry;
    using folia::ai::agent::History;

    struct Conversation {
        std::string id, title, model, effort;
        std::filesystem::path folder;
        bool openTabs = false;
        bool search = false;
        bool operator==(Conversation const&) const = default;
    };

    std::vector<Conversation> Conversations();
    Conversation NewConversation(std::string title);
    Conversation LoadConversation(std::string const& id);
    void SaveConversationOptions(Conversation&);
    Conversation RenameConversation(std::string const& id, std::string title, bool automatic = false);
    void DeleteConversation(std::string const& id);
    struct HistoryPage {
        std::vector<Entry> entries;
        std::uintmax_t before = 0;
    };
    HistoryPage ReadHistoryPage(std::string const&, std::optional<std::uintmax_t> before = {});
    std::filesystem::path AttachmentPath(std::string const& chat, std::string const& id);
    std::string StoreImage(std::string const& chat, std::span<std::uint8_t const> png);
    folia::ai::ContentPart LoadImage(std::string const& chat, std::string const& id);
    History LoadHistory(std::string const& id);
    void AppendEntry(std::string const& id, Entry const&, History const* checkpoint = nullptr);
}
