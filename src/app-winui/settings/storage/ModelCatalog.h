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
    };

    std::vector<ModelProfile> Profiles();
    ModelProfile SaveProfile(ModelProfile, std::optional<std::wstring> key);
    std::string ImportLegacyCompletion(folia::platform::completion::CompletionConfig const&,
                                      std::wstring const& credentialTarget);
    void DeleteProfile(std::string const& id);
    std::wstring ProfileKey(ModelProfile const& connection);
    std::vector<ConfiguredModel> Models();
    std::pair<ConfiguredModel, ModelProfile> Resolve(std::string const& modelId);
    bool CanUseHostedSearch(ConfiguredModel const& model, ModelProfile const& profile) noexcept;
}
