#pragma once

#include "providers/ModelTypes.h"
import std;

namespace folia::platform::completion { struct CompletionConfig; }

namespace winrt::Folia::models {
    using folia::ai::ApiProtocol;

    struct ConfiguredModel {
        std::string id, name, provider, upstream, effort;
        bool webSearch = false;
        std::vector<std::string> efforts;
        std::optional<std::uint32_t> maxInputTokens, maxOutputTokens;
        bool vision = false;
        bool enabled = true;
        std::optional<folia::ai::WebSearchDialect> searchFormat;
        bool webFetch = false;

        std::string const& Label() const { return name.empty() ? upstream : name; }
        std::string ResolveEffort(std::string_view selection) const;
    };

    struct ModelProfile {
        std::string id, name, baseUrl;
        ApiProtocol protocol = ApiProtocol::Responses;
        std::vector<ConfiguredModel> models;
        std::string iconPng;
        bool fullUrl = false;
        std::string catalogUrl;
        bool allowWebSearch = false;
        std::string providerType = "custom";
        bool allowWebFetch = false;
        std::optional<folia::ai::WebSearchDialect> webFormat;
    };

    std::vector<ModelProfile> Profiles();
    ModelProfile SaveProfile(ModelProfile, std::optional<std::wstring> key);
    std::string ImportLegacyCompletion(folia::platform::completion::CompletionConfig const&,
                                      std::wstring const& credentialTarget);
    void DeleteProfile(std::string const& id);
    std::wstring ProfileKey(ModelProfile const& connection);
    std::vector<ConfiguredModel> Models();
    std::pair<ConfiguredModel, ModelProfile> Resolve(std::string const& modelId);
    folia::ai::PageAccess HostedPageAccess(ConfiguredModel const&, ModelProfile const&);
    std::optional<folia::ai::WebSearchDialect> HostedSearchFormat(ConfiguredModel const&, ModelProfile const&);
}
