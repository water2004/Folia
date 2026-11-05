#pragma once
#include <winrt/Windows.Data.Json.h>
import std;
import folia.completion;

namespace winrt::Folia {
folia::completion::PromptSettings LoadCompletionPromptSettings();
folia::completion::PromptSettings LoadCompletionPromptSettings(std::filesystem::path const&);
folia::completion::PromptSettings ParseCompletionPromptSettings(Windows::Data::Json::JsonObject const&);
Windows::Data::Json::JsonObject SerializeCompletionPromptSettings(folia::completion::PromptSettings const&);
}
