#pragma once

#include "agent/Agent.h"

namespace winrt::Folia::chat {
    struct TabSnapshot {
        std::string text;
        std::vector<std::pair<unsigned, std::string>> outline;
    };
    struct OpenTab {
        std::string title;
        std::uint64_t documentId{};
        std::function<TabSnapshot(std::stop_token)> read;
    };

    std::vector<std::string> FindWorkspaceMentions(std::filesystem::path const&, std::string const& prefix, std::stop_token);
    std::vector<folia::ai::agent::Tool> MakeTools(
        std::optional<std::vector<OpenTab>>, std::filesystem::path const& folder);
}
