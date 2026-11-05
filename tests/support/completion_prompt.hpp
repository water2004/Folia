#pragma once
#include <nlohmann/json.hpp>
import folia.completion;

namespace completion_tests {
inline folia::completion::PromptSettings const& PromptSettings() {
    static auto const settings = [] {
        auto path = std::filesystem::path{__FILE__}.parent_path().parent_path().parent_path()
            / "src/app-winui/Assets/completion.json";
        std::ifstream input(path);
        auto json = nlohmann::json::parse(input);
        auto const& policies = json.at("policies");
        folia::completion::PromptSettings settings{json.at("instructions").get<std::string>(),
            {policies.at("conservative").get<std::string>(), policies.at("balanced").get<std::string>(),
                policies.at("proactive").get<std::string>()}, json.at("documentPrefix").get<std::string>()};
        settings.region_name = json.at("regionName").get<std::string>();
        settings.before_edit = json.at("beforeEdit").get<std::string>();
        settings.after_edit = json.at("afterEdit").get<std::string>();
        for (auto const& example : json.value("examples", nlohmann::json::array()))
            settings.examples.push_back({example.at("before").get<std::string>(), example.at("editable").get<std::string>(),
                example.at("after").get<std::string>(), example.at("response").get<std::string>()});
        return settings;
    }();
    return settings;
}
inline folia::completion::Prompt MakePrompt(folia::completion::Context const& context,
    folia::completion::Budget budget = {}, folia::completion::Eagerness level = folia::completion::Eagerness::Balanced) {
    return folia::completion::make_prompt(context, PromptSettings(), budget, level);
}
}
