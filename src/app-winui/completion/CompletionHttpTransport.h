#pragma once
#include "completion/CompletionService.h"
namespace winrt::Folia {
// Credentials are supplied separately; the transport never reads app settings.
CompletionResult RequestCompletionHttp(CompletionConfig const&, folia::completion::Prompt const&,
    std::stop_token, std::wstring key, std::chrono::milliseconds inactivityTimeout = std::chrono::seconds(15));
std::vector<std::string> FetchCompletionReasoningEfforts(CompletionConfig const&, std::stop_token, std::wstring key);
std::vector<std::string> ParseCompletionReasoningEfforts(std::string_view json, std::string_view model);
}
