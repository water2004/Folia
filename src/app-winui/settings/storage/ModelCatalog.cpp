#include "pch.h"
#include "settings/storage/ModelCatalog.h"
#include "settings/storage/ProviderPresets.h"
#include "providers/ModelValidation.h"
#include "storage/JsonRecords.h"
#include "storage/AssetPaths.h"
#include "windows/ai/ModelHttpClient.h"
#include "windows/completion/CompletionService.h"

namespace winrt::Folia::models {
    using namespace records;

    namespace {
        std::mutex files;
        auto ProvidersFolder() { return AssetPath(L"ai-models") / L"providers"; }

        std::wstring KeyTarget(std::string const& id) {
            (void)Path(ProvidersFolder(), id, L".json");
            return L"Folia/AIProvider/" + std::wstring(to_hstring(id));
        }

        ConfiguredModel DecodeModel(Json const& data, std::string const& provider, std::string id) {
            ConfiguredModel model{std::move(id), data.at("name").get<std::string>(), provider, data.at("upstream").get<std::string>(),
                data.at("effort").get<std::string>(), data.at("webSearch").get<bool>(), data.at("efforts").get<std::vector<std::string>>()};
            if (auto size = data.value("maxInputTokens", 0u)) model.maxInputTokens = size;
            if (auto size = data.value("maxOutputTokens", 0u)) model.maxOutputTokens = size;
            if (auto mode = data.find("searchFormat"); mode != data.end())
                model.searchFormat = folia::ai::ParseSearchDialect(mode->get_ref<std::string const&>());
            model.webFetch = data.at("webFetch").get<bool>();
            model.vision = data.at("vision").get<bool>();
            model.enabled = data.at("enabled").get<bool>();
            return model;
        }

        ModelProfile DecodeProfile(Json const& data, std::string id) {
            if (data.at("schemaVersion") != 1) throw std::invalid_argument("Unsupported provider record version");
            ModelProfile profile{std::move(id), data.at("name").get<std::string>(), data.at("baseUrl").get<std::string>(),
                folia::ai::ParseProtocol(data.at("protocol").get<std::string>())};
            profile.providerType = data.at("providerType").get<std::string>();
            profile.iconPng = data.value("iconPng", "");
            profile.fullUrl = data.at("fullUrl").get<bool>();
            profile.catalogUrl = data.value("catalogUrl", "");
            profile.allowWebFetch = data.at("allowWebFetch").get<bool>();
            if (data.contains("webFormat")) profile.webFormat = folia::ai::ParseSearchDialect(data.at("webFormat").get<std::string>());
            profile.allowWebSearch = data.at("allowWebSearch").get<bool>();
            for (auto const& value : data.at("models").get_ref<Json::array_t const&>())
                profile.models.push_back(DecodeModel(value, profile.id, value.at("id").get<std::string>()));
            return profile;
        }

        std::vector<ModelProfile>& Catalog() {
            static auto catalog = List<ModelProfile>(ProvidersFolder(), DecodeProfile);
            return catalog;
        }

        Json EncodeModels(ModelProfile& profile) {
            Json out = Json::array();
            std::set<std::string> ids;
            for (auto& model : profile.models) {
                if (model.id.empty()) model.id = NewId();
                model.provider = profile.id;
                if (model.name.size() > 256 || model.upstream.empty() ||
                    model.upstream.size() > 128 || model.upstream.find_first_of("\r\n") != std::string::npos ||
                    model.efforts.size() > 32 || !ids.insert(model.id).second ||
                    std::ranges::any_of(model.efforts, [&](auto const& effort) {
                        return effort.empty() || !folia::ai::SupportsReasoningOptions(profile.protocol, {effort});
                    }) || model.ResolveEffort(model.effort) != model.effort)
                    throw std::invalid_argument("Invalid model configuration");
                Json entry{{"id", model.id}, {"name", model.name}, {"upstream", model.upstream},
                    {"effort", model.effort}, {"efforts", model.efforts}, {"webSearch", model.webSearch},
                    {"webFetch", model.webFetch}, {"vision", model.vision}, {"enabled", model.enabled}};
                if (model.searchFormat) entry["searchFormat"] = folia::ai::SearchDialectId(*model.searchFormat);
                if (model.maxInputTokens) entry["maxInputTokens"] = *model.maxInputTokens;
                if (model.maxOutputTokens) entry["maxOutputTokens"] = *model.maxOutputTokens;
                out.push_back(std::move(entry));
            }
            return out;
        }
    }

