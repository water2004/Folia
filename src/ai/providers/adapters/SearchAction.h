#pragma once
#include "../ModelTypes.h"

namespace folia::ai {
inline std::string WebOperation(Json const& item, char const* member) {
    const auto pageKind = [](Json const& value) {
        return value == "web_fetch" || value == "deepseek_read_url" || value == "read_link" ||
               value == "open_page" || value == "find_in_page" || value == "browse_page";
    };
    for (auto key : {"name", "kind", "type"})
        if (auto it = item.find(key); it != item.end() && pageKind(*it)) return "web_fetch";
    if (auto action = item.find(member); action != item.end() && action->is_object())
        for (auto key : {"type", "action"})
            if (auto it = action->find(key); it != action->end() && pageKind(*it)) return "web_fetch";
    return "web_search";
}
inline void AddPageSource(ModelResult& result, Json const& page) {
    if (!page.is_object() || result.citations.size() >= 64) return;
    auto url = page.value("url", "");
    if (!url.starts_with("https://") && !url.starts_with("http://")) return;
    for (auto const& citation : result.citations) if (citation.url == url) return;
    auto title = page.find("title");
    result.citations.push_back({title != page.end() && title->is_string() ? title->get<std::string>() : "", std::move(url)});
}
inline std::string SearchTarget(Json const& item, char const* member) {
    auto action = item.find(member);
    if (action == item.end() || !action->is_object()) return {};
    for (auto field : {"query", "url"}) {
        auto text = action->find(field);
        if (text != action->end() && text->is_string() && !text->get_ref<std::string const&>().empty())
            return text->get<std::string>();
    }
    std::string result;
    auto queries = action->find("queries");
    if (queries != action->end() && queries->is_array()) {
        for (auto const& query : *queries) {
            if (!query.is_string()) continue;
            if (!result.empty()) result += " · ";
            result += query.get_ref<std::string const&>();
        }
    }
    return result;
}
}
