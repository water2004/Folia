#include "pch.h"
#include "settings/storage/ProviderPresets.h"
#include "agent/ChatView.h"
#include "agent/ChatControls.h"
#include "providers/ModelValidation.h"
#include "windows/ai/ModelHttpClient.h"
#include <wil/resource.h>

namespace winrt::Folia::chat {
    using namespace models;
    using folia::ai::MessageRole;
    using folia::ai::agent::Options;
    using folia::ai::agent::ProgressKind;
    namespace {
        std::mutex runningMutex;
        std::set<std::string> running;

        std::string TitleFrom(std::string_view text) {
            auto line = text.substr(0, text.find_first_of("\r\n"));
            auto end = std::min<std::size_t>(line.size(), 80);
            while (end < line.size() && end && (static_cast<unsigned char>(line[end]) & 0xc0) == 0x80) --end;
            return std::string(line.substr(0, end));
        }
    }

    void ChatView::Send() { if (!extras_->RunTypedCommand()) StartTurn(false); }

    fire_and_forget ChatView::CompactContext() {
        auto weak = weak_from_this();
        auto queue = root_.DispatcherQueue();
        auto id = conversation_.id;
        auto index = modelPicker_->SelectedIndex();
        if (transcript_->Empty()) { Report(Localize(L"ChatNothingToCompact")); co_return; }
        if (index < 0) { Report(Localize(L"ChatNeedsProvider")); co_return; }
        {
            std::lock_guard lock(runningMutex);
            if (!running.insert(id).second) { Report(Localize(L"ChatAlreadyRunning")); co_return; }
        }
        auto release = wil::scope_exit([&] { std::lock_guard lock(runningMutex); running.erase(id); });
        cancellation_ = {};
        auto stop = cancellation_.get_token();
        bool changed = false;
        hstring status;
        try {
            auto [model, profile] = Resolve(routes_.at(index).model.id);
            SetBusy(true);
            Report(Localize(L"ChatCompacting"));
            co_await resume_background();
            auto history = LoadHistory(id);
            Options options{model.upstream};
            if (model.maxInputTokens) options.inputBytes = *model.maxInputTokens;
            if (model.maxOutputTokens) options.outputTokens = *model.maxOutputTokens;
            changed = folia::ai::agent::Compact(history, options, [&](auto const& wire, auto token, auto const&) {
                return folia::platform::models::RequestChat(profile.baseUrl, profile.protocol, wire, ProfileKey(profile), token,
                    profile.fullUrl, {.explicitThinkingToggle = Preset(profile.providerType).explicitThinkingToggle}, {});
            }, stop);
            if (changed) AppendCompaction(id, history);
        } catch (std::exception const& error) { status = to_hstring(error.what()); }
        catch (hresult_error const& error) { status = error.message(); }
        queue.TryEnqueue([weak, changed, stopped = stop.stop_requested(), status] {
            if (auto view = weak.lock()) {
                view->SetBusy(false);
                if (changed) {
                    view->usedTokens_.reset();
                    view->UpdateUsage();
                    view->transcript_->Refresh(true);
                }
                view->Report(stopped ? Localize(L"ChatInterrupted") : !status.empty() ? status :
                    changed ? hstring{} : Localize(L"ChatNothingToCompact"));
            }
        });
    }

