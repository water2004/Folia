#pragma once
#include "windows/completion/CompletionService.h"
#include "providers/ModelCatalogSchema.h"
#include "providers/ModelTypes.h"
namespace folia::platform::completion {
// Credentials are supplied separately; the transport never reads app settings.
CompletionResult RequestCompletionHttp(CompletionConfig const&, folia::completion::Prompt const&,
    std::stop_token, std::wstring key, std::chrono::milliseconds inactivityTimeout = std::chrono::seconds(15));
struct CompletionModel {
    std::string id, name;
    std::vector<std::string> reasoningEfforts;
    std::optional<std::uint32_t> maxInputTokens, maxOutputTokens;
    bool vision = false;
    bool webSearch = false;
    std::optional<ai::WebSearchDialect> searchFormat;
};
struct CompletionModelPage {
    std::vector<CompletionModel> models;
    std::string next;
};
CompletionModelPage ParseCompletionModels(std::string_view json, int protocol, ai::ModelCatalogSchema = ai::ModelCatalogSchema::Protocol);
std::vector<CompletionModel> FetchCompletionModels(CompletionConfig const&, std::stop_token, std::wstring key, ai::ModelCatalogSchema = ai::ModelCatalogSchema::Protocol);
}
