#pragma once

#include "providers/ModelTypes.h"
import std;

namespace folia::ai::agent {
    struct Source {
        std::string title;
        std::string url;
    };

    struct ToolActivity {
        std::string name;
        std::string target;
        bool failed = false;
        std::string detail;
        bool pending = false;
    };

    struct StepEnd {
        std::size_t text = 0, reasoning = 0, tools = 0;
        enum class Notice { None, Compaction, Retry };
        Notice notice = Notice::None;
        unsigned attempt = 0;
    };
    struct DocumentReference {
        std::string tabKey, path, token;
        bool operator==(DocumentReference const&) const = default;
    };
    struct ReasoningSummary {
        std::size_t offset;
        std::string text;
    };
    struct Entry {
        MessageRole role;
        std::string markdown;
        std::vector<Source> sources;
        std::string reasoning;
        std::vector<ToolActivity> activities;
        Usage usage;
        std::string modelId;
        std::vector<StepEnd> steps;
        std::vector<std::string> images;
        bool interrupted = false;
        std::string error;
        bool outputLimit = false;
        std::vector<DocumentReference> documents;
        std::vector<ReasoningSummary> reasoningSummaries;
    };

    struct History {
        std::vector<Entry> entries;
        std::string summary;
        std::size_t summarized = 0;
        std::size_t retired = 0;
    };

    struct Tool {
        ToolDefinition definition;
        std::function<Json(Json const&, std::stop_token)> invoke;
        std::function<std::string(Json const&)> target;
    };

    struct Options {
        std::string model;
        std::string effort;
        std::optional<WebSearchDialect> webSearch;
        std::size_t inputBytes = 96'000;
        std::uint32_t outputTokens = 4'096;
        std::function<ContentPart(std::string const&)> loadImage;
    };

    enum class ProgressKind { TextDelta, TextSnapshot, ReasoningDelta, ReasoningSnapshot, Reading, Compacting, InputUsage, StepStarted, ToolResult, Retrying, HostedSearch, SummaryDelta, ReasoningSummary };
    using Progress = std::function<void(ProgressKind, std::string_view)>;
    using Request = std::function<ModelResult(ModelRequest const&, std::stop_token, Progress const&)>;

    // The caller owns persistence and access decisions. Protocol replay and raw
    // tool results live only within a run, and are discarded at its boundary.
    Entry Run(History&, Options const&, std::span<Tool const>, Request const&, Progress const&, std::stop_token, Entry* partial = nullptr);
}
