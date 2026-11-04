#include "../Adapters.h"
#include "Internal.h"

namespace fairy::providers {
class AnthropicDecoder final : public IResponseDecoder {
    ModelResult result_;
    bool started_{}, messageStopped_{}, finished_{}, sawMessageDelta_{}, refusal_{};
    std::map<std::size_t, Json> blocks_;
    std::set<std::size_t> stopped_;
    std::set<std::size_t> seenStart_;
    std::string stopReason_;
    std::string reasoning_;
    void RequireStarted() const {
        if (!started_ || messageStopped_)
            Bad();
    }
    Json& Block(std::size_t i) {
        auto it = blocks_.find(i);
        if (it == blocks_.end() || stopped_.contains(i))
            Bad();
        return it->second;
    }
    void AppendText(std::size_t i, std::string const& s, char const* key) {
        Json& b = Block(i);
        if (!IsString(b, key))
            Bad();
        b[key] = b[key].get<std::string>() + s;
    }

  public:
    std::vector<StreamEvent> OnEvent(SseEvent const& ev) override {
        if (finished_)
            Bad();
        Json d;
        try {
            d = ParseJson(ev.data);
        } catch (...) {
            Bad();
        }
        if (!d.is_object())
            Bad();
        auto type = ev.event.empty()
                        ? (IsString(d, "type") ? d["type"].get<std::string>() : std::string{})
                        : ev.event;
        if (IsString(d, "type") && d["type"].get<std::string>() != type)
            Bad();
        std::vector<StreamEvent> out;
        if (type == "ping")
            return out;
        if (type == "error")
            Bad();
        if (type == "message_start") {
            if (started_ || !d.contains("message") || !d["message"].is_object())
                Bad();
            auto const& m = d["message"];
            if (!IsString(m, "id") || !IsString(m, "type") || m["type"] != "message" ||
                !IsString(m, "role") || m["role"] != "assistant")
                Bad();
            started_ = true;
            result_.responseId = m["id"].get<std::string>();
            if (m.contains("usage"))
                SetUsage(result_.usage, m["usage"]);
            return out;
        }
        RequireStarted();
        if (type == "content_block_start") {
            if (!d.contains("index") || !d["index"].is_number_unsigned() ||
                !d.contains("content_block") || !d["content_block"].is_object())
                Bad();
            auto i = d["index"].get<std::size_t>();
            if (!seenStart_.insert(i).second)
                Bad();
            blocks_.emplace(i, d["content_block"]);
            auto const& b = blocks_.at(i);
            if (!IsString(b, "type"))
                Bad();
            auto bt = b["type"].get<std::string>();
            if (bt == "text" && IsString(b, "text") && !b["text"].get<std::string>().empty())
                out.push_back({StreamEventKind::TextDelta, b["text"].get<std::string>()});
            if (bt == "tool_use") {
                if (!IsString(b, "id") || !IsString(b, "name"))
                    Bad();
                out.push_back({StreamEventKind::ToolCallDelta,
                               {},
                               b["id"].get<std::string>(),
                               b["name"].get<std::string>()});
            }
        } else if (type == "content_block_delta") {
            if (!d.contains("index") || !d["index"].is_number_unsigned() || !d.contains("delta") ||
                !d["delta"].is_object() || !IsString(d["delta"], "type"))
                Bad();
            auto i = d["index"].get<std::size_t>();
            auto const& delta = d["delta"];
            auto dt = delta["type"].get<std::string>();
            if (dt == "text_delta") {
                auto s = String(delta, "text");
                AppendText(i, s, "text");
                out.push_back({StreamEventKind::TextDelta, s});
            } else if (dt == "thinking_delta") {
                auto s = String(delta, "thinking");
                Json& b = Block(i);
                if (!b.contains("thinking"))
                    b["thinking"] = "";
                AppendText(i, s, "thinking");
                if (reasoning_.size() + s.size() > 1024u * 1024u)
                    Bad();
                ValidateReasoningText(s);
                reasoning_ += s;
                out.push_back({StreamEventKind::ReasoningDelta, s});
            } else if (dt == "signature_delta") {
                auto s = String(delta, "signature");
                Json& b = Block(i);
                if (!b.contains("signature"))
                    b["signature"] = "";
                AppendText(i, s, "signature");
            } else if (dt == "input_json_delta") {
                auto s = String(delta, "partial_json");
                Json& b = Block(i);
                if (!IsString(b, "type") || b["type"] != "tool_use" || !b.contains("input") ||
                    !b["input"].is_object())
                    Bad();
                if (!b.contains("_partial_json")) {
                    if (!b["input"].empty())
                        Bad();
                    b["_partial_json"] = "";
                }
                b["_partial_json"] = b["_partial_json"].get<std::string>() + s;
                out.push_back({StreamEventKind::ToolCallDelta, s, b.value("id", std::string{}),
                               b.value("name", std::string{})});
            } else
                Bad();
        } else if (type == "content_block_stop") {
            if (!d.contains("index") || !d["index"].is_number_unsigned())
                Bad();
            auto i = d["index"].get<std::size_t>();
            Json& b = Block(i);
            stopped_.insert(i);
            if (b["type"] == "refusal")
                refusal_ = true;
            if (b["type"] == "tool_use") {
                if (b.contains("_partial_json")) {
                    Json input;
                    try {
                        input = ParseJson(b["_partial_json"].get<std::string>());
                    } catch (...) {
                        Bad();
                    }
                    if (!input.is_object())
                        Bad();
                    b["input"] = std::move(input);
                    b.erase("_partial_json");
                }
                ReadToolUse(result_, b);
            }
        } else if (type == "message_delta") {
            if (sawMessageDelta_)
                Bad();
            sawMessageDelta_ = true;
            if (d.contains("delta")) {
                if (!d["delta"].is_object())
                    Bad();
                if (d["delta"].contains("stop_reason")) {
                    if (!d["delta"]["stop_reason"].is_string())
                        Bad();
                    stopReason_ = d["delta"]["stop_reason"].get<std::string>();
                }
            }
            if (d.contains("usage")) {
                auto previousIn = result_.usage.inputTokens;
                SetUsage(result_.usage, d["usage"]);
                if (previousIn && !result_.usage.inputTokens)
                    result_.usage.inputTokens = previousIn;
            }
            out.push_back({StreamEventKind::UsageUpdated, {}, {}, {}, result_.usage});
        } else if (type == "message_stop") {
            if (messageStopped_)
                Bad();
            messageStopped_ = true;
        } else
            Bad();
        return out;
    }
    bool Complete() const noexcept override { return messageStopped_; }
    ModelResult Finish() override {
        if (finished_ || !started_ || !sawMessageDelta_ || !messageStopped_ ||
            stopped_.size() != blocks_.size() || stopReason_.empty())
            Bad();
        finished_ = true;
        Json content = Json::array();
        for (auto const& [i, b] : blocks_) {
            (void)i;
            auto clean = b;
            clean.erase("_partial_json");
            content.push_back(std::move(clean));
        }
        result_.message.providerState["anthropic_content"] = content;
        result_.message.reasoning.text = reasoning_;
        for (auto const& b : content) {
            if (b["type"] == "text")
                result_.message.content.push_back(
                    {ContentKind::Text, b.value("text", std::string{}), {}, {}});
        }
        if (stopReason_ == "max_tokens")
            result_.finish = FinishReason::Truncated;
        else if (stopReason_ == "refusal" || refusal_)
            result_.finish = FinishReason::Refused;
        else if (stopReason_ == "tool_use" || !result_.message.toolCalls.empty())
            result_.finish = FinishReason::ToolCalls;
        else if (stopReason_ == "end_turn" || stopReason_ == "stop_sequence")
            result_.finish = FinishReason::Completed;
        else
            Bad();
        if (result_.finish == FinishReason::Truncated || result_.finish == FinishReason::Refused)
            result_.message.toolCalls.clear();
        ValidateResult(result_);
        return result_;
    }
};
std::unique_ptr<IResponseDecoder> CreateAnthropicDecoder() {
    return std::make_unique<AnthropicDecoder>();
}
} // namespace fairy::providers
