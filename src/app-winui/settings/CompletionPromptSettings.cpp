#include "pch.h"
#include "settings/CompletionPromptSettings.h"
#include "storage/AssetPaths.h"

namespace winrt::Folia {
folia::completion::PromptSettings ParseCompletionPromptSettings(Windows::Data::Json::JsonObject const& json) {
    auto policies = json.GetNamedObject(L"policies");
    folia::completion::PromptSettings settings{
        winrt::to_string(json.GetNamedString(L"instructions")),
        {winrt::to_string(policies.GetNamedString(L"conservative")),
            winrt::to_string(policies.GetNamedString(L"balanced")),
            winrt::to_string(policies.GetNamedString(L"proactive"))},
        winrt::to_string(json.GetNamedString(L"documentPrefix"))};
    settings.region_name = winrt::to_string(json.GetNamedString(L"regionName"));
    settings.before_edit = winrt::to_string(json.GetNamedString(L"beforeEdit"));
    settings.after_edit = winrt::to_string(json.GetNamedString(L"afterEdit"));
    if (json.HasKey(L"examples")) {
        auto examples = json.GetNamedArray(L"examples");
        if (examples.Size() > 4) throw std::invalid_argument("Completion prompt has too many examples.");
        for (auto const& value : examples) {
            auto example = value.GetObject();
            settings.examples.push_back({winrt::to_string(example.GetNamedString(L"before")),
                winrt::to_string(example.GetNamedString(L"editable")), winrt::to_string(example.GetNamedString(L"after")),
                winrt::to_string(example.GetNamedString(L"response"))});
        }
    }
    if (auto error = folia::completion::validate_prompt_settings(settings)) throw std::invalid_argument(*error);
    return settings;
}
folia::completion::PromptSettings LoadCompletionPromptSettings() {
    static auto const defaults = LoadCompletionPromptSettings(AssetPath(L"completion.json"));
    return defaults;
}
folia::completion::PromptSettings LoadCompletionPromptSettings(std::filesystem::path const& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw std::runtime_error("Cannot read completion prompt settings.");
    std::string text(32769, '\0');
    stream.read(text.data(), static_cast<std::streamsize>(text.size()));
    text.resize(static_cast<std::size_t>(stream.gcount()));
    if (text.size() > 32768 || stream.bad()) throw std::runtime_error("Completion prompt settings exceed their size limit or cannot be read.");
    return ParseCompletionPromptSettings(Windows::Data::Json::JsonObject::Parse(winrt::to_hstring(text)));
}
Windows::Data::Json::JsonObject SerializeCompletionPromptSettings(folia::completion::PromptSettings const& settings) {
    using namespace Windows::Data::Json;
    if (auto error = folia::completion::validate_prompt_settings(settings)) throw std::invalid_argument(*error);
    JsonObject json, policies;
    json.Insert(L"instructions", JsonValue::CreateStringValue(winrt::to_hstring(settings.instructions)));
    std::array<wchar_t const*, 3> names{L"conservative", L"balanced", L"proactive"};
    for (std::size_t i = 0; i < names.size(); ++i)
        policies.Insert(names[i], JsonValue::CreateStringValue(winrt::to_hstring(settings.policies[i])));
    json.Insert(L"policies", policies);
    json.Insert(L"documentPrefix", JsonValue::CreateStringValue(winrt::to_hstring(settings.document_prefix)));
    json.Insert(L"regionName", JsonValue::CreateStringValue(winrt::to_hstring(settings.region_name)));
    json.Insert(L"beforeEdit", JsonValue::CreateStringValue(winrt::to_hstring(settings.before_edit)));
    json.Insert(L"afterEdit", JsonValue::CreateStringValue(winrt::to_hstring(settings.after_edit)));
    JsonArray examples;
    for (auto const& entry : settings.examples) {
        JsonObject example;
        example.Insert(L"before", JsonValue::CreateStringValue(winrt::to_hstring(entry.before)));
        example.Insert(L"editable", JsonValue::CreateStringValue(winrt::to_hstring(entry.editable)));
        example.Insert(L"after", JsonValue::CreateStringValue(winrt::to_hstring(entry.after)));
        example.Insert(L"response", JsonValue::CreateStringValue(winrt::to_hstring(entry.response)));
        examples.Append(example);
    }
    json.Insert(L"examples", examples);
    return json;
}
}
