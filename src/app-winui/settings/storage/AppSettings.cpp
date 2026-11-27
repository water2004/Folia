#include "pch.h"
#include "settings/storage/ModelCatalog.h"
#include "settings/storage/AppSettings.h"
#include "settings/storage/ShortcutSettingsSerialization.h"
#include "settings/storage/CompletionPromptSettings.h"
#include "localization/Localization.h"
#include "windows/media/mermaid/MermaidRenderer.h"
#include "storage/AssetPaths.h"
#include "storage/JsonRecords.h"

namespace
{
    using namespace winrt::Windows::Data::Json;

    class SettingsLock
    {
        wil::unique_mutex mutex_;
        wil::mutex_release_scope_exit locked_;

    public:
        SettingsLock()
        {
            mutex_.create(L"Local\\Folia.AppSettings");
            locked_ = mutex_.acquire(nullptr, 2000);
            if (!locked_) throw std::runtime_error("Folia settings are busy.");
        }
    };

    std::string ReadUtf8(std::filesystem::path const& path)
    {
        std::ifstream stream(path, std::ios::binary);
        if (!stream) throw std::runtime_error("cannot open settings file");
        return { std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>() };
    }

    void WriteSettings(std::filesystem::path const& path, JsonObject const& root)
    {
        std::filesystem::create_directories(path.parent_path());
        auto temporary = path.parent_path() / (winrt::Folia::records::NewId() + ".tmp");
        auto cleanup = wil::scope_exit([&] { ::DeleteFileW(temporary.c_str()); });
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        stream << winrt::to_string(root.Stringify());
        stream.close();
        if (!stream) throw std::runtime_error("cannot write settings file");
        if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            winrt::throw_last_error();
    }

    winrt::hstring MigrateCompletionModel(JsonObject const& root, std::filesystem::path const& path, winrt::Folia::AppSettings& settings)
    {
        using namespace winrt::Folia;
        try {
            auto completion = root.GetNamedObject(L"completion");
            auto legacy = settings.completion;
            legacy.baseUrl = winrt::to_string(completion.GetNamedString(L"baseUrl", L""));
            legacy.model = winrt::to_string(completion.GetNamedString(L"model", L""));
            if (legacy.baseUrl.empty() || legacy.model.empty()) return {};
            auto protocol = completion.GetNamedNumber(L"protocol", 0);
            if (protocol != 0 && protocol != 1 && protocol != 2)
                throw std::invalid_argument("Invalid legacy AI protocol.");
            legacy.protocol = static_cast<int>(protocol);
            legacy.fullUrl = completion.GetNamedBoolean(L"fullUrl", false);
            legacy.catalogUrl = winrt::to_string(completion.GetNamedString(L"catalogUrl", L""));

            auto modelId = models::ImportLegacyCompletion(legacy, L"Folia/InlineCompletion/APIKey");
            completion.Insert(L"modelId", JsonValue::CreateStringValue(winrt::to_hstring(modelId)));
            WriteSettings(path, root);
            settings.completionModelId = std::move(modelId);
            return {};
        }
        catch (winrt::hresult_error const& error) {
            return LocalizeFormat(L"AIModelMigrationFailed", {error.message()});
        }
        catch (std::exception const& error) {
            return LocalizeFormat(L"AIModelMigrationFailed", {winrt::to_hstring(error.what())});
        }
    }
}

