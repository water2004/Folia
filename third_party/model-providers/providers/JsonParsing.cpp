#include "JsonParsing.h"

#include <set>

namespace fairy::providers {
namespace {
constexpr std::size_t kMaxJsonBytes = 12u * 1024u * 1024u;
}

Json ParseJson(std::string_view text) {
    if (text.empty() || text.size() > kMaxJsonBytes)
        throw ProtocolError("JSON size limit exceeded");
    std::vector<std::set<std::string>> keys;
    try {
        return Json::parse(text, [&keys](int depth, Json::parse_event_t event, Json& parsed) {
            if (depth > 64)
                throw ProtocolError("JSON nesting limit exceeded");
            if (event == Json::parse_event_t::object_start)
                keys.emplace_back();
            else if (event == Json::parse_event_t::object_end)
                keys.pop_back();
            else if (event == Json::parse_event_t::key &&
                     !keys.back().insert(parsed.get<std::string>()).second)
                throw ProtocolError("Duplicate JSON field");
            return true;
        });
    } catch (Json::exception const&) {
        throw ProtocolError("Malformed JSON");
    }
}

} // namespace fairy::providers
