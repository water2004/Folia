#include "pch.h"
#include "agent/ChatTurnView.h"
#include "agent/ChatControls.h"

namespace winrt::Folia::chat {
    using folia::ai::agent::ProgressKind;
    using folia::ai::agent::StepEnd;

    ChatTurnView::ChatTurnView(folia::ThemeProfile const& theme, OpenDocument open) : theme_(theme), open_(std::move(open)) {
        root_.Spacing(6);
        root_.Name(L"ChatTurn");
    }
    void ChatTurnView::SetTheme(folia::ThemeProfile const& theme) {
        theme_ = theme;
        for (auto const& surface : surfaces_) surface->SetTheme(theme);
    }
    void ChatTurnView::SealText() {
        if (surface_) surface_->Seal();
        surface_.reset();
    }
    void ChatTurnView::CloseActivity() {
        if (activity_) activity_->Complete();
        activity_.reset();
        inReason_ = false;
    }
    ChatActivityView& ChatTurnView::Activity() {
        SealText();
        if (!activity_) {
            activity_ = ChatActivityView::Create();
            activities_.push_back(activity_);
            root_.Children().Append(activity_->Element());
        }
        return *activity_;
    }
    void ChatTurnView::Text(std::string_view piece, bool initial) {
        if (piece.empty()) return;
        bool followingActivity = static_cast<bool>(activity_);
        CloseActivity();
        if (followingActivity && piece.starts_with("\n\n")) piece.remove_prefix(2);
        if (piece.empty()) return;
        if (!surface_) {
            surface_ = ChatReplySurface::Create(initial ? std::string(piece) : std::string{}, theme_);
            surface_->openDocument = [open = open_, documents = documents_](auto const& link) { open(link, *documents); };
            root_.Children().Append(surface_->Element());
            surfaces_.push_back(surface_);
            if (initial) return;
        }
        surface_->AppendMarkdown(piece);
    }
    void ChatTurnView::Progress(ProgressKind kind, hstring const& value) {
        if (kind == ProgressKind::StepStarted) {
            stepStart_ = markdown_.size();
            stepReason_.clear();
            searches_.clear();
            lastReason_.reset();
            inReason_ = false;
            return;
        }
        if (kind == ProgressKind::InputUsage) return;
        if (kind == ProgressKind::TextDelta || kind == ProgressKind::TextSnapshot) {
            auto buffer = to_string(value);
            std::string_view piece(buffer);
            bool first = stepStart_ == markdown_.size();
            if (kind == ProgressKind::TextSnapshot && !first) {
                auto written = std::string_view(markdown_).substr(stepStart_ + (stepStart_ ? 2 : 0));
                if (!piece.starts_with(written)) return; // A revised terminal snapshot is reconciled in Finish.
                piece.remove_prefix(written.size());
            }
            if (piece.empty()) return;
            auto begin = markdown_.size();
            if (first && stepStart_) markdown_ += "\n\n";
            markdown_.append(piece);
            Text(std::string_view(markdown_).substr(begin));
        } else if (kind == ProgressKind::ReasoningSnapshot || kind == ProgressKind::ReasoningDelta) {
            auto next = kind == ProgressKind::ReasoningSnapshot ? to_string(value) : stepReason_ + to_string(value);
            if (next.empty() || next == stepReason_) return;
            if (!inReason_) {
                reasonStart_ = next.starts_with(stepReason_) ? stepReason_.size() : 0;
                Activity().StartStep();
            }
            if (!next.starts_with(stepReason_)) reasonStart_ = 0;
            stepReason_ = std::move(next);
            Activity().Reason(to_hstring(std::string_view(stepReason_).substr(reasonStart_)));
            inReason_ = true;
            lastReason_ = activity_;
        } else if (kind == ProgressKind::ReasoningSummary) {
            if (lastReason_) lastReason_->Summary(value);
        } else if (kind == ProgressKind::HostedSearch) {
            auto update = folia::ai::Json::parse(to_string(value));
            auto description = update.value("operation", "web_search") + " " + update.value("target", "");
            auto id = update.at("id").get<std::string>();
            auto it = searches_.find(id);
            if (it == searches_.end()) {
                auto index = Activity().StartTool(description);
                it = searches_.emplace(std::move(id), std::pair{activity_, index}).first;
                inReason_ = false;
            }
            auto const& [trace, index] = it->second;
            trace->UpdateTool(index, description, update.value("done", false), update.value("failed", false), to_hstring(update.value("target", "")));
        } else {
            auto& trace = Activity();
            inReason_ = false;
            if (kind == ProgressKind::Reading) trace.StartTool(to_string(value));
            else if (kind == ProgressKind::ToolResult) {
                auto item = folia::ai::Json::parse(to_string(value));
                trace.EndTool(item.value("action", ""), item.value("failed", false), to_hstring(item.value("detail", "")));
            } else if (kind == ProgressKind::Retrying) trace.Status(LocalizeFormat(L"ChatRetrying", {value}), Localize(L"ChatRetried"));
            else if (kind == ProgressKind::Compacting) trace.Status(Localize(L"ChatCompacting"), Localize(L"ChatCompacted"));
        }
        received_ = true;
    }
    void ChatTurnView::Replay(Entry const& entry) {
        root_.Children().Clear();
        activity_.reset(); surface_.reset();
        activities_.clear(); surfaces_.clear();
        StepEnd previous;
        auto step = [&](StepEnd end) {
            auto reason = std::string_view(entry.reasoning).substr(previous.reasoning, end.reasoning - previous.reasoning);
            if (!reason.empty()) {
                if (reason.starts_with("\n\n")) reason.remove_prefix(2);
                auto& trace = Activity();
                trace.StartStep();
                trace.Reason(to_hstring(reason));
                auto caption = std::ranges::lower_bound(entry.reasoningSummaries, previous.reasoning, {}, &folia::ai::agent::ReasoningSummary::offset);
                if (caption != entry.reasoningSummaries.end() && caption->offset < end.reasoning) trace.Summary(to_hstring(caption->text));
            }
            Text(std::string_view(entry.markdown).substr(previous.text, end.text - previous.text), true);
            for (auto const& tool : std::span(entry.activities).subspan(previous.tools, end.tools - previous.tools)) {
                auto desc = tool.name + " " + tool.target;
                auto& trace = Activity();
                auto index = trace.StartTool(desc);
                if (!tool.pending) trace.UpdateTool(index, desc, true, tool.failed, to_hstring(tool.detail));
            }
            if (end.notice == StepEnd::Notice::Retry) Activity().Status(Localize(L"ChatRetried"), Localize(L"ChatRetried"));
            if (end.notice == StepEnd::Notice::Compaction) Activity().Status(Localize(L"ChatCompacted"), Localize(L"ChatCompacted"));
            previous = end;
        };
        for (auto end : entry.steps) step(end);
        step({entry.markdown.size(), entry.reasoning.size(), entry.activities.size()});
    }
    void ChatTurnView::Finish(Entry const& entry) {
        *documents_ = entry.documents;
        if (!received_ || markdown_ != entry.markdown) Replay(entry);
        SealText();
        CloseActivity();
    }
}
