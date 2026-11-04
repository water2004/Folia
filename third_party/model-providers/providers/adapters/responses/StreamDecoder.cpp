#include "../Adapters.h"
#include "Internal.h"

namespace fairy::providers {
class Decoder final : public IResponseDecoder {
    std::set<std::uint64_t> sequences_;
    std::unordered_map<int, Json> added_;
    std::unordered_map<int, Json> done_;
    std::map<std::pair<int, int>, std::string> textDeltas_;
    std::map<std::pair<int, int>, std::string> reasoningDeltas_;
    std::set<std::pair<int, int>> reasoningDone_;
    std::map<std::pair<int, int>, std::string> reasoningTextDeltas_;
    std::set<std::pair<int, int>> reasoningTextDone_;
    std::set<std::pair<int, int>> reasoningDeltaDelivered_;
    std::set<std::pair<int, int>> reasoningTextDeltaDelivered_;
    bool reasoningSummaryStreamed_{};
    bool reasoningTextStreamed_{};
    std::unordered_map<int, std::string> argumentDeltas_;
    std::set<std::pair<int, int>> textDone_;
    std::unordered_set<int> argumentsDone_;
    Json finalResponse_;
    bool terminal_{};
    bool failed_{};

    void CheckSnapshot(Json const& response) {
        if (!response.contains("output") || !response["output"].is_array())
            Bad("Responses terminal output is invalid.");
        for (auto const& [index, item] : done_) {
            if (index < 0 || static_cast<std::size_t>(index) >= response["output"].size() ||
                response["output"][index] != item)
                Bad("Responses output item conflicts with terminal snapshot.");
        }
        for (auto const& [index, item] : added_) {
            if (index < 0 || static_cast<std::size_t>(index) >= response["output"].size())
                Bad("Responses output item conflicts with terminal snapshot.");
            auto const& canonical = response["output"][index];
            if (!canonical.is_object())
                Bad("Responses output item conflicts with terminal snapshot.");
            if (canonical.value("id", std::string{}) != item.value("id", std::string{}) ||
                canonical.value("type", std::string{}) != item.value("type", std::string{}))
                Bad("Responses output item conflicts with terminal snapshot.");
            if (String(item, "type") == "function_call" &&
                (String(canonical, "call_id") != String(item, "call_id") ||
                 String(canonical, "name") != String(item, "name")))
                Bad("Responses function identity conflicts with terminal snapshot.");
        }
        for (auto const& [position, text] : textDeltas_) {
            auto const [index, contentIndex] = position;
            if (index < 0 || contentIndex < 0 ||
                static_cast<std::size_t>(index) >= response["output"].size())
                Bad("Responses text conflicts with terminal snapshot.");
            auto const& item = response["output"][index];
            if (!item.is_object() || !item.contains("content") || !item["content"].is_array() ||
                static_cast<std::size_t>(contentIndex) >= item["content"].size())
                Bad("Responses text conflicts with terminal snapshot.");
            auto const& part = item["content"][contentIndex];
            if (String(part, "type") != "output_text" || String(part, "text") != text)
                Bad("Responses text conflicts with terminal snapshot.");
        }
        for (auto const& [position, text] : reasoningDeltas_) {
            auto const [index, summaryIndex] = position;
            if (index < 0 || summaryIndex < 0 ||
                static_cast<std::size_t>(index) >= response["output"].size())
                Bad("Responses reasoning summary conflicts with terminal snapshot.");
            auto const& item = response["output"][index];
            if (String(item, "type") != "reasoning" || !item.contains("summary") ||
                !item["summary"].is_array() ||
                static_cast<std::size_t>(summaryIndex) >= item["summary"].size())
                Bad("Responses reasoning summary conflicts with terminal snapshot.");
            auto const& part = item["summary"][summaryIndex];
            if (String(part, "type") != "summary_text" || String(part, "text") != text)
                Bad("Responses reasoning summary conflicts with terminal snapshot.");
        }
        for (auto const& [position, text] : reasoningTextDeltas_) {
            auto const [index, contentIndex] = position;
            if (index < 0 || contentIndex < 0 ||
                static_cast<std::size_t>(index) >= response["output"].size())
                Bad("Responses reasoning text conflicts with terminal snapshot.");
            auto const& item = response["output"][index];
            if (String(item, "type") != "reasoning" || !item.contains("content") ||
                !item["content"].is_array() ||
                static_cast<std::size_t>(contentIndex) >= item["content"].size())
                Bad("Responses reasoning text conflicts with terminal snapshot.");
            auto const& part = item["content"][contentIndex];
            if (String(part, "type") != "reasoning_text" || String(part, "text") != text)
                Bad("Responses reasoning text conflicts with terminal snapshot.");
        }
        for (auto const& [index, args] : argumentDeltas_) {
            if (index < 0 || static_cast<std::size_t>(index) >= response["output"].size() ||
                String(response["output"][index], "type") != "function_call" ||
                String(response["output"][index], "arguments") != args)
                Bad("Responses function arguments conflict with terminal snapshot.");
        }
    }
    void HandleOutputItemAdded(Json const& data) {
        auto rawIndex = Unsigned(data, "output_index");
        if (rawIndex > static_cast<std::uint64_t>(std::numeric_limits<int>::max()))
            Bad("Responses output index is out of range.");
        auto idx = static_cast<int>(rawIndex);
        if (!data.contains("item") || !data["item"].is_object() || added_.contains(idx))
            Bad("Responses output item event is invalid.",
                added_.contains(idx) ? ProtocolFailure::DuplicateStreamEvent
                                     : ProtocolFailure::InvalidStreamEvent);
        added_[idx] = data["item"];
    }

