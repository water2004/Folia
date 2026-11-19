module folia.mermaid.frontmatter;
import std;
import folia.mermaid.source_text;

namespace folia::mermaid {
std::expected<FrontMatterResult, std::string> extract_front_matter(
    std::u16string_view source, YamlLimits limits) {
    const auto match = match_front_matter(source);
    if (!match) return FrontMatterResult{std::u16string(source), {}};
    std::u16string body;
    std::u16string_view rest = match->body;
    while (true) {
        const auto end = rest.find(u'\n');
        auto line = rest.substr(0, end);
        if (line.starts_with(match->indent)) line.remove_prefix(match->indent.size());
        body += line;
        if (end == std::u16string_view::npos) break;
        body += u'\n';
        rest.remove_prefix(end + 1);
    }
    auto parsed = parse_config_yaml(body, limits);
    if (!parsed) return std::unexpected(parsed.error());
    FrontMatterResult result{std::u16string(source.substr(match->length)), {}};
    for (auto [key, destination] : {
            std::pair{u"title", &result.metadata.title},
            std::pair{u"displayMode", &result.metadata.display_mode}}) {
        const auto value = parsed->find(key);
        if (!value || !config_truthy(*value)) continue;
        auto text = config_to_string(*value);
        if (!text) return std::unexpected(text.error());
        *destination = std::move(*text);
    }
    if (const auto config = parsed->find(u"config"); config && config_truthy(*config)) {
        result.metadata.config = *config;
    }
    return result;
}
}
