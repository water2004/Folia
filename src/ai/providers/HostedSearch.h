#pragma once

#include "ModelTypes.h"

namespace folia::ai {
inline std::string_view SearchDialectId(WebSearchDialect dialect) {
    switch (dialect) {
    case WebSearchDialect::Standard: return "standard";
    case WebSearchDialect::DeepSeek: return "deepseek";
    case WebSearchDialect::OpenRouter: return "openrouter";
    }
    throw ProtocolError("Unknown web search format");
}
inline WebSearchDialect ParseSearchDialect(std::string_view value) {
    for (auto dialect : {WebSearchDialect::Standard, WebSearchDialect::DeepSeek, WebSearchDialect::OpenRouter})
        if (SearchDialectId(dialect) == value) return dialect;
    throw ProtocolError("Unknown web search format");
}
inline bool SupportsSearchDialect(ApiProtocol protocol, WebSearchDialect dialect) {
    switch (dialect) {
    case WebSearchDialect::Standard: return protocol == ApiProtocol::Responses || protocol == ApiProtocol::AnthropicMessages;
    case WebSearchDialect::DeepSeek: return true;
    case WebSearchDialect::OpenRouter: return protocol == ApiProtocol::ChatCompletions || protocol == ApiProtocol::Responses;
    }
    return false;
}
inline constexpr std::pair<PageAccess, std::string_view> pageFormats[]{
    {PageAccess::None, "none"}, {PageAccess::WithSearch, "with-search"}, {PageAccess::DeepSeek, "deepseek"},
    {PageAccess::Anthropic, "anthropic"}, {PageAccess::OpenRouter, "openrouter"}};
inline PageAccess ParsePageAccess(std::string_view id) {
    for (auto [value, name] : pageFormats) if (name == id) return value;
    throw ProtocolError("Unknown web page format");
}
inline bool SeparatePageTool(PageAccess mode) { return mode != PageAccess::None && mode != PageAccess::WithSearch; }
// DeepSeek permits hosted tools together with client functions only in thinking mode.
inline bool MustIsolateWebTools(std::optional<WebSearchDialect> search, PageAccess pages, std::string_view effort) {
    return (search == WebSearchDialect::DeepSeek || pages == PageAccess::DeepSeek) && (effort.empty() || effort == "none");
}
inline Json PageTool(ApiProtocol protocol, PageAccess mode) {
    if (mode == PageAccess::DeepSeek) {
        Json tool{{"type", "deepseek_read_url"}};
        if (protocol == ApiProtocol::AnthropicMessages) tool["name"] = "deepseek_read_url";
        return tool;
    }
    if (mode == PageAccess::Anthropic && protocol == ApiProtocol::AnthropicMessages)
        return {{"type", "web_fetch_20250910"}, {"name", "web_fetch"}, {"max_uses", 3}, {"citations", {{"enabled", true}}}};
    if (mode == PageAccess::OpenRouter && SupportsSearchDialect(protocol, WebSearchDialect::OpenRouter))
        return {{"type", "openrouter:web_fetch"}};
    throw ProtocolError("Web page retrieval is unavailable for this protocol");
}
inline Json HostedSearchTool(ApiProtocol protocol, WebSearchDialect dialect) {
    if (!SupportsSearchDialect(protocol, dialect)) throw ProtocolError("Web search format is unavailable for this protocol");
    if (dialect == WebSearchDialect::OpenRouter) return {{"type", "openrouter:web_search"}};
    if (dialect == WebSearchDialect::DeepSeek) {
        Json tool{{"type", "deepseek_web_search"}};
        if (protocol == ApiProtocol::AnthropicMessages) tool["name"] = "deepseek_web_search";
        return tool;
    }
    if (protocol == ApiProtocol::AnthropicMessages)
        return {{"type", "web_search_20250305"}, {"name", "web_search"}, {"max_uses", 3}};
    return {{"type", "web_search"}};
}
}