    void HandleOutputItemDone(Json const& data) {
        auto rawIndex = Unsigned(data, "output_index");
        if (rawIndex > static_cast<std::uint64_t>(std::numeric_limits<int>::max()))
            Bad("Responses output index is out of range.");
        auto idx = static_cast<int>(rawIndex);
        if (!data.contains("item") || !data["item"].is_object() || done_.contains(idx))
            Bad("Responses output item event is invalid.",
                done_.contains(idx) ? ProtocolFailure::DuplicateStreamEvent
                                    : ProtocolFailure::InvalidStreamEvent);
        auto added = added_.find(idx);
        if (added != added_.end() &&
            (String(added->second, "id") != String(data["item"], "id") ||
             String(added->second, "type") != String(data["item"], "type")))
            Bad("Responses output item lifecycle conflicts.",
                ProtocolFailure::InvalidStreamIdentity);
        done_[idx] = data["item"];
    }

    void HandleOutputTextDelta(Json const& data, std::vector<StreamEvent>& out) {
        auto rawOutput = Unsigned(data, "output_index");
        auto rawContent = Unsigned(data, "content_index");
        if (rawOutput > static_cast<std::uint64_t>(std::numeric_limits<int>::max()) ||
            rawContent > static_cast<std::uint64_t>(std::numeric_limits<int>::max()))
            Bad("Responses text index is out of range.");
        auto const idx = static_cast<int>(rawOutput);
        auto const contentIdx = static_cast<int>(rawContent);
        auto const position = std::make_pair(idx, contentIdx);
        if (textDone_.contains(position) || done_.contains(idx))
            Bad("Responses text delta arrived after completion.", ProtocolFailure::LateStreamEvent);
        auto delta = RequiredString(data, "delta");
        textDeltas_[position] += delta;
        StreamEvent e;
        e.kind = StreamEventKind::TextDelta;
        e.text = delta;
        out.push_back(std::move(e));
    }

    void HandleOutputTextDone(Json const& data) {
        auto rawOutput = Unsigned(data, "output_index");
        auto rawContent = Unsigned(data, "content_index");
        if (rawOutput > static_cast<std::uint64_t>(std::numeric_limits<int>::max()) ||
            rawContent > static_cast<std::uint64_t>(std::numeric_limits<int>::max()))
            Bad("Responses text index is out of range.");
        auto const position =
            std::make_pair(static_cast<int>(rawOutput), static_cast<int>(rawContent));
        if (done_.contains(position.first))
            Bad("Responses text completion arrived after item completion.",
                ProtocolFailure::LateStreamEvent);
        auto full = RequiredString(data, "text");
        if (!textDone_.insert(position).second)
            Bad("Responses text completion is duplicated.", ProtocolFailure::DuplicateStreamEvent);
        if (textDeltas_.contains(position) && textDeltas_[position] != full)
            Bad("Responses text fragments conflict.", ProtocolFailure::ConflictingStreamFragment);
        textDeltas_[position] = full;
    }

