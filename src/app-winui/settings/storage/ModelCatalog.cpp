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
            auto effort = data.value("effort", "");
            auto levels = data.value("efforts", std::vector<std::string>{});
            if (!data.contains("efforts") && !effort.empty()) levels.push_back(effort);
            ConfiguredModel model{std::move(id), data.at("name").get<std::string>(), provider, data.at("upstream").get<std::string>(),
                std::move(effort), data.value("webSearch", false), std::move(levels)};
            if (auto size = data.value("maxInputTokens", 0u)) model.maxInputTokens = size;
            if (auto size = data.value("maxOutputTokens", 0u)) model.maxOutputTokens = size;
            if (auto mode = data.find("searchFormat"); mode != data.end())
                model.searchFormat = folia::ai::ParseSearchDialect(mode->get_ref<std::string const&>());
            model.vision = data.value("vision", false);
            model.enabled = data.value("enabled", true);
            return model;
        }

        struct StoredProvider {
            ModelProfile profile;
            bool hasModels = false;
        };

        StoredProvider DecodeProfile(Json const& data, std::string id) {
            ModelProfile profile{std::move(id), data.at("name").get<std::string>(), data.at("baseUrl").get<std::string>(),
                folia::ai::ParseProtocol(data.at("protocol").get<std::string>())};
            profile.providerType = data.value("providerType", "custom");
            profile.iconPng = data.value("iconPng", "");
            profile.fullUrl = data.value("fullUrl", false);
            profile.catalogUrl = data.value("catalogUrl", "");
            profile.allowWebSearch = data.value("allowWebSearch", true);
            auto models = data.find("configuredModels");
            if (models != data.end())
                for (auto const& value : *models)
                    profile.models.push_back(DecodeModel(value, profile.id, value.at("id").get<std::string>()));
            return {std::move(profile), models != data.end()};
        }

        std::vector<ModelProfile> ReadCatalog() {
            auto saved = List<StoredProvider>(ProvidersFolder(), DecodeProfile);
            if (std::ranges::any_of(saved, [](auto const& item) { return !item.hasModels; })) {
                // Earlier builds kept each configured model in a separate record.
                // Import those IDs into the provider draft until it is saved as a unit.
                auto previous = List<ConfiguredModel>(AssetPath(L"ai-models") / L"models",
                    [](Json const& data, std::string id) { return DecodeModel(data, data.at("provider").get<std::string>(), std::move(id)); });
                for (auto& model : previous)
                    if (auto owner = std::ranges::find_if(saved, [&](auto const& item) {
                            return !item.hasModels && item.profile.id == model.provider;
                        }); owner != saved.end())
                        owner->profile.models.push_back(std::move(model));
            }
            std::vector<ModelProfile> result;
            for (auto& item : saved) result.push_back(std::move(item.profile));
            return result;
        }

        std::vector<ModelProfile>& Catalog() {
            static auto catalog = ReadCatalog();
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
                    {"vision", model.vision}, {"enabled", model.enabled}};
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

    std::optional<folia::ai::WebSearchDialect> HostedSearchFormat(ConfiguredModel const& model, ModelProfile const& profile) {
        if (!model.webSearch || !profile.allowWebSearch) return {};
        auto route = Preset(profile.providerType).Find(profile.protocol);
        if (!route || route->search.empty()) return {};
        if (!model.searchFormat) return route->requiresSearchFormat ? std::nullopt : std::optional(route->search.front());
        return std::ranges::contains(route->search, *model.searchFormat) ? model.searchFormat : std::nullopt;
    }

    std::vector<ModelProfile> Profiles() {
        std::lock_guard lock(files);
        return Catalog();
    }

    static ModelProfile SaveProfileLocked(ModelProfile profile, std::optional<std::wstring> const& key) {
        if (profile.id.empty()) profile.id = NewId();
        auto route = Preset(profile.providerType).Find(profile.protocol);
        if (!route) throw std::invalid_argument("The selected provider does not offer this protocol.");
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
            Json data{{"name", profile.name}, {"baseUrl", profile.baseUrl},
                {"protocol", folia::ai::ProtocolName(profile.protocol)}, {"configuredModels", std::move(models)},
                {"fullUrl", profile.fullUrl}, {"allowWebSearch", profile.allowWebSearch}, {"providerType", profile.providerType}};
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

    std::string ImportLegacyCompletion(folia::platform::completion::CompletionConfig const& legacy,
                                       std::wstring const& credentialTarget) {
        namespace transport = folia::platform::models;
        auto endpoint = folia::platform::completion::ResolveCompletionEndpoint(legacy);
        if (!legacy.catalogUrl.empty()) transport::ValidateUrl(legacy.catalogUrl);
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
        profile.fullUrl = legacy.fullUrl;
        profile.catalogUrl = legacy.catalogUrl;
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

    std::wstring ProfileKey(ModelProfile const& connection) {
        std::lock_guard lock(files);
        auto current = Read(Path(ProvidersFolder(), connection.id, L".json"));
        if (current.at("baseUrl") != connection.baseUrl || current.at("protocol") != folia::ai::ProtocolName(connection.protocol) ||
            current.value("fullUrl", false) != connection.fullUrl || current.value("catalogUrl", "") != connection.catalogUrl)
            throw std::runtime_error("AI provider settings changed. Select the model again.");
        return folia::platform::models::LoadKey(KeyTarget(connection.id));
    }

    std::vector<ConfiguredModel> Models() {
        std::lock_guard lock(files);
        std::vector<ConfiguredModel> models;
        for (auto const& provider : Catalog())
            models.append_range(provider.models | std::views::filter(&ConfiguredModel::enabled));
        return models;
    }

    std::pair<ConfiguredModel, ModelProfile> Resolve(std::string const& id) {
        std::lock_guard lock(files);
        for (auto const& profile : Catalog()) {
            auto model = std::ranges::find(profile.models, id, &ConfiguredModel::id);
            if (model != profile.models.end() && model->enabled)
                return {*model, {profile.id, profile.name, profile.baseUrl, profile.protocol, {}, {},
                    profile.fullUrl, profile.catalogUrl, profile.allowWebSearch, profile.providerType}};
        }
        throw std::runtime_error("The selected AI model is unavailable.");
    }
}
