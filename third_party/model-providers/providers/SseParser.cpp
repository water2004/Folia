#include "SseParser.h"
#include "JsonParsing.h"
namespace fairy::providers {
std::vector<SseEvent> SseParser::Feed(std::string_view bytes) {
    if (pending_.size() + bytes.size() > 1024u * 1024u)
        throw ProtocolError("SSE frame limit exceeded");
    pending_.append(bytes);
    std::vector<SseEvent> events;
    std::size_t start{};
    while (true) {
        auto end = pending_.find('\n', start);
        if (end == std::string::npos)
            break;
        auto line = std::string_view(pending_).substr(start, end - start);
        start = end + 1;
        if (!line.empty() && line.back() == '\r')
            line.remove_suffix(1);
        eventBytes_ += line.size();
        if (eventBytes_ > 1024u * 1024u)
            throw ProtocolError("SSE event limit exceeded");
        if (line.empty()) {
            if (hasData_) {
                if (!current_.data.empty())
                    current_.data.pop_back();
                events.push_back(std::move(current_));
            }
            current_ = {};
            eventBytes_ = 0;
            hasData_ = false;
            continue;
        }
        if (line.front() == ':')
            continue;
        auto colon = line.find(':');
        auto field = line.substr(0, colon);
        auto value = colon == std::string_view::npos ? std::string_view{} : line.substr(colon + 1);
        if (!value.empty() && value.front() == ' ')
            value.remove_prefix(1);
        if (field == "data") {
            current_.data.append(value);
            current_.data += '\n';
            hasData_ = true;
        } else if (field == "event")
            current_.event.assign(value);
        else if (field == "id") {
            if (value.find('\0') != std::string_view::npos)
                throw ProtocolError("Invalid SSE identity");
            current_.id.assign(value);
        }
    }
    pending_.erase(0, start);
    return events;
}
void SseParser::Finish() const {
    if (!pending_.empty() || hasData_ || eventBytes_ != 0)
        throw ProtocolError("Truncated SSE frame", ProtocolFailure::IncompleteStream);
}
} // namespace fairy::providers
