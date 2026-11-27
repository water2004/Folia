#include "agent/HostedSearch.h"

namespace folia::ai::agent {
    bool NeedsSeparateSearch(Options const& options, std::span<Tool const> localTools) {
        // The DeepSeek hosted format accepts client functions alongside search only in thinking mode.
        return !localTools.empty() && options.webSearch == WebSearchDialect::DeepSeek &&
               (options.effort.empty() || options.effort == "none");
    }

    Tool PublicSearchTool(Options const& options, Request const& send, std::vector<Source>& sources) {
        Tool tool;
        tool.definition = {"web_search", "Search the public web for a factual question. Supply public search terms only and cite returned source URLs.",
            {{"type", "object"}, {"properties", {{"query", {{"type", "string"}, {"maxLength", 512}}}}},
             {"required", {"query"}}, {"additionalProperties", false}}};
        tool.invoke = [&options, &send, &sources](Json const& args, std::stop_token stop) -> Json {
            auto query = args.value("query", "");
            if (query.empty() || query.size() > 2048 || query.find_first_not_of(" \t\r\n") == query.npos)
                return {{"error", "Supply a short public search query"}};
            ModelRequest wire;
            wire.model = options.model;
            wire.reasoning.effort = options.effort;
            wire.webSearch = true;
            wire.maxOutputTokens = (std::min)(options.outputTokens, 768u);
            wire.messages.push_back({MessageRole::System, {{ContentKind::Text,
                "Use hosted web search for the public query. Return a concise factual answer with cited sources."}}});
            wire.messages.push_back({MessageRole::User, {{ContentKind::Text, std::move(query)}}});
            auto result = send(wire, stop, {});
            if (stop.stop_requested()) throw std::runtime_error("Search cancelled");
            if (result.citations.empty() && std::ranges::none_of(result.searches, [](auto const& item) { return !item.failed; }))
                return {{"error", "The provider returned no completed web search or cited sources"}};

            Json cited = Json::array();
            std::size_t bytes = 0;
            for (auto const& citation : result.citations) {
                if (!citation.url.starts_with("https://") && !citation.url.starts_with("http://")) continue;
                if (cited.size() >= 12 || bytes + citation.url.size() + citation.title.size() > 4096) break;
                cited.push_back({{"title", citation.title}, {"url", citation.url}});
                bytes += citation.url.size() + citation.title.size();
                if (sources.size() < 32 && std::ranges::find(sources, citation.url, &Source::url) == sources.end())
                    sources.push_back({citation.title, citation.url});
            }
            return {{"text", PlainText(result.message)}, {"sources", std::move(cited)}};
        };
        return tool;
    }
}
