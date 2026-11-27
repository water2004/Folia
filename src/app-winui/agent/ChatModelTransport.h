#pragma once

#include "agent/Agent.h"
#include "settings/storage/ModelCatalog.h"

namespace winrt::Folia::chat {
    folia::ai::agent::Request MakeChatRequest(models::ProviderConnection connection,
        std::optional<folia::ai::WebSearchDialect> search = {});
}