    std::string ConfiguredModel::ResolveEffort(std::string_view selection) const {
        auto allowed = [&](std::string_view value) { return std::ranges::find(efforts, value) != efforts.end(); };
        if (!selection.empty() && allowed(selection)) return std::string(selection);
        return !effort.empty() && allowed(effort) ? effort : std::string{};
    }

    HostedTools ResolveHostedTools(ConfiguredModel const& model, ProviderConnection const& profile) {
        using folia::ai::PageAccess;
        auto route = Preset(profile.providerType).Find(profile.protocol);
        if (!route) return {};
        auto format = profile.webFormat ? profile.webFormat : model.searchFormat;
        if (!format && !route->requiresSearchFormat && !route->search.empty()) format = route->search.front();
        HostedTools tools;
        if (model.webSearch && profile.allowWebSearch && format && std::ranges::contains(route->search, *format))
            tools.search = format;
        auto pages = route->Pages(format);
        if (pages == PageAccess::WithSearch ? tools.search.has_value() : model.webFetch && profile.allowWebFetch)
            tools.pages = pages;
        return tools;
    }

    std::vector<ModelProfile> Profiles() {
        std::lock_guard lock(files);
        return Catalog();
    }

    static ModelProfile SaveProfileLocked(ModelProfile profile, std::optional<std::wstring> const& key) {
        if (profile.id.empty()) profile.id = NewId();
        auto route = Preset(profile.providerType).Find(profile.protocol);
        if (!route) throw std::invalid_argument("The selected provider does not offer this protocol.");
        if (profile.webFormat && !std::ranges::contains(route->search, *profile.webFormat))
            throw std::invalid_argument("Unsupported web tool format on this connection");
        for (auto& model : profile.models) {
            if (model.searchFormat && !std::ranges::contains(route->search, *model.searchFormat))
                model.searchFormat.reset();
        }
        auto models = EncodeModels(profile);
        auto& catalog = Catalog();
        auto existing = std::ranges::find(catalog, profile.id, &ModelProfile::id);
        auto target = KeyTarget(profile.id);
        auto path = Path(ProvidersFolder(), profile.id, L".json");
        auto previous = folia::platform::models::LoadKey(target);
        if (!key && !previous.empty() && existing != catalog.end() &&
            (existing->baseUrl != profile.baseUrl || existing->protocol != profile.protocol ||
             existing->fullUrl != profile.fullUrl || existing->catalogUrl != profile.catalogUrl))
            throw std::runtime_error("Enter or clear the API key before changing the provider connection.");
        auto saved = profile;
        auto index = static_cast<std::size_t>(existing - catalog.begin());
        if (index == catalog.size()) catalog.reserve(catalog.size() + 1);
        if (key) folia::platform::models::StoreKey(target, *key);
        try {
            Json data{{"schemaVersion", 1}, {"name", profile.name}, {"baseUrl", profile.baseUrl},
                {"protocol", folia::ai::ProtocolName(profile.protocol)}, {"models", std::move(models)},
                {"fullUrl", profile.fullUrl}, {"allowWebSearch", profile.allowWebSearch}, {"allowWebFetch", profile.allowWebFetch}, {"providerType", profile.providerType}};
            if (profile.webFormat) data["webFormat"] = folia::ai::SearchDialectId(*profile.webFormat);
            if (!profile.catalogUrl.empty()) data["catalogUrl"] = profile.catalogUrl;
            if (!profile.iconPng.empty()) data["iconPng"] = profile.iconPng;
            Write(path, data);
        } catch (...) {
            if (key) folia::platform::models::StoreKey(target, previous);
            throw;
        }
        if (index == catalog.size()) catalog.insert(catalog.begin(), std::move(saved));
        else catalog[index] = std::move(saved);
        return profile;
    }

    ModelProfile SaveProfile(ModelProfile profile, std::optional<std::wstring> key) {
        std::lock_guard lock(files);
        return SaveProfileLocked(std::move(profile), key);
    }

    namespace {
        auto& RequireRecord(auto& records, std::string const& id) {
            auto it = std::ranges::find_if(records, [&](auto const& record) { return record.id == id; });
            if (it == records.end()) throw std::runtime_error("This configuration no longer exists.");
            return *it;
        }

        void UpdateModels(std::string const& providerId, auto change) {
            std::lock_guard lock(files);
            auto profile = RequireRecord(Catalog(), providerId);
            change(profile.models);
            SaveProfileLocked(std::move(profile), std::nullopt);
        }
    }

