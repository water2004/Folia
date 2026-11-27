#pragma once

#include "agent/Agent.h"
#include "editor/session/EditorSourceLocation.h"

namespace winrt::Folia::chat {
    struct TabSnapshot {
        std::string text;
        std::vector<std::pair<unsigned, std::string>> outline;
    };
    struct OpenTab {
        std::string title;
        std::uint64_t documentId{};
        std::function<TabSnapshot(std::stop_token)> read;
        folia::ai::agent::DocumentReference document;
    };

    struct ReferenceTarget { const folia::ai::agent::DocumentReference& document; EditorSourceLocation location; };
    std::optional<ReferenceTarget> ResolveDocumentLink(std::string_view, std::span<const folia::ai::agent::DocumentReference>);
    using OnReference = std::function<void(folia::ai::agent::DocumentReference const&)>;
    std::vector<std::string> FindWorkspaceMentions(std::filesystem::path const&, std::string const& prefix, std::stop_token);
    std::vector<folia::ai::agent::Tool> MakeTools(
        std::optional<std::vector<OpenTab>>, std::filesystem::path const& folder, OnReference onReference = {});
}
