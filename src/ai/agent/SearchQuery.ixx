export module folia.ai.agent.search_query;
import std;
import folia.ai.model;

export namespace folia::ai::agent {
    inline Json SearchQueryProperties() {
        return {
            {"query", {{"type", "string"}, {"description",
                R"(Exact text to find, with no regular expressions. Escape every literal backslash in JSON: {"query":"\\nu"} searches for the LaTeX command. Copy source excerpts as displayed, without adding escape characters to the text itself.)"}}},
            {"match_control_characters", {{"type", "boolean"}, {"description",
                "Set true only when the query intentionally includes a real newline, tab, or other control character."}}}
        };
    }

    inline std::expected<std::string_view, std::string_view> ReadSearchQuery(Json const& args) {
        auto query = args.find("query");
        if (query == args.end()) return std::string_view{};
        if (!query->is_string()) return std::unexpected("Search query must be a string.");

        const auto controls = args.find("match_control_characters");
        if (controls != args.end() && !controls->is_boolean())
            return std::unexpected("match_control_characters must be a boolean.");

        std::string_view text = query->get_ref<std::string const&>();
        if ((controls == args.end() || !controls->get<bool>()) &&
            std::ranges::any_of(text, [](unsigned char value) { return value < 0x20 || value == 0x7f; }))
            return std::unexpected(std::string_view{
                R"(The query contains a decoded control character. For a literal backslash, escape it in the JSON string: {"query":"\\nu"}. Set match_control_characters to true only if you intend to match a real newline, tab, or other control character.)"});
        return text;
    }
}