namespace winrt::Folia
{
    SettingsLoadResult LoadAppSettings()
    {
        try
        {
            auto path = AssetPath(L"settings.json");
            if (!std::filesystem::exists(path)) return {};
            SettingsLock lock;
            auto root = Windows::Data::Json::JsonObject::Parse(winrt::to_hstring(ReadUtf8(path)));
            auto schemaVersion = root.GetNamedNumber(L"schemaVersion");
            if (schemaVersion != 1.0 && schemaVersion != 2.0
                && schemaVersion != 3.0 && schemaVersion != 4.0 && schemaVersion != 5.0 && schemaVersion != 6.0)
                throw std::runtime_error("unsupported settings schema version");
            AppSettings settings;
            if (schemaVersion >= 5.0) {
                auto completion = root.GetNamedObject(L"completion");
                settings.completion.enabled = completion.GetNamedBoolean(L"enabled", false);
                settings.completionModelId = winrt::to_string(completion.GetNamedString(L"modelId", L""));
                settings.completion.reasoningEffort = winrt::to_string(completion.GetNamedString(L"reasoningEffort", L""));
                settings.completion.eagerness = static_cast<folia::completion::Eagerness>(static_cast<int>(completion.GetNamedNumber(L"eagerness", 1)));
                settings.completion.contextKB = static_cast<std::uint32_t>(completion.GetNamedNumber(L"contextKB", 7));
                settings.completion.outputTokens = static_cast<std::uint32_t>(completion.GetNamedNumber(L"outputTokens", 128));
                settings.completion.explicitPromptCache = completion.GetNamedBoolean(L"explicitPromptCache", false);
                try {
                    if (completion.HasKey(L"prompt"))
                        settings.completion.prompt = ParseCompletionPromptSettings(completion.GetNamedObject(L"prompt"));
                } catch (winrt::hresult_error const& error) { settings.completion.promptError = winrt::to_string(error.message()); }
                catch (std::exception const& error) { settings.completion.promptError = error.what(); }
            }
            if (schemaVersion >= 4.0)
                settings.latexSuggestionsEnabled = root.GetNamedBoolean(L"latexSuggestionsEnabled", true);
            settings.themeId = winrt::to_string(root.GetNamedString(L"themeId"));
            settings.openFilesInNewWindow = root.GetNamedBoolean(L"openFilesInNewWindow", false);
            settings.appearance.autoHideTools = root.GetNamedBoolean(L"autoHideDocumentTools", false);
            auto material = root.GetNamedString(L"windowMaterial", L"");
            auto reading = root.GetNamedString(L"readingMaterial",
                root.GetNamedBoolean(L"materialInDocument", true) ? L"window" : L"solid");
            for (auto const& choice : windowMaterials) {
                if (choice.id == std::wstring_view(material)) settings.appearance.material = choice.value;
                if (choice.id == std::wstring_view(reading)) settings.appearance.readingMaterial = choice.value;
            }
            auto imageWidth = root.GetNamedNumber(L"imageMaxWidthPercent", settings.media.imageMaxWidthPercent);
            auto const& widths = MediaDisplaySettings::imageWidths;
            if (auto width = std::find(widths.begin(), widths.end(), imageWidth); width != widths.end())
                settings.media.imageMaxWidthPercent = *width;
            auto mermaidTheme = winrt::to_string(root.GetNamedString(L"mermaidTheme", L""));
            if (!mermaidTheme.empty()) {
                auto names = folia::platform::MermaidRenderer::ThemeNames();
                if (std::ranges::find(names, mermaidTheme) != names.end())
                    settings.media.mermaidTheme = std::move(mermaidTheme);
            }
            if (schemaVersion >= 2.0)
                settings.languageId = winrt::to_string(root.GetNamedString(L"languageId"));
            if (schemaVersion >= 3.0)
                settings.shortcutBindings = ParseShortcutSettings(root.GetNamedArray(L"shortcutBindings"));
            if (settings.themeId.empty()) throw std::runtime_error("themeId cannot be empty");
            if (!IsSupportedLanguage(settings.languageId))
                throw std::runtime_error("unsupported languageId");
            auto diagnostic = schemaVersion == 5.0 && settings.completionModelId.empty()
                ? MigrateCompletionModel(root, path, settings) : hstring{};
            if (settings.completionModelId.empty()) settings.completion.enabled = false;
            return { std::move(settings), true, std::move(diagnostic) };
        }
        catch (winrt::hresult_error const& error)
        {
            return { {}, false, LocalizeFormat(L"SettingsFallback", { error.message() }) };
        }
        catch (std::exception const& error)
        {
            return { {}, false, LocalizeFormat(L"SettingsFallback", { winrt::to_hstring(error.what()) }) };
        }
    }

