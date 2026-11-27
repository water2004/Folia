#pragma once

#include "providers/HostedSearch.h"
#include "providers/ModelCatalogSchema.h"
import std;

namespace winrt::Folia::models {
    struct ProviderRoute {
        folia::ai::ApiProtocol protocol;
        std::string address;
        std::vector<folia::ai::WebSearchDialect> search;
        bool requiresSearchFormat = false;
        std::vector<std::string> effortChoices;
    };
    struct ProviderPreset {
        std::string id, name;
        std::vector<ProviderRoute> routes;
        bool explicitThinkingToggle = false;
        folia::ai::ModelCatalogSchema catalog = folia::ai::ModelCatalogSchema::Protocol;
        ProviderRoute const* Find(folia::ai::ApiProtocol protocol) const;
    };
    std::span<ProviderPreset const> ProviderPresets();
    ProviderPreset const& Preset(std::string_view id);
}
