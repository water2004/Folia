#pragma once
#include "../../JsonParsing.h"
#include "../../ModelAdapter.h"
#include "../../ModelValidation.h"
#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <string_view>

namespace fairy::providers {
namespace {
constexpr std::size_t MaxMessages = 512;
constexpr std::size_t MaxTextBytes = 4 * 1024 * 1024;
constexpr std::size_t MaxImageBytes = 20 * 1024 * 1024;
constexpr std::size_t MaxToolCount = 128;
constexpr std::size_t MaxToolArgumentBytes = 1024 * 1024;

[[noreturn]] void Invalid() {
    throw ProtocolError("Invalid Chat Completions protocol data.");
}
bool IsZeroIndex(Json const& v) {
    if (v.is_number_unsigned())
        return v.get<std::uint64_t>() == 0;
    if (v.is_number_integer())
        return v.get<std::int64_t>() == 0;
    return false;
}
std::string GetString(Json const& v, char const* key, bool allowEmpty = true) {
    if (!v.is_object() || !v.contains(key) || !v[key].is_string())
        Invalid();
    auto s = v[key].get<std::string>();
    if ((!allowEmpty && s.empty()) || s.size() > MaxTextBytes)
        Invalid();
    return s;
}
std::optional<std::uint64_t> ReadCount(Json const& v, char const* key) {
    if (!v.contains(key))
        return {};
    if (!v[key].is_number_unsigned() && !v[key].is_number_integer())
        Invalid();
    if (v[key].is_number_integer() && v[key].get<std::int64_t>() < 0)
        Invalid();
    auto n = v[key].get<std::uint64_t>();
    if (n > 1000000000ULL)
        Invalid();
    return n;
}
Usage ReadUsage(Json const& v) {
    if (!v.is_object())
        Invalid();
    Usage u;
    u.inputTokens = ReadCount(v, "prompt_tokens");
    u.outputTokens = ReadCount(v, "completion_tokens");
    u.totalTokens = ReadCount(v, "total_tokens");
    if (v.contains("prompt_tokens_details")) {
        auto const& d = v["prompt_tokens_details"];
        if (!d.is_object())
            Invalid();
        u.cachedInputTokens = ReadCount(d, "cached_tokens");
    }
    return u;
}
std::string Role(MessageRole role) {
    switch (role) {
    case MessageRole::System:
        return "system";
    case MessageRole::User:
        return "user";
    case MessageRole::Assistant:
        return "assistant";
    case MessageRole::Tool:
        return "tool";
    }
    Invalid();
}
bool ValidName(std::string const& s) {
    if (s.empty() || s.size() > 64)
        return false;
    return std::all_of(s.begin(), s.end(),
                       [](unsigned char c) { return std::isalnum(c) || c == '_' || c == '-'; });
}
bool ValidBase64(std::string const& s) {
    if (s.empty() || s.size() > MaxImageBytes || s.size() % 4 != 0)
        return false;
    bool pad = false;
    unsigned pads = 0;
    for (unsigned char c : s) {
        if (c == '=') {
            pad = true;
            if (++pads > 2)
                return false;
        } else if (!(std::isalnum(c) || c == '+' || c == '/'))
            return false;
        else if (pad)
            return false;
    }
    return true;
}
Json MessageContent(Message const& m) {
    if (m.content.empty())
        return nullptr;
    bool hasImage = false;
    for (auto const& p : m.content) {
        if (p.kind == ContentKind::Image)
            hasImage = true;
        else if (p.kind != ContentKind::Text || p.text.size() > MaxTextBytes)
            Invalid();
    }
    if (!hasImage && m.content.size() == 1 && m.content.front().kind == ContentKind::Text)
        return m.content.front().text;
    Json parts = Json::array();
    for (auto const& p : m.content) {
        if (p.kind == ContentKind::Text)
            parts.push_back({{"type", "text"}, {"text", p.text}});
        else {
            if (p.kind != ContentKind::Image ||
                (p.mediaType != "image/png" && p.mediaType != "image/jpeg" &&
                 p.mediaType != "image/webp" && p.mediaType != "image/gif") ||
                !ValidBase64(p.base64))
                Invalid();
            parts.push_back(
                {{"type", "image_url"},
                 {"image_url", {{"url", "data:" + p.mediaType + ";base64," + p.base64}}}});
        }
    }
    return parts;
}
Json MakeToolCalls(std::vector<ToolCall> const& calls) {
    Json result = Json::array();
    for (auto const& call : calls) {
        if (call.id.empty() || call.id.size() > 256 || !ValidName(call.name) ||
            call.arguments.size() > MaxToolArgumentBytes)
            Invalid();
        Json args;
        try {
            args = ParseJson(call.arguments);
        } catch (...) {
            Invalid();
        }
        if (!args.is_object())
            Invalid();
        result.push_back({{"id", call.id},
                          {"type", "function"},
                          {"function", {{"name", call.name}, {"arguments", call.arguments}}}});
    }
    return result;
}
std::string FinishName(FinishReason f) {
    switch (f) {
    case FinishReason::Completed:
        return "stop";
    case FinishReason::ToolCalls:
        return "tool_calls";
    case FinishReason::Truncated:
        return "length";
    case FinishReason::Refused:
        return "content_filter";
    }
    Invalid();
}
FinishReason ParseFinish(Json const& f) {
    if (!f.is_string())
        Invalid();
    auto s = f.get<std::string>();
    if (s == "stop")
        return FinishReason::Completed;
    if (s == "tool_calls" || s == "function_call")
        return FinishReason::ToolCalls;
    if (s == "length")
        return FinishReason::Truncated;
    if (s == "content_filter")
        return FinishReason::Refused;
    Invalid();
}
Json const& SingleChoice(Json const& response) {
    if (!response.is_object() || !response.contains("choices") || !response["choices"].is_array() ||
        response["choices"].size() != 1)
        Invalid();
    auto const& choice = response["choices"][0];
    if (!choice.is_object() || !choice.contains("index") || !IsZeroIndex(choice["index"]))
        Invalid();
    return choice;
}
ModelResult ParseFull(Json const& response) {
    auto const& choice = SingleChoice(response);
    if (!choice.contains("finish_reason") || !choice.contains("message") ||
        !choice["message"].is_object())
        Invalid();
    auto const& source = choice["message"];
    if (!source.contains("role") || !source["role"].is_string() || source["role"] != "assistant")
        Invalid();
    ModelResult result;
    result.message.role = MessageRole::Assistant;
    result.finish = ParseFinish(choice["finish_reason"]);
    if (source.contains("content") && !source["content"].is_null()) {
        if (!source["content"].is_string() ||
            source["content"].get<std::string>().size() > MaxTextBytes)
            Invalid();
        auto txt = source["content"].get<std::string>();
        if (!txt.empty())
            result.message.content.push_back({ContentKind::Text, std::move(txt), {}, {}});
    }
    if (source.contains("tool_calls")) {
        auto const& calls = source["tool_calls"];
        if (!calls.is_array() || calls.size() > MaxToolCount)
            Invalid();
        for (auto const& c : calls) {
            if (!c.is_object() || !c.contains("type") || c["type"] != "function" ||
                !c.contains("id") || !c["id"].is_string() || !c.contains("function") ||
                !c["function"].is_object())
                Invalid();
            auto id = c["id"].get<std::string>();
            auto name = GetString(c["function"], "name", false);
            auto argsText = GetString(c["function"], "arguments");
            if (id.empty() || id.size() > 256 || !ValidName(name) ||
                argsText.size() > MaxToolArgumentBytes)
                Invalid();
            Json args;
            try {
                args = ParseJson(argsText);
            } catch (...) {
                Invalid();
            }
            if (!args.is_object())
                Invalid();
            result.message.toolCalls.push_back(
                {std::move(id), std::move(name), std::move(argsText)});
        }
    }
    if (source.contains("refusal") && !source["refusal"].is_null()) {
        if (!source["refusal"].is_string() ||
            source["refusal"].get<std::string>().size() > MaxTextBytes)
            Invalid();
        if (!source["refusal"].get<std::string>().empty())
            result.finish = FinishReason::Refused;
    }
    Json replay = Json::object();
    if (source.contains("reasoning_content")) {
        if (!source["reasoning_content"].is_string() ||
            source["reasoning_content"].get<std::string>().size() > MaxTextBytes)
            Invalid();
        replay["reasoning_content"] = source["reasoning_content"];
        result.message.reasoning.text = source["reasoning_content"].get<std::string>();
        ValidateReasoningText(result.message.reasoning.text);
    }
    if (source.contains("tool_calls"))
        replay["tool_calls"] = source["tool_calls"];
    result.message.providerState["chat_message"] = std::move(replay);
    if (response.contains("id")) {
        if (!response["id"].is_string() || response["id"].get<std::string>().size() > 256)
            Invalid();
        result.responseId = response["id"].get<std::string>();
    }
    if (response.contains("usage") && !response["usage"].is_null())
        result.usage = ReadUsage(response["usage"]);
    if (!result.message.toolCalls.empty()) {
        auto callsOnly = result;
        callsOnly.finish = FinishReason::ToolCalls;
        ValidateResult(callsOnly);
    }
    if (result.finish == FinishReason::Truncated || result.finish == FinishReason::Refused)
        result.message.toolCalls.clear();
    ValidateResult(result);
    return result;
} // namespace
} // namespace
} // namespace fairy::providers