    std::optional<winrt::hstring> SaveAppSettings(AppSettings const& settings)
    {
        try
        {
            if (!settings.completion.promptError.empty()) throw std::invalid_argument(settings.completion.promptError);
            SettingsLock lock;
            Windows::Data::Json::JsonObject root;
            root.Insert(L"schemaVersion", Windows::Data::Json::JsonValue::CreateNumberValue(6));
            Windows::Data::Json::JsonObject completion;
            completion.Insert(L"enabled", Windows::Data::Json::JsonValue::CreateBooleanValue(settings.completion.enabled));
            completion.Insert(L"modelId", Windows::Data::Json::JsonValue::CreateStringValue(winrt::to_hstring(settings.completionModelId)));
            if (!settings.completion.reasoningEffort.empty())
                completion.Insert(L"reasoningEffort", Windows::Data::Json::JsonValue::CreateStringValue(winrt::to_hstring(settings.completion.reasoningEffort)));
            completion.Insert(L"eagerness", Windows::Data::Json::JsonValue::CreateNumberValue(static_cast<int>(settings.completion.eagerness)));
            completion.Insert(L"contextKB", Windows::Data::Json::JsonValue::CreateNumberValue(settings.completion.contextKB));
            completion.Insert(L"outputTokens", Windows::Data::Json::JsonValue::CreateNumberValue(settings.completion.outputTokens));
            completion.Insert(L"explicitPromptCache", Windows::Data::Json::JsonValue::CreateBooleanValue(settings.completion.explicitPromptCache));
            if (settings.completion.prompt)
                completion.Insert(L"prompt", SerializeCompletionPromptSettings(*settings.completion.prompt));
            root.Insert(L"completion", completion);
            root.Insert(L"latexSuggestionsEnabled", Windows::Data::Json::JsonValue::CreateBooleanValue(settings.latexSuggestionsEnabled));
            root.Insert(L"themeId", Windows::Data::Json::JsonValue::CreateStringValue(winrt::to_hstring(settings.themeId)));
            root.Insert(L"languageId", Windows::Data::Json::JsonValue::CreateStringValue(winrt::to_hstring(settings.languageId)));
            root.Insert(L"openFilesInNewWindow", Windows::Data::Json::JsonValue::CreateBooleanValue(settings.openFilesInNewWindow));
            root.Insert(L"autoHideDocumentTools", Windows::Data::Json::JsonValue::CreateBooleanValue(settings.appearance.autoHideTools));
            hstring reading = L"window";
            for (auto const& choice : windowMaterials) {
                if (choice.value == settings.appearance.material)
                    root.Insert(L"windowMaterial", Windows::Data::Json::JsonValue::CreateStringValue(hstring(choice.id)));
                if (choice.value == settings.appearance.readingMaterial) reading = hstring(choice.id);
            }
            root.Insert(L"readingMaterial", Windows::Data::Json::JsonValue::CreateStringValue(reading));
            root.Insert(L"imageMaxWidthPercent", Windows::Data::Json::JsonValue::CreateNumberValue(settings.media.imageMaxWidthPercent));
            root.Insert(L"mermaidTheme", Windows::Data::Json::JsonValue::CreateStringValue(winrt::to_hstring(settings.media.mermaidTheme)));
            root.Insert(L"shortcutBindings", SerializeShortcutSettings(settings.shortcutBindings));
            WriteSettings(AssetPath(L"settings.json"), root);
            return std::nullopt;
        }
        catch (winrt::hresult_error const& error)
        {
            return LocalizeFormat(L"UnableSaveSettings", { error.message() });
        }
        catch (std::exception const& error)
        {
            return LocalizeFormat(L"UnableSaveSettings", { winrt::to_hstring(error.what()) });
        }
    }
}
