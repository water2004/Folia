#pragma once

namespace winrt::Folia::code_language
{
    struct Choice { std::string_view name; std::string_view aliases; };
    inline constexpr Choice choices[] = {
        {"text", "plain plaintext txt"}, {"cpp", "c++ cc cxx hpp"}, {"c", "h"},
        {"csharp", "c# cs"}, {"java", ""}, {"javascript", "js jsx node"},
        {"typescript", "ts"}, {"python", "py"}, {"rust", "rs"}, {"go", "golang"},
        {"json", ""}, {"jsonc", ""}, {"yaml", "yml"}, {"html", "htm"},
        {"css", ""}, {"shell", "sh bash zsh"}, {"powershell", "ps1 pwsh"},
        {"sql", ""}, {"markdown", "md"}, {"mermaid", ""}
    };

    inline std::string Fold(std::string_view value)
    {
        std::string result(value);
        for (auto& ch : result)
            if (ch >= 'A' && ch <= 'Z') ch += 'a' - 'A';
        return result;
    }

    // A fence language is one token. Do not silently truncate pasted input.
    inline std::optional<std::string> Parse(std::string_view value)
    {
        auto first = value.find_first_not_of(" \t");
        if (first == value.npos) return std::string{};
        value = value.substr(first, value.find_last_not_of(" \t") - first + 1);
        if (value.find_first_of(" \t\r\n`") != value.npos) return std::nullopt;
        for (unsigned char ch : value) if (ch < 0x20 || ch == 0x7f) return std::nullopt;
        return std::string(value);
    }

    inline std::vector<std::string> Filter(std::string_view query)
    {
        auto folded = Fold(query);
        std::vector<std::string> result;
        for (auto const& choice : choices)
            if (choice.name.find(folded) != choice.name.npos
                || (!folded.empty() && choice.aliases.find(folded) != choice.aliases.npos))
                result.emplace_back(choice.name);
        return result;
    }
}