    void HandleReasoningTextDelta(Json const& data, ReasoningKind kind,
                                  std::vector<StreamEvent>& out) {
        auto rawOutput = Unsigned(data, "output_index");
        auto const indexKey = kind == ReasoningKind::Summary ? "summary_index" : "content_index";
        auto rawSubIndex = Unsigned(data, indexKey);
        if (rawOutput > static_cast<std::uint64_t>(std::numeric_limits<int>::max()) ||
            rawSubIndex > static_cast<std::uint64_t>(std::numeric_limits<int>::max()))
            Bad("Responses reasoning text index is out of range.");
        auto const idx = static_cast<int>(rawOutput);
        auto const subIndex = static_cast<int>(rawSubIndex);
        auto itemId = RequiredString(data, "item_id");
        auto added = added_.find(idx);
        auto const position = std::make_pair(idx, subIndex);
        auto& deltas = kind == ReasoningKind::Summary ? reasoningDeltas_ : reasoningTextDeltas_;
        auto const& completed =
            kind == ReasoningKind::Summary ? reasoningDone_ : reasoningTextDone_;
        if (completed.contains(position) || done_.contains(idx))
            Bad("Responses reasoning text arrived after completion.",
                ProtocolFailure::LateStreamEvent);
        if (added == added_.end() || String(added->second, "type") != "reasoning" ||
            String(added->second, "id") != itemId)
            Bad("Responses reasoning item identity is invalid.",
                ProtocolFailure::InvalidStreamIdentity);
        auto delta = RequiredString(data, "delta");
        if (deltas[position].size() + delta.size() > 1024u * 1024u)
            Bad("Responses reasoning text exceeds limits.", ProtocolFailure::StreamLimit);
        ValidateReasoningText(delta);
        deltas[position] += delta;
        if (!delta.empty()) {
            auto& delivered = kind == ReasoningKind::Summary ? reasoningDeltaDelivered_
                                                             : reasoningTextDeltaDelivered_;
            delivered.insert(position);
            if (kind == ReasoningKind::Summary)
                reasoningSummaryStreamed_ = true;
            else
                reasoningTextStreamed_ = true;
            StreamEvent event{StreamEventKind::ReasoningDelta, std::move(delta)};
            event.reasoningKind = kind;
            out.push_back(std::move(event));
        }
    }

    void HandleReasoningTextDone(Json const& data, ReasoningKind kind,
                                 std::vector<StreamEvent>& out) {
        auto rawOutput = Unsigned(data, "output_index");
        auto const indexKey = kind == ReasoningKind::Summary ? "summary_index" : "content_index";
        auto rawSubIndex = Unsigned(data, indexKey);
        if (rawOutput > static_cast<std::uint64_t>(std::numeric_limits<int>::max()) ||
            rawSubIndex > static_cast<std::uint64_t>(std::numeric_limits<int>::max()))
            Bad("Responses reasoning text index is out of range.");
        auto const idx = static_cast<int>(rawOutput);
        auto const subIndex = static_cast<int>(rawSubIndex);
        auto itemId = RequiredString(data, "item_id");
        auto added = added_.find(idx);
        if (done_.contains(idx))
            Bad("Responses reasoning text completion arrived after item completion.",
                ProtocolFailure::LateStreamEvent);
        if (added == added_.end() || String(added->second, "type") != "reasoning" ||
            String(added->second, "id") != itemId)
            Bad("Responses reasoning item identity is invalid.",
                ProtocolFailure::InvalidStreamIdentity);
        auto const position = std::make_pair(idx, subIndex);
        auto full = RequiredString(data, "text");
        ValidateReasoningText(full);
        auto& deltas = kind == ReasoningKind::Summary ? reasoningDeltas_ : reasoningTextDeltas_;
        auto& completed = kind == ReasoningKind::Summary ? reasoningDone_ : reasoningTextDone_;
        if (completed.contains(position))
            Bad("Responses reasoning text completion is duplicated.",
                ProtocolFailure::DuplicateStreamEvent);
        if (deltas.contains(position) && deltas[position] != full)
            Bad("Responses reasoning text fragments conflict.",
                ProtocolFailure::ConflictingStreamFragment);
        completed.insert(position);
        deltas[position] = std::move(full);
        auto const& delivered = kind == ReasoningKind::Summary ? reasoningDeltaDelivered_
                                                               : reasoningTextDeltaDelivered_;
        if (!deltas[position].empty() && !delivered.contains(position)) {
            if (kind == ReasoningKind::Summary)
                reasoningSummaryStreamed_ = true;
            else
                reasoningTextStreamed_ = true;
            StreamEvent event{StreamEventKind::ReasoningDelta, deltas[position]};
            event.reasoningKind = kind;
            out.push_back(std::move(event));
        }
    }

