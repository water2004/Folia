#pragma once
#include "settings/CompletionPromptSettings.h"

namespace completion_tests {
inline folia::completion::Prompt MakePrompt(folia::completion::Context const& context,
    folia::completion::Budget budget = {}, folia::completion::Eagerness level = folia::completion::Eagerness::Balanced) {
    return folia::completion::make_prompt(context, winrt::Folia::LoadCompletionPromptSettings(), budget, level);
}
}