    void SaveConnection(ModelProfile connection, std::optional<std::wstring> key) {
        std::lock_guard lock(files);
        if (!connection.id.empty()) connection.models = RequireRecord(Catalog(), connection.id).models;
        SaveProfileLocked(std::move(connection), key);
    }

    void SaveModel(std::string const& providerId, ConfiguredModel model) {
        UpdateModels(providerId, [&](auto& models) {
            if (model.id.empty()) models.push_back(std::move(model));
            else RequireRecord(models, model.id) = std::move(model);
        });
    }

    void DeleteModel(std::string const& providerId, std::string const& modelId) {
        UpdateModels(providerId, [&](auto& models) {
            std::erase_if(models, [&](auto const& model) { return model.id == modelId; });
        });
    }

    void SetModelEnabled(std::string const& providerId, std::string const& modelId, bool enabled) {
        UpdateModels(providerId, [&](auto& models) { RequireRecord(models, modelId).enabled = enabled; });
    }

    std::string ImportLegacyCompletion(folia::platform::completion::CompletionConfig const& legacy,
                                       std::wstring const& credentialTarget) {
        namespace transport = folia::platform::models;
        auto endpoint = folia::platform::completion::ResolveCompletionEndpoint(legacy);
        auto protocol = static_cast<ApiProtocol>(legacy.protocol);
        if (!folia::ai::SupportsReasoningOptions(protocol, {legacy.reasoningEffort}))
            throw std::invalid_argument("Invalid legacy reasoning effort.");

        std::lock_guard lock(files);
        for (auto const& profile : Catalog()) {
            if (profile.protocol != protocol) continue;
            auto match = std::ranges::find(profile.models, legacy.model, &ConfiguredModel::upstream);
            if (match == profile.models.end()) continue;
            try {
                if (transport::Endpoint(profile.baseUrl, protocol, transport::Route::Model, profile.fullUrl) == endpoint)
                    return match->id;
            } catch (std::invalid_argument const&) {
                // An invalid connection cannot match the legacy one.
            }
        }

        auto host = Windows::Foundation::Uri(to_hstring(legacy.baseUrl)).Host();
        ModelProfile profile{{}, to_string(host), legacy.baseUrl, protocol};
        ConfiguredModel model{{}, legacy.model, {}, legacy.model, legacy.reasoningEffort};
        if (!model.effort.empty()) model.efforts.push_back(model.effort);
        profile.models.push_back(std::move(model));
        auto key = transport::LoadKey(credentialTarget);
        auto saved = SaveProfileLocked(std::move(profile), std::move(key));
        return saved.models.front().id;
    }

    void DeleteProfile(std::string const& id) {
        std::lock_guard lock(files);
        auto& catalog = Catalog();
        auto target = KeyTarget(id);
        auto previous = folia::platform::models::LoadKey(target);
        folia::platform::models::StoreKey(target, L"");
        try {
            std::filesystem::remove(Path(ProvidersFolder(), id, L".json"));
        } catch (...) {
            if (!previous.empty()) folia::platform::models::StoreKey(target, previous);
            throw;
        }
        std::erase_if(catalog, [&](auto const& profile) { return profile.id == id; });
    }

    std::wstring ProfileKey(ProviderConnection const& connection) {
        std::lock_guard lock(files);
        auto current = Read(Path(ProvidersFolder(), connection.id, L".json"));
        if (current.at("baseUrl") != connection.baseUrl || current.at("protocol") != folia::ai::ProtocolName(connection.protocol) ||
            current.value("fullUrl", false) != connection.fullUrl || current.value("catalogUrl", "") != connection.catalogUrl)
            throw std::runtime_error("AI provider settings changed. Select the model again.");
        return folia::platform::models::LoadKey(KeyTarget(connection.id));
    }

    std::vector<ModelChoice> AvailableModels() {
        std::lock_guard lock(files);
        std::vector<ModelChoice> choices;
        std::unordered_map<std::string, std::size_t> names;
        for (auto const& profile : Catalog()) {
            for (auto const& model : profile.models) {
                if (!model.enabled) continue;
                ++names[model.Label()];
                choices.push_back({{model, profile}, model.Label()});
            }
        }
        for (auto& choice : choices)
            if (names.at(choice.label) > 1) choice.label += " · " + choice.provider.name;
        return choices;
    }

    ResolvedModel Resolve(std::string const& id) {
        std::lock_guard lock(files);
        for (auto const& profile : Catalog()) {
            auto model = std::ranges::find(profile.models, id, &ConfiguredModel::id);
            if (model != profile.models.end() && model->enabled)
                return {*model, profile};
        }
        throw std::runtime_error("The selected AI model is unavailable.");
    }
}
