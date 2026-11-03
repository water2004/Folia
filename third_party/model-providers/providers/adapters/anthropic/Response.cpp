#include "../Adapters.h"
#include "Internal.h"

namespace fairy::providers {
ModelResult ParseAnthropicResponse(Json const& response) {
    try {
        if (!response.is_object() || !IsString(response, "type") || response["type"] != "message" ||
            !IsString(response, "id") || !IsString(response, "role") ||
            response["role"] != "assistant" || !response.contains("content") ||
            !response["content"].is_array() || !IsString(response, "stop_reason") ||
            !response.contains("usage"))
            Bad();
        ModelResult r;
        r.responseId = response["id"].get<std::string>();
        SetUsage(r.usage, response["usage"]);
        r.message.providerState["anthropic_content"] = response["content"];
        for (auto const& b : response["content"]) {
            if (!IsString(b, "type"))
                Bad();
            auto type = b["type"].get<std::string>();
            if (type == "text") {
                auto s = String(b, "text");
                r.message.content.push_back({ContentKind::Text, s, {}, {}});
            } else if (type == "tool_use")
                ReadToolUse(r, b);
            else if (type == "thinking" && b.contains("thinking")) {
                if (!b["thinking"].is_string())
                    Bad();
                r.message.reasoning.text += b["thinking"].get<std::string>();
            } else if (type == "refusal")
                r.finish = FinishReason::Refused;
            // Unknown/future blocks remain opaque and are replayed byte-for-JSON.
        }
        auto stop = response["stop_reason"].get<std::string>();
        if (stop == "max_tokens")
            r.finish = FinishReason::Truncated;
        else if (stop == "refusal")
            r.finish = FinishReason::Refused;
        else if (stop == "tool_use" || !r.message.toolCalls.empty())
            r.finish = FinishReason::ToolCalls;
        else if (stop == "end_turn" || stop == "stop_sequence")
            r.finish = FinishReason::Completed;
        else
            Bad();
        if (r.finish == FinishReason::Truncated || r.finish == FinishReason::Refused)
            r.message.toolCalls.clear();
        ValidateResult(r);
        return r;
    } catch (ProtocolError const&) {
        throw;
    } catch (...) {
        Bad();
    }
}
} // namespace fairy::providers
