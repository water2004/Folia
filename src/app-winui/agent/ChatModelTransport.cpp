#include "pch.h"
#include "agent/ChatModelTransport.h"
#include "settings/storage/ProviderPresets.h"
#include "windows/ai/ModelHttpClient.h"

namespace winrt::Folia::chat {
    using namespace folia::ai;
    using agent::ProgressKind;

    agent::Request MakeChatRequest(models::ProviderConnection connection, std::optional<WebSearchDialect> search) {
        auto const& preset = models::Preset(connection.providerType);
        ProviderRequestOptions adapter{search.value_or(WebSearchDialect::Standard), preset.explicitThinkingToggle, preset.cache};
        return [connection = std::move(connection), adapter](ModelRequest const& wire, std::stop_token stop, agent::Progress const& progress) {
            auto observe = [&](StreamEvent const& event) {
                if (!progress) return;
                using enum StreamEventKind;
                switch (event.kind) {
                case TextDelta:
                    progress(ProgressKind::TextDelta, event.text);
                    break;
                case ReasoningDelta:
                    progress(event.reasoningKind == ReasoningKind::Summary ? ProgressKind::SummaryDelta : ProgressKind::ReasoningDelta, event.text);
                    break;
                case WebSearchStarted:
                case WebSearchCompleted:
                case WebSearchFailed:
                    progress(ProgressKind::HostedSearch, Json{{"id", event.callId}, {"target", event.text}, {"operation", event.name},
                        {"done", event.kind != WebSearchStarted}, {"failed", event.kind == WebSearchFailed}}.dump());
                    break;
                default:
                    break;
                }
            };
            return folia::platform::models::RequestChat(connection.baseUrl, connection.protocol, wire,
                models::ProfileKey(connection), stop, connection.fullUrl, adapter, observe, [&](unsigned attempt) {
                    if (progress) progress(ProgressKind::Retrying, std::to_string(attempt));
                });
        };
    }
}