    void ChatView::StartTurn(bool retry) {
        if (busy_) return;
        if (extras_->Pasting()) { Report(Localize(L"ChatPreparingImage")); return; }
        try {
            auto question = FieldText(input_);
            if (retry) {
                auto last = transcript_->LastReply();
                if (!last || (last->error.empty() && !last->interrupted)) return;
            } else if (question.empty() && extras_->Images().empty()) return;
            auto active = modelPicker_->SelectedIndex();
            if (active < 0) { Report(Localize(L"ChatNeedsProvider")); return; }
            auto [model, profile] = Resolve(routes_.at(active).model.id);
            if (!model.vision && !extras_->Images().empty()) { Report(Localize(L"ChatNeedsVision")); return; }
            if (question.size() > maxQuestionBytes) {
                Report(Localize(L"ChatInvalidPrompt"));
                return;
            }
            {
                std::lock_guard lock(runningMutex);
                if (!running.insert(conversation_.id).second) {
                    Report(Localize(L"ChatAlreadyRunning"));
                    return;
                }
            }
            auto release = wil::scope_exit([&] { std::lock_guard lock(runningMutex); running.erase(conversation_.id); });
            std::optional<std::vector<OpenTab>> tabs;
            auto available = host_.tabs();
            std::vector<folia::ai::agent::DocumentReference> mentions;
            auto original = retry ? transcript_->LastQuestion() : nullptr;
            for (auto const& tab : available) {
                if (!retry && extras_->Mentions(tab)) mentions.push_back(tab.document);
                else if (original && std::ranges::any_of(original->documents, [&](auto const& saved) {
                    return (!saved.tabKey.empty() && saved.tabKey == tab.document.tabKey) ||
                        (!saved.path.empty() && saved.path == tab.document.path);
                })) mentions.push_back(tab.document);
            }
            if (!openTabs_.IsChecked()) std::erase_if(available, [&](auto const& tab) {
                return !std::ranges::contains(mentions, tab.document);
            });
            if (openTabs_.IsChecked() || !available.empty()) tabs = std::move(available);
            SaveConversationOptions(conversation_);
            if (!retry) {
                if (transcript_->Empty()) conversation_ = RenameConversation(conversation_.id,
                    question.empty() ? to_string(Localize(L"ChatImageMessage")) : TitleFrom(question), TitleSource::Prompt);
                Entry entry{MessageRole::User, std::move(question)};
                entry.documents = std::move(mentions);
                auto rollback = wil::scope_exit([&] {
                    for (auto const& image : entry.images) DeleteFileW(AttachmentPath(conversation_.id, image).c_str());
                });
                for (auto const& image : extras_->Images()) entry.images.push_back(StoreImage(conversation_.id, image.png));
                AppendEntry(conversation_.id, entry);
                rollback.release();
                extras_->Clear();
                input_.Text({});
            }
            KeepPreview();
            transcript_->SetTitle(to_hstring(conversation_.title));
            transcript_->Refresh(true);
            transcript_->Begin();
            ArrangeComposer();
            host_.changed(conversation_);
            cancellation_ = {};
            SetBusy(true);
            Report({});
            Execute(weak_from_this(), conversation_, std::move(profile), std::move(model), std::move(tabs), retry,
                cancellation_.get_token(), root_.DispatcherQueue());
            release.release();
        } catch (std::exception const& error) { SetBusy(false); Report(to_hstring(error.what())); }
        catch (hresult_error const& error) { SetBusy(false); Report(error.message()); }
    }

    fire_and_forget ChatView::GenerateTitle(std::weak_ptr<ChatView> weak, std::string id, ModelProfile profile,
        ConfiguredModel model, std::string prompt, Microsoft::UI::Dispatching::DispatcherQueue queue) {
        co_await resume_background();
        try {
            using namespace folia::ai;
            if (!CanGenerateTitle(id)) co_return;
            ModelRequest wire;
            wire.model = model.upstream;
            wire.maxOutputTokens = (std::min)(model.maxOutputTokens.value_or(512u), 512u);
            if (!model.efforts.empty()) wire.reasoning.effort = model.efforts.front();
            if (prompt.size() > 1600) {
                std::size_t end = 1600;
                while ((static_cast<unsigned char>(prompt[end]) & 0xC0) == 0x80) --end;
                prompt.resize(end);
            }
            Message instruction, question;
            instruction.role = MessageRole::System;
            instruction.content.push_back({ContentKind::Text,
                "Title this conversation from its first message in the user's language. Return just one short specific title, at most 8 words or 18 Chinese characters. No quotes, Markdown or explanation."});
            question.role = MessageRole::User;
            question.content.push_back({ContentKind::Text, std::move(prompt)});
            wire.messages = {std::move(instruction), std::move(question)};
            auto result = folia::platform::models::RequestChat(profile.baseUrl, profile.protocol, wire, ProfileKey(profile), {},
                profile.fullUrl, {.explicitThinkingToggle = Preset(profile.providerType).explicitThinkingToggle}, {});
            if (result.finish != FinishReason::Completed) co_return;
            auto text = PlainText(result.message);
            auto start = text.find_first_not_of(" \t\r\n\"");
            if (start == text.npos) co_return;
            auto last = text.find_last_not_of(" \t\r\n\"");
            auto title = TitleFrom(std::string_view(text).substr(start, last - start + 1));
            if (title.empty()) co_return;
            RenameConversation(id, title, TitleSource::Model);
            queue.TryEnqueue([weak, id] {
                if (auto self = weak.lock()) {
                    try {
                        auto saved = LoadConversation(id);
                        self->SetTitle(saved.title);
                        self->host_.changed(saved);
                    } catch (std::exception const&) {}
                }
            });
        } catch (std::exception const&) {} catch (hresult_error const&) {}
    }

