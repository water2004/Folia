#include "pch.h"
#include "settings/storage/ProviderPresets.h"
#include "storage/AssetPaths.h"
#include "storage/JsonRecords.h"

namespace winrt::Folia::models {
    folia::ai::PageAccess ProviderRoute::Pages(std::optional<folia::ai::WebSearchDialect> format) const {
        if (!format && (requiresSearchFormat || search.empty())) return folia::ai::PageAccess::None;
        auto it = pages.find(format ? *format : search.front());
        return it == pages.end() ? folia::ai::PageAccess::None : it->second;
    }
    bool ProviderRoute::HasPageTool() const {
        return std::ranges::any_of(pages, [](auto const& item) { return folia::ai::SeparatePageTool(item.second); });
    }

    ProviderRoute const* ProviderPreset::Find(folia::ai::ApiProtocol protocol) const {
        auto found = std::ranges::find(routes, protocol, &ProviderRoute::protocol);
        return found == routes.end() ? nullptr : &*found;
    }

    std::span<ProviderPreset const> ProviderPresets() {
        static auto const presets = [] {
            std::vector<ProviderPreset> items;
            for (auto const& item : records::Read(AssetPath(L"provider-presets.json"))) {
                ProviderPreset preset{item.at("id").get<std::string>(), item.at("name").get<std::string>()};
                preset.explicitThinkingToggle = item.value("explicitThinkingToggle", false);
                if (auto schema = item.find("catalog"); schema != item.end()) {
                    if (*schema != "openrouter") throw std::runtime_error("Unknown model catalog schema");
                    preset.catalog = folia::ai::ModelCatalogSchema::OpenRouter;
                }
                for (auto const& spec : item.at("routes")) {
                    ProviderRoute route{folia::ai::ParseProtocol(spec.at("protocol").get_ref<std::string const&>()), spec.at("address").get<std::string>()};
                    route.effortChoices = item.value("effortChoices", std::vector<std::string>{});
                    route.requiresSearchFormat = spec.value("selectSearchFormat", false);
                    for (auto const& option : spec.at("search")) {
                        auto dialect = folia::ai::ParseSearchDialect(option.get_ref<std::string const&>());
                        if (!folia::ai::SupportsSearchDialect(route.protocol, dialect)) throw std::runtime_error("Invalid provider search format");
                        route.search.push_back(dialect);
                    }
                    if (auto pages = spec.find("pages"); pages != spec.end())
                        for (auto const& [format, mode] : pages->items())
                            route.pages.emplace(folia::ai::ParseSearchDialect(format), folia::ai::ParsePageAccess(mode.get<std::string>()));
                    if (preset.Find(route.protocol)) throw std::runtime_error("Duplicate provider protocol");
                    preset.routes.push_back(std::move(route));
                }
                if (preset.routes.empty() || std::ranges::contains(items, preset.id, &ProviderPreset::id))
                    throw std::runtime_error("Invalid provider preset");
                items.push_back(std::move(preset));
            }
            return items;
        }();
        return presets;
    }

    ProviderPreset const& Preset(std::string_view id) {
        auto choices = ProviderPresets();
        auto found = std::ranges::find(choices, id, &ProviderPreset::id);
        if (found == choices.end()) throw std::invalid_argument("Unknown provider type");
        return *found;
    }
}