    void HandleFunctionArgumentsDelta(Json const& data, std::vector<StreamEvent>& out) {
        auto rawIndex = Unsigned(data, "output_index");
        if (rawIndex > static_cast<std::uint64_t>(std::numeric_limits<int>::max()))
            Bad("Responses output index is out of range.");
        auto const idx = static_cast<int>(rawIndex);
        auto itemId = RequiredString(data, "item_id");
        auto added = added_.find(idx);
        if (argumentsDone_.contains(idx) || done_.contains(idx))
            Bad("Responses function delta arrived after completion.",
                ProtocolFailure::LateStreamEvent);
        if (added == added_.end() || String(added->second, "type") != "function_call" ||
            String(added->second, "id") != itemId)
            Bad("Responses function call item identity is invalid.",
                ProtocolFailure::InvalidStreamIdentity);
        auto callId = String(added->second, "call_id");
        auto name = String(added->second, "name");
        if (callId.empty() || name.empty())
            Bad("Responses function call identity is incomplete.",
                ProtocolFailure::InvalidStreamIdentity);
        if (data.contains("call_id") && RequiredString(data, "call_id") != callId)
            Bad("Responses function call id conflicts.", ProtocolFailure::InvalidStreamIdentity);
        if (data.contains("name") && RequiredString(data, "name") != name)
            Bad("Responses function name conflicts.", ProtocolFailure::InvalidStreamIdentity);
        auto delta = RequiredString(data, "delta");
        argumentDeltas_[idx] += delta;
        StreamEvent e;
        e.kind = StreamEventKind::ToolCallDelta;
        e.callId = callId;
        e.name = name;
        e.text = delta;
        out.push_back(std::move(e));
    }

    void HandleFunctionArgumentsDone(Json const& data) {
        auto rawIndex = Unsigned(data, "output_index");
        if (rawIndex > static_cast<std::uint64_t>(std::numeric_limits<int>::max()))
            Bad("Responses output index is out of range.");
        auto const idx = static_cast<int>(rawIndex);
        if (done_.contains(idx))
            Bad("Responses function arguments completed after item completion.",
                ProtocolFailure::LateStreamEvent);
        auto itemId = RequiredString(data, "item_id");
        auto added = added_.find(idx);
        if (added == added_.end() || String(added->second, "type") != "function_call" ||
            String(added->second, "id") != itemId)
            Bad("Responses function call item identity is invalid.",
                ProtocolFailure::InvalidStreamIdentity);
        auto full = RequiredString(data, "arguments");
        // Validate JSON only after the terminal response status is known; an incomplete
        // response may legitimately end with a partial function argument string.
        if (!argumentsDone_.insert(idx).second)
            Bad("Responses function arguments completion is duplicated.",
                ProtocolFailure::DuplicateStreamEvent);
        if (argumentDeltas_.contains(idx) && argumentDeltas_[idx] != full)
            Bad("Responses function fragments conflict.",
                ProtocolFailure::ConflictingStreamFragment);
        argumentDeltas_[idx] = full;
    }