    fire_and_forget ChatView::Execute(std::weak_ptr<ChatView> weak, Conversation conversation, ModelProfile profile, ConfiguredModel model,
        std::optional<std::vector<OpenTab>> tabs, bool retry, std::stop_token stop,
        Microsoft::UI::Dispatching::DispatcherQueue queue) {
        auto release = wil::scope_exit([&] { std::lock_guard lock(runningMutex); running.erase(conversation.id); });
        co_await resume_background();
        std::wstring error;
        std::optional<Entry> answer;
        Entry partial{MessageRole::Assistant};
        bool writing = false;
        std::string titlePrompt;
        try {
            auto history = LoadHistory(conversation.id);
            auto recordedCount = history.entries.size();
            if (retry) {
                while (!history.entries.empty() && history.entries.back().role == MessageRole::Assistant &&
                    (history.entries.back().interrupted || !history.entries.back().error.empty())) history.entries.pop_back();
            }
            auto skippedTail = recordedCount - history.entries.size();
            if (!history.retired && history.entries.size() == 1) titlePrompt = history.entries.front().markdown;
            auto tools = MakeTools(std::move(tabs), conversation.folder, [&](auto const& ref) {
                partial.documents.push_back(ref);
                queue.TryEnqueue([weak, ref] { if (auto view = weak.lock(); view && view->busy_) view->transcript_->AddReference(ref); });
            });
            auto search = HostedSearchFormat(model, profile);
            Options options{model.upstream, model.ResolveEffort(conversation.effort), conversation.search ? search : std::nullopt};
            const auto pages = HostedPageAccess(model, profile);
            if (pages == folia::ai::PageAccess::WithSearch ? options.webSearch.has_value() : conversation.readPages)
                options.pages = pages;
            // A UTF-8 byte budget conservatively bounds text tokens without guessing the upstream tokenizer.
            if (model.maxInputTokens) options.inputBytes = *model.maxInputTokens;
            if (model.maxOutputTokens) options.outputTokens = *model.maxOutputTokens;
            if (model.vision) options.loadImage = [&](std::string const& image) { return LoadImage(conversation.id, image); };
            auto dispatch = [&](ProgressKind kind, std::string_view text) {
                auto value = to_hstring(text);
                queue.TryEnqueue([weak, kind, value] {
                    if (auto view = weak.lock(); view && view->busy_) {
                        if (kind == ProgressKind::InputUsage) {
                            view->usedTokens_ = std::stoull(value.c_str());
                            view->usageModel_ = view->conversation_.model;
                            view->UpdateUsage();
                        } else view->transcript_->Progress(kind, value);
                    }
                });
            };
            // Deliver the pending lane before a tool or another text kind starts, even
            // when both arrive within a single display interval.
            std::string pendingText, reasoning, pendingCaption;
            std::optional<ProgressKind> pending;
            auto deadline = std::chrono::steady_clock::time_point{};
            auto flush = [&] {
                if (pending == ProgressKind::TextDelta) { dispatch(*pending, pendingText); pendingText.clear(); }
                if (pending == ProgressKind::ReasoningDelta) dispatch(ProgressKind::ReasoningSnapshot, reasoning);
                pending.reset();
                if (!pendingCaption.empty()) { dispatch(ProgressKind::ReasoningSummary, pendingCaption); pendingCaption.clear(); }
            };
            auto publish = [&](ProgressKind kind, std::string_view text) {
                if (kind == ProgressKind::ReasoningSummary && pending == ProgressKind::ReasoningDelta) {
                    pendingCaption = text;
                    return;
                }
                if (pending && *pending != kind) flush();
                if (kind == ProgressKind::TextDelta || kind == ProgressKind::ReasoningDelta) {
                    (kind == ProgressKind::TextDelta ? pendingText : reasoning).append(text);
                    pending = kind;
                    auto now = std::chrono::steady_clock::now();
                    if (now >= deadline) { flush(); deadline = now + std::chrono::milliseconds(40); }
                } else {
                    if (kind == ProgressKind::StepStarted) { reasoning.clear(); deadline = {}; }
                    dispatch(kind, text);
                }
            };
            folia::ai::ProviderRequestOptions adapter{search.value_or(folia::ai::WebSearchDialect::Standard), Preset(profile.providerType).explicitThinkingToggle};
            auto request = [&](auto const& wire, auto token, folia::ai::agent::Progress const& progress) {
                return folia::platform::models::RequestChat(profile.baseUrl, profile.protocol, wire, ProfileKey(profile), token,
                    profile.fullUrl, adapter, [&](folia::ai::StreamEvent const& event) {
                        if (!progress) return;
                        if (event.kind == folia::ai::StreamEventKind::TextDelta) progress(ProgressKind::TextDelta, event.text);
                        if (event.kind == folia::ai::StreamEventKind::ReasoningDelta)
                            progress(event.reasoningKind == folia::ai::ReasoningKind::Summary ? ProgressKind::SummaryDelta : ProgressKind::ReasoningDelta, event.text);
                        using enum folia::ai::StreamEventKind;
                        if (event.kind == WebSearchStarted || event.kind == WebSearchCompleted || event.kind == WebSearchFailed)
                            progress(ProgressKind::HostedSearch, folia::ai::Json{{"id", event.callId}, {"target", event.text}, {"operation", event.name},
                                {"done", event.kind != WebSearchStarted}, {"failed", event.kind == WebSearchFailed}}.dump());
                    }, [&](unsigned attempt) { if (progress) progress(ProgressKind::Retrying, std::to_string(attempt)); });
            };
            answer = folia::ai::agent::Run(history, options, tools, request, publish, stop, &partial);
            answer->modelId = model.id;
            writing = true;
            AppendEntry(conversation.id, *answer, &history, skippedTail);
        } catch (std::exception const& exception) { answer.reset(); error = to_hstring(exception.what()); }
        catch (hresult_error const& exception) { answer.reset(); error = exception.message(); }
        if (!answer && !writing) {
            partial.modelId = model.id;
            partial.interrupted = stop.stop_requested();
            if (!partial.interrupted) partial.error = to_string(hstring(error));
            try { AppendEntry(conversation.id, partial); error.clear(); }
            catch (std::exception const& exception) { error = to_hstring(exception.what()); }
        }
        bool generate = answer && !titlePrompt.empty() && !stop.stop_requested();
        queue.TryEnqueue([weak, answer = std::move(answer), error = std::move(error)]() mutable {
            auto view = weak.lock();
            if (!view) return;
            view->SetBusy(false);
            if (answer) {
                view->usageModel_ = answer->modelId;
                view->usedTokens_ = answer->usage.inputTokens;
                view->UpdateUsage();
            }
            view->transcript_->Finish();
            view->Report(hstring(error));
        });
        if (generate)
            GenerateTitle(weak, conversation.id, std::move(profile), std::move(model), std::move(titlePrompt), queue);
    }
}
