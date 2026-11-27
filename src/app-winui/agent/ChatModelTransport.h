#pragma once

#include "settings/storage/ModelCatalog.h"

import std;
import folia.ai.agent;

namespace winrt::Folia::chat {
    folia::ai::agent::Request MakeChatRequest(models::ProviderConnection connection,
        std::optional<folia::ai::WebSearchDialect> search = {});
}
