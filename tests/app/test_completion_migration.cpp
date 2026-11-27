#include "pch.h"
#include "settings/storage/ModelCatalog.h"
#include "settings/storage/AppSettings.h"
#include "storage/AssetPaths.h"
#include "storage/JsonRecords.h"
#include "support/folia_test.hpp"
#include "windows/ai/ModelHttpClient.h"

namespace {
    using namespace boost::ut;
    using namespace winrt::Folia;
    namespace keys = folia::platform::models;

    suite completion_migration = [] {
        "legacy AI import copies a credential and reuses the saved model on retry"_test = [] {
            auto legacyTarget = L"Folia/Test/LegacyCompletion/" + std::wstring(winrt::to_hstring(records::NewId()));
            keys::StoreKey(legacyTarget, L"migration-test-key");
            auto eraseLegacy = wil::scope_exit([&] { keys::StoreKey(legacyTarget, L""); });
            auto count = models::Profiles().size();
            folia::platform::completion::CompletionConfig legacy;
            legacy.baseUrl = "https://migration-test.invalid/v1";
            legacy.protocol = static_cast<int>(models::ApiProtocol::Responses);
            legacy.model = "migration-test";
            legacy.reasoningEffort = "none";

            auto id = models::ImportLegacyCompletion(legacy, legacyTarget);
            auto [model, profile] = models::Resolve(id);
            auto eraseProfile = wil::scope_exit([&] { models::DeleteProfile(profile.id); });
            expect(models::Profiles().size() == count + 1);
            expect(!profile.fullUrl && profile.catalogUrl.empty());
            expect(model.upstream == legacy.model);
            expect(model.efforts == std::vector<std::string>{"none"});
            expect(!model.webSearch && !profile.allowWebSearch);
            expect(static_cast<bool>(models::ProfileKey(profile) == L"migration-test-key"));

            auto sameEndpoint = profile;
            sameEndpoint.providerType = "deepseek";
            expect(static_cast<bool>(models::ProfileKey(sameEndpoint) == L"migration-test-key"));
            sameEndpoint.baseUrl += "/another";
            expect(throws<std::runtime_error>([&] { models::ProfileKey(sameEndpoint); }));

            keys::StoreKey(legacyTarget, L"changed-legacy-key");
            expect(models::ImportLegacyCompletion(legacy, legacyTarget) == id);
            expect(models::Profiles().size() == count + 1);
            expect(static_cast<bool>(models::ProfileKey(profile) == L"migration-test-key"));
            expect(static_cast<bool>(keys::LoadKey(legacyTarget) == L"changed-legacy-key"));
        };

        "legacy AI settings link an existing model and preserve other settings"_test = [] {
            auto path = AssetPath(L"settings.json");
            if (!expect(!std::filesystem::exists(path))) return;
            models::ModelProfile draft{{}, "Existing connection", "https://migration-link.invalid/v1/responses",
                models::ApiProtocol::Responses};
            draft.fullUrl = true;
            draft.models.push_back({{}, "Existing model", {}, "existing-model", "high", false, {"high"}});
            auto profile = models::SaveProfile(std::move(draft), std::nullopt);
            auto cleanup = wil::scope_exit([&] {
                std::filesystem::remove(path);
                models::DeleteProfile(profile.id);
            });
            records::Write(path, {
                {"schemaVersion", 5}, {"themeId", "system"}, {"languageId", "system"},
                {"shortcutBindings", records::Json::array()}, {"autoHideDocumentTools", true},
                {"completion", {{"baseUrl", "https://migration-link.invalid"}, {"protocol", 1},
                    {"model", "existing-model"}, {"reasoningEffort", "high"}, {"enabled", true},
                    {"contextKB", 12}, {"outputTokens", 96}}},
                {"futureSetting", "preserved"}
            });

            auto count = models::Profiles().size();
            auto loaded = LoadAppSettings();
            auto raw = records::Read(path);
            expect(loaded.loadedFromFile && loaded.diagnostic.empty());
            expect(loaded.settings.appearance.autoHideTools && loaded.settings.completion.enabled);
            expect(loaded.settings.completion.contextKB == 12_u);
            expect(loaded.settings.completion.outputTokens == 96_u);
            expect(loaded.settings.completionModelId == profile.models.front().id);
            expect(raw.value("futureSetting", std::string{}) == "preserved");
            expect(raw.at("schemaVersion") == 6);
            expect(!raw.at("completion").contains("baseUrl") && !raw.at("completion").contains("model") &&
                   !raw.at("completion").contains("protocol"));
            expect(raw.at("completion").value("modelId", std::string{}) == profile.models.front().id);
            expect(static_cast<bool>(LoadAppSettings().settings == loaded.settings));
            expect(models::Profiles().size() == count);
        };
    };
}
