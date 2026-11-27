module folia.ai.agent.hosted_tools;
import std;
import folia.ai.agent;
import folia.ai.providers;

namespace folia::ai::agent {
    bool NeedsIsolatedWebRequest(Options const& options, std::span<Tool const> localTools) {
        return !localTools.empty() && MustIsolateWebTools(options.webSearch, options.pages, options.effort);
    }

    Tool PublicWebTool(WebTask task, Options const& options, Request const& send, std::vector<Source>& sources) {
        Tool tool;
        const bool read = task == WebTask::ReadPage;
        const auto field = read ? "url" : "query";
        tool.definition = {read ? "web_fetch" : "web_search", read ?
            "Read a public HTTP(S) URL supplied by the user or a search result." :
            "Search the public web using public search terms. Cite returned URLs.",
            {{"type", "object"}, {"properties", {{field, {{"type", "string"}, {"maxLength", read ? 2048 : 512}}}}},
             {"required", {field}}, {"additionalProperties", false}}};
        tool.target = [field](Json const& args) { return args.value(field, ""); };
        tool.invoke = [read, field, &options, &send, &sources](Json const& args, std::stop_token stop) -> Json {
            auto query = args.value(field, "");
            if (query.empty() || query.size() > 2048 || query.find_first_not_of(" \t\r\n") == query.npos)
                return {{"error", "Supply a short public query or URL"}};
            if (read && !query.starts_with("https://") && !query.starts_with("http://"))
                return {{"error", "Supply an HTTP(S) URL"}};
            ModelRequest wire;
            wire.model = options.model;
            wire.reasoning.effort = options.effort;
            wire.webSearch = !read;
            if (read) wire.pages = options.pages;
            wire.maxOutputTokens = (std::min)(options.outputTokens, 768u);
            wire.messages.push_back({MessageRole::System, {{ContentKind::Text,
                read ? "Use the hosted URL reading tool on the supplied URL. Summarize the page and cite its URL." :
                "Use hosted web search for the public query. Return a concise factual answer with cited sources."}}});
            wire.messages.push_back({MessageRole::User, {{ContentKind::Text, std::move(query)}}});
            auto result = send(wire, stop, {});
            if (stop.stop_requested()) throw std::runtime_error("Web request cancelled");
            if (result.citations.empty() && std::ranges::none_of(result.searches, [](auto const& item) { return !item.failed; }))
                return {{"error", "The provider returned no completed web operation or sources"}};

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
