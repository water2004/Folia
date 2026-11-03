#include "../Adapters.h"
#include "Internal.h"

namespace fairy::providers {
class ChatDecoder final : public IResponseDecoder {
    struct Call {
        std::string id, name, arguments;
        bool sawId{}, sawName{};
    };
    std::map<int, Call> calls_;
    ModelResult result_;
    bool sawChoice_{}, sawFinish_{}, sawDone_{}, failed_{};
    bool sawUsage_{};
    Json usageRaw_;
    FinishReason finish_{FinishReason::Completed};
    std::optional<std::string> responseId_;
    std::string reasoning_;
    std::size_t textBytes_{};
    void Fail() {
        failed_ = true;
        Invalid();
    }

  public:
    std::vector<StreamEvent> OnEvent(SseEvent const& event) override {
        if (failed_ || sawDone_)
            Fail();
        if (event.data == "[DONE]") {
            sawDone_ = true;
            return {};
        }
        Json data;
        try {
            data = ParseJson(event.data);
        } catch (...) {
            Fail();
        }
        std::vector<StreamEvent> out;
        if (data.is_string() && data.get<std::string>() == "[DONE]") {
            sawDone_ = true;
            return out;
        }
        if (!data.is_object())
            Fail();
        if (data.contains("id")) {
            if (!data["id"].is_string())
                Fail();
            auto id = data["id"].get<std::string>();
            if (id.size() > 256 || (responseId_ && *responseId_ != id))
                Fail();
            responseId_ = id;
        }
        bool usageThisEvent = data.contains("usage") && !data["usage"].is_null();
        if (usageThisEvent) {
            if (sawUsage_ && usageRaw_ != data["usage"])
                Fail();
            sawUsage_ = true;
            usageRaw_ = data["usage"];
            try {
                result_.usage = ReadUsage(data["usage"]);
            } catch (...) {
                Fail();
            }
            StreamEvent e;
            e.kind = StreamEventKind::UsageUpdated;
            e.usage = result_.usage;
            out.push_back(std::move(e));
        }
        if (!data.contains("choices") || !data["choices"].is_array())
            Fail();
        auto const& choices = data["choices"];
        if (choices.empty()) {
            if (!usageThisEvent)
                Fail();
            return out;
        } // Usage-only chunk.
        if (choices.size() != 1 || !choices[0].is_object() || !choices[0].contains("index") ||
            !IsZeroIndex(choices[0]["index"]))
            Fail();
        auto const& choice = choices[0];
        sawChoice_ = true;
        if (choice.contains("delta")) {
            auto const& delta = choice["delta"];
            if (!delta.is_object())
                Fail();
            if (delta.contains("role") &&
                (!delta["role"].is_string() || delta["role"] != "assistant"))
                Fail();
            for (auto const key : {"content", "reasoning_content"})
                if (delta.contains(key) && !delta[key].is_null()) {
                    if (!delta[key].is_string())
                        Fail();
                    auto s = delta[key].get<std::string>();
                    if (s.size() > MaxTextBytes)
                        Fail();
                    if (!s.empty()) {
                        StreamEvent e;
                        e.kind = key == std::string_view("content")
                                     ? StreamEventKind::TextDelta
                                     : StreamEventKind::ReasoningDelta;
                        e.text = s;
                        out.push_back(std::move(e));
                        if (key == std::string_view("content")) {
                            if (textBytes_ + s.size() > MaxTextBytes)
                                Fail();
                            textBytes_ += s.size();
                            result_.message.content.push_back(
                                {ContentKind::Text, std::move(s), {}, {}});
                        } else {
                            if (reasoning_.size() + s.size() > MaxTextBytes)
                                Fail();
                            try {
                                ValidateReasoningText(s);
                            } catch (...) {
                                Fail();
                            }
                            if (s.size() > 1024u * 1024u - reasoning_.size())
                                Fail();
                            reasoning_ += s;
                        }
                    }
                }
            if (delta.contains("tool_calls")) {
                auto const& list = delta["tool_calls"];
                if (!list.is_array() || list.size() > MaxToolCount)
                    Fail();
                for (auto const& d : list) {
                    if (!d.is_object() || !d.contains("index") || !d["index"].is_number_integer())
                        Fail();
                    auto const& indexJson = d["index"];
                    std::uint64_t rawIndex{};
                    if (indexJson.is_number_unsigned())
                        rawIndex = indexJson.get<std::uint64_t>();
                    else {
                        auto signedIndex = indexJson.get<std::int64_t>();
                        if (signedIndex < 0)
                            Fail();
                        rawIndex = static_cast<std::uint64_t>(signedIndex);
                    }
                    if (rawIndex >= MaxToolCount)
                        Fail();
                    int index = static_cast<int>(rawIndex);
                    auto& c = calls_[index];
                    if (d.contains("id")) {
                        if (!d["id"].is_string())
                            Fail();
                        auto v = d["id"].get<std::string>();
                        if (v.size() > 256 || (c.sawId && c.id != v))
                            Fail();
                        c.id = v;
                        c.sawId = true;
                    }
                    if (d.contains("function")) {
                        auto const& fn = d["function"];
                        if (!fn.is_object())
                            Fail();
                        if (fn.contains("name")) {
                            if (!fn["name"].is_string())
                                Fail();
                            auto v = fn["name"].get<std::string>();
                            if (v.size() > 64)
                                Fail();
                            c.name += v;
                            c.sawName = true;
                        }
                        if (fn.contains("arguments")) {
                            if (!fn["arguments"].is_string())
                                Fail();
                            auto v = fn["arguments"].get<std::string>();
                            if (c.arguments.size() + v.size() > MaxToolArgumentBytes)
                                Fail();
                            c.arguments += v;
                        }
                    }
                    if (d.contains("type") && (!d["type"].is_string() || d["type"] != "function"))
                        Fail();
                    StreamEvent e;
                    e.kind = StreamEventKind::ToolCallDelta;
                    e.callId = d.value("id", std::string{});
                    if (d.contains("function") && d["function"].is_object()) {
                        if (d["function"].contains("name"))
                            e.name = d["function"]["name"].get<std::string>();
                        if (d["function"].contains("arguments"))
                            e.text = d["function"]["arguments"].get<std::string>();
                    }
                    out.push_back(std::move(e));
                }
            }
        }
        if (choice.contains("finish_reason") && !choice["finish_reason"].is_null()) {
            if (sawFinish_)
                Fail();
            finish_ = ParseFinish(choice["finish_reason"]);
            sawFinish_ = true;
        }
        return out;
    }
    ModelResult Finish() override {
        if (failed_ || !sawDone_ || !sawFinish_ || !sawChoice_)
            Invalid();
        if (responseId_)
            result_.responseId = *responseId_;
        for (auto const& [index, c] : calls_) {
            (void)index;
            if (!c.sawId || !c.sawName || !ValidName(c.name) || c.arguments.empty())
                Invalid();
            Json args;
            try {
                args = ParseJson(c.arguments);
            } catch (...) {
                Invalid();
            }
            if (!args.is_object())
                Invalid();
            result_.message.toolCalls.push_back({c.id, c.name, c.arguments});
        }
        result_.finish = finish_;
        result_.message.reasoning.text = reasoning_;
        if (!result_.message.providerState.is_object())
            result_.message.providerState = Json::object();
        // Replay text deltas exactly as a string while keeping provider-specific reasoning.
        if (!result_.message.providerState.contains("chat_message"))
            result_.message.providerState["chat_message"] = Json::object();
        if (!reasoning_.empty())
            result_.message.providerState["chat_message"]["reasoning_content"] = reasoning_;
        if (!calls_.empty()) {
            auto& rawCalls = result_.message.providerState["chat_message"]["tool_calls"];
            rawCalls = Json::array();
            for (auto const& [index, call] : calls_) {
                (void)index;
                rawCalls.push_back(
                    {{"id", call.id},
                     {"type", "function"},
                     {"function", {{"name", call.name}, {"arguments", call.arguments}}}});
            }
        }
        if (!result_.message.toolCalls.empty()) {
            auto callsOnly = result_;
            callsOnly.finish = FinishReason::ToolCalls;
            ValidateResult(callsOnly);
        }
        if (result_.finish == FinishReason::Truncated || result_.finish == FinishReason::Refused)
            result_.message.toolCalls.clear();
        ValidateResult(result_);
        return result_;
    }
};
std::unique_ptr<IResponseDecoder> CreateChatCompletionsDecoder() {
    return std::make_unique<ChatDecoder>();
}
} // namespace fairy::providers
