#pragma once
#include "completion/CompletionService.h"
namespace winrt::Folia {
// Credentials are supplied separately; the transport never reads app settings.
CompletionResult RequestCompletionHttp(CompletionConfig const&, folia::completion::Prompt const&,
    std::stop_token, std::wstring key, std::chrono::milliseconds inactivityTimeout = std::chrono::seconds(15));
struct CompletionModel {
    std::string id, name;
    std::vector<std::string> reasoningEfforts;
};
struct CompletionModelPage {
    std::vector<CompletionModel> models;
    std::string next;
};
CompletionModelPage ParseCompletionModels(std::string_view json, int protocol);
std::vector<CompletionModel> FetchCompletionModels(CompletionConfig const&, std::stop_token, std::wstring key);
}
