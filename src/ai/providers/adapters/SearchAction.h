#pragma once
#include "../ModelTypes.h"

namespace folia::ai {
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
