#pragma once
#include "../SearchAction.h"
#include "../../JsonParsing.h"
#include "../../ModelAdapter.h"
#include "../../ModelValidation.h"
#include <algorithm>
#include <map>
#include <set>
#include <utility>

namespace folia::ai {
namespace {
[[noreturn]] void Bad() {
    throw ProtocolError("Invalid Anthropic protocol message.");
}
bool IsString(Json const& j, char const* key) {
    return j.is_object() && j.contains(key) && j[key].is_string();
}
std::string String(Json const& j, char const* key) {
    if (!IsString(j, key))
        Bad();
    return j[key].get<std::string>();
}
Json ParseObject(std::string const& s) {
    try {
        auto j = ParseJson(s);
        if (!j.is_object())
            Bad();
        return j;
    } catch (ProtocolError const&) {
        throw;
    } catch (...) {
        Bad();
    }
}
std::string RoleName(MessageRole r) {
    if (r == MessageRole::User)
        return "user";
    if (r == MessageRole::Assistant)
        return "assistant";
    Bad();
}
Json TextBlock(std::string const& text) {
    return Json{{"type", "text"}, {"text", text}};
}
bool IsHostedWebTool(Json const& block) {
    auto name = String(block, "name");
    return name == "web_search" || name == "deepseek_web_search" || name == "web_fetch" || name == "deepseek_read_url";
}
void AddHostedSearches(ModelResult& result, Json const& content) {
    for (auto const& b : content) {
        if (!IsString(b, "type")) continue;
        if (b["type"] == "server_tool_use" && IsHostedWebTool(b) && IsString(b, "id") && result.searches.size() < 32) {
            result.searches.push_back({String(b, "id"), SearchTarget(b, "input"), false, WebOperation(b, "input")});
        } else if ((b["type"] == "web_search_tool_result" || b["type"] == "web_fetch_tool_result") && IsString(b, "tool_use_id") &&
                   b.contains("content") && IsString(b["content"], "type")) {
            auto search = std::ranges::find(result.searches, String(b, "tool_use_id"), &HostedSearch::id);
            if (search == result.searches.end()) continue;
            auto const& payload = b["content"];
            search->failed = payload["type"] == "web_search_tool_result_error" || payload["type"] == "web_fetch_tool_result_error";
            if (!search->failed && b["type"] == "web_fetch_tool_result") AddPageSource(result, payload);
        }
    }
}
void AddCitations(ModelResult& result, Json const& block) {
    auto citations = block.find("citations");
    if (citations == block.end() || !citations->is_array()) return;
    for (auto const& citation : *citations)
        if (IsString(citation, "type") && citation["type"] == "web_search_result_location" &&
            IsString(citation, "url") && result.citations.size() < 64)
            result.citations.push_back({IsString(citation, "title") ? String(citation, "title") : "", String(citation, "url")});
}
void SetUsage(Usage& u, Json const& j) {
    if (!j.is_object())
        Bad();
    auto read = [&](char const* key) -> std::optional<std::uint64_t> {
        if (!j.contains(key))
            return std::nullopt;
        if (!j[key].is_number_unsigned())
            Bad();
        return j[key].get<std::uint64_t>();
    };
    u.inputTokens = read("input_tokens");
    u.outputTokens = read("output_tokens");
    if (u.inputTokens && u.outputTokens)
        u.totalTokens = *u.inputTokens + *u.outputTokens;
    if (j.contains("cache_read_input_tokens"))
        u.cachedInputTokens = read("cache_read_input_tokens");
    else if (j.contains("cache_creation_input_tokens"))
        u.cachedInputTokens = read("cache_creation_input_tokens");
}
void ReadToolUse(ModelResult& result, Json const& b) {
    if (!IsString(b, "id") || !IsString(b, "name") || !b.contains("input") ||
        !b["input"].is_object())
        Bad();
    ToolCall c{b["id"].get<std::string>(), b["name"].get<std::string>(), b["input"].dump()};
    if (c.id.empty() || c.name.empty())
        Bad();
    if (std::any_of(result.message.toolCalls.begin(), result.message.toolCalls.end(),
                    [&](auto const& x) { return x.id == c.id; }))
        Bad();
    result.message.toolCalls.push_back(std::move(c));
} // namespace
} // namespace
} // namespace folia::ai
