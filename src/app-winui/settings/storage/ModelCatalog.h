#pragma once

import std;
import folia.ai.model;

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

    struct ProviderConnection {
        std::string id, name, baseUrl;
        ApiProtocol protocol = ApiProtocol::Responses;
        bool fullUrl = false;
        std::string catalogUrl;
        bool allowWebSearch = false;
        std::string providerType = "custom";
        bool allowWebFetch = false;
        std::optional<folia::ai::WebSearchDialect> webFormat;

        ProviderConnection() = default;
        ProviderConnection(std::string id, std::string name, std::string baseUrl, ApiProtocol protocol = ApiProtocol::Responses)
            : id(std::move(id)), name(std::move(name)), baseUrl(std::move(baseUrl)), protocol(protocol) {}
    };

    struct ModelProfile : ProviderConnection {
        using ProviderConnection::ProviderConnection;
        std::vector<ConfiguredModel> models;
        std::string iconPng;
    };

    std::vector<ModelProfile> Profiles();
    ModelProfile SaveProfile(ModelProfile, std::optional<std::wstring> key);
    void SaveConnection(ModelProfile, std::optional<std::wstring> key);
    void SaveModel(std::string const& providerId, ConfiguredModel model);
    void DeleteModel(std::string const& providerId, std::string const& modelId);
    void SetModelEnabled(std::string const& providerId, std::string const& modelId, bool enabled);
    std::string ImportLegacyCompletion(folia::platform::completion::CompletionConfig const&,
                                      std::wstring const& credentialTarget);
    void DeleteProfile(std::string const& id);
    std::wstring ProfileKey(ProviderConnection const& connection);
    struct ResolvedModel {
        ConfiguredModel model;
        ProviderConnection provider;
    };
    struct ModelChoice : ResolvedModel {
        std::string label;
    };
    std::vector<ModelChoice> AvailableModels();
    ResolvedModel Resolve(std::string const& modelId);

    struct HostedTools {
        std::optional<folia::ai::WebSearchDialect> search;
        folia::ai::PageAccess pages = folia::ai::PageAccess::None;
    };
    HostedTools ResolveHostedTools(ConfiguredModel const&, ProviderConnection const&);
}