    void HandleTerminalEvent(Json const& data, std::string const& type,
                             std::vector<StreamEvent>& out) {
        terminal_ = true;
        if (type == "response.failed" || type == "error") {
            failed_ = true;
            Bad("Responses stream failed.", ProtocolFailure::ProviderReportedFailure);
        }
        if (!data.contains("response") || !data["response"].is_object())
            Bad("Responses terminal event is malformed.", ProtocolFailure::InvalidTerminal);
        finalResponse_ = data["response"];
        bool truncated = type == "response.incomplete";
        if (truncated != (String(finalResponse_, "status") == "incomplete"))
            Bad("Responses terminal event conflicts with response status.",
                ProtocolFailure::InvalidTerminal);
        try {
            CheckSnapshot(finalResponse_);
        } catch (ProtocolError const& error) {
            throw ProtocolError(error.what(), ProtocolFailure::ConflictingSnapshot);
        }
        ModelResult result;
        try {
            result = ParseTerminal(finalResponse_, truncated);
        } catch (ProtocolError const& error) {
            if (error.Failure() == ProtocolFailure::ProviderReportedFailure)
                throw;
            throw ProtocolError(error.what(), ProtocolFailure::InvalidTerminal);
        }
        if (!reasoningTextStreamed_ && !result.message.reasoning.text.empty()) {
            StreamEvent event{StreamEventKind::ReasoningDelta, result.message.reasoning.text};
            event.reasoningKind = ReasoningKind::ProviderText;
            out.push_back(std::move(event));
            reasoningTextStreamed_ = true;
        }
        if (!reasoningSummaryStreamed_ && !result.message.reasoning.summary.empty()) {
            StreamEvent event{StreamEventKind::ReasoningDelta, result.message.reasoning.summary};
            event.reasoningKind = ReasoningKind::Summary;
            out.push_back(std::move(event));
            reasoningSummaryStreamed_ = true;
        }
        StreamEvent e;
        e.kind = StreamEventKind::UsageUpdated;
        e.usage = result.usage;
        out.push_back(std::move(e));
        // Keep canonical terminal result for Finish().
        finalResponse_["__fairy_finish_truncated"] = truncated;
    }
    std::vector<StreamEvent> OnEventImpl(SseEvent const& event) {
        if (terminal_)
            Bad("Responses stream has events after terminal event.",
                ProtocolFailure::TrailingStreamEvent);
        Json data = Json::parse(event.data, nullptr, false);
        if (data.is_discarded() || !data.is_object())
            Bad("Responses stream event is malformed.", ProtocolFailure::InvalidStreamJson);
        auto type = String(data, "type");
        if (type.empty())
            type = event.event;
        if (data.contains("sequence_number")) {
            if (!data["sequence_number"].is_number_integer())
                Bad("Responses stream sequence is invalid.", ProtocolFailure::InvalidStreamField);
            auto seq = Unsigned(data, "sequence_number");
            if (!sequences_.insert(seq).second)
                Bad("Responses stream has duplicate sequence number.",
                    ProtocolFailure::DuplicateStreamEvent);
        }
        std::vector<StreamEvent> out;
        if (type == "response.output_item.added" || type == "output_item.added")
            HandleOutputItemAdded(data);
        else if (type == "response.output_item.done" || type == "output_item.done")
            HandleOutputItemDone(data);
        else if (type == "response.output_text.delta" || type == "output_text.delta")
            HandleOutputTextDelta(data, out);
        else if (type == "response.output_text.done" || type == "output_text.done")
            HandleOutputTextDone(data);
        else if (type == "response.reasoning_summary_text.delta" ||
                 type == "reasoning_summary_text.delta")
            HandleReasoningTextDelta(data, ReasoningKind::Summary, out);
        else if (type == "response.reasoning_summary_text.done" ||
                 type == "reasoning_summary_text.done")
            HandleReasoningTextDone(data, ReasoningKind::Summary, out);
        else if (type == "response.reasoning_text.delta" || type == "reasoning_text.delta")
            HandleReasoningTextDelta(data, ReasoningKind::ProviderText, out);
        else if (type == "response.reasoning_text.done" || type == "reasoning_text.done")
            HandleReasoningTextDone(data, ReasoningKind::ProviderText, out);
        else if (type == "response.function_call_arguments.delta" ||
                 type == "function_call_arguments.delta")
            HandleFunctionArgumentsDelta(data, out);
        else if (type == "response.function_call_arguments.done" ||
                 type == "function_call_arguments.done")
            HandleFunctionArgumentsDone(data);
        else if (type == "response.completed" || type == "response.incomplete" ||
                 type == "response.failed" || type == "error")
            HandleTerminalEvent(data, type, out);
        return out;
    }

  public:
    std::vector<StreamEvent> OnEvent(SseEvent const& event) override {
        try {
            return OnEventImpl(event);
        } catch (ProtocolError const& error) {
            if (error.Failure() != ProtocolFailure::InvalidData)
                throw;
            throw ProtocolError(error.what(), ProtocolFailure::InvalidStreamEvent);
        } catch (Json::exception const&) {
            throw ProtocolError("Responses stream event is malformed.",
                                ProtocolFailure::InvalidStreamField);
        }
    }
    bool Complete() const noexcept override { return terminal_; }
    ModelResult Finish() override {
        if (!terminal_)
            throw ProtocolError("Responses stream ended before a terminal event.",
                                ProtocolFailure::IncompleteStream);
        if (failed_)
            Bad("Responses stream failed.");
        bool truncated = finalResponse_.value("__fairy_finish_truncated", false);
        finalResponse_.erase("__fairy_finish_truncated");
        auto result = ParseTerminal(finalResponse_, truncated);
        if (truncated)
            result.message.toolCalls.clear();
        return result;
    }
};
std::unique_ptr<IResponseDecoder> CreateResponsesDecoder() {
    return std::make_unique<Decoder>();
}
} // namespace fairy::providers
