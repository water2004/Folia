#include "pch.h"
#include "support/folia_test.hpp"
#include "settings/CompletionPromptSettings.h"

namespace {
using namespace boost::ut;
using namespace winrt::Folia;
suite completion_settings_tests = [] {
    "completion prompt settings retain custom instructions policies and examples on save"_test = [] {
        auto settings = LoadCompletionPromptSettings();
        settings.instructions += " Custom 中文 ${policy}.";
        settings.policies[0] = "Custom conservative rule.";
        settings.examples = {{"he", "lo", " world", "<insert>l</insert>"}};
        auto json = SerializeCompletionPromptSettings(settings);
        auto reloaded = ParseCompletionPromptSettings(json);
        expect(reloaded == settings);
        auto first = folia::completion::make_prompt({{}, "$", "$"}, reloaded);
        expect(first.instructions.contains("Custom 中文"));
        expect(first.examples.size() == 1_u);
        expect(first.examples[0].second == "<insert>l</insert>");
        auto invalid = json; invalid.Insert(L"instructions", winrt::Windows::Data::Json::JsonValue::CreateStringValue(L"no parameters"));
        expect(throws([&] { (void)ParseCompletionPromptSettings(invalid); }));
    };
    "completion prompt files are editable data with bounded reads and no compiled fallback"_test = [] {
        auto root = std::filesystem::path{__FILE__}.parent_path().parent_path().parent_path()
            / "build/tests/completion-settings";
        std::filesystem::create_directories(root);
        auto path = root / "prompt.json";
        auto settings = LoadCompletionPromptSettings();
        auto write = [&](std::string const& text) { std::ofstream file(path, std::ios::binary); file << text; };
        write(winrt::to_string(SerializeCompletionPromptSettings(settings).Stringify()));
        expect(LoadCompletionPromptSettings(path) == settings);
        settings.instructions += " Read from file.";
        write(winrt::to_string(SerializeCompletionPromptSettings(settings).Stringify()));
        expect(LoadCompletionPromptSettings(path) == settings);
        write(std::string(32769, ' '));
        expect(throws([&] { (void)LoadCompletionPromptSettings(path); }));
        write("invalid JSON");
        expect(throws([&] { (void)LoadCompletionPromptSettings(path); }));
        expect(throws([&] { (void)LoadCompletionPromptSettings(root / "absent.json"); }));
    };
};
}
