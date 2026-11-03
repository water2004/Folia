#pragma once
#include "completion/CompletionService.h"
namespace winrt::Folia {
// Credentials are supplied separately; the transport never reads app settings.
CompletionResult RequestCompletionHttp(CompletionConfig const&, folia::completion::Prompt const&,
    std::stop_token, std::wstring key);
}
