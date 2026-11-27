module folia.ai.agent.tool_output;
import std;

namespace folia::ai::agent {
    namespace {
        std::string Encode(Json const& value) {
            return value.dump(-1, ' ', false, Json::error_handler_t::replace);
        }

        std::string Utf8Text(Json const& value) {
            // A workspace snippet can contain malformed UTF-8. Apply the same
            // replacement policy as metadata without exposing JSON escapes to the model.
            return Json::parse(Encode(value)).get<std::string>();
        }

        std::size_t FenceLength(std::string_view text, char marker) {
            std::size_t longest = 2, run = 0;
            for (char c : text) {
                run = c == marker ? run + 1 : 0;
                longest = std::max(longest, run);
            }
            return longest + 1;
        }

        std::string Preview(Json const& body) {
            std::string display;
            for (auto key : {"error", "text"}) {
                if (auto it = body.find(key); it != body.end() && it->is_string())
                    display = it->get<std::string>();
            }
            for (auto key : {"matches", "headings", "entries", "tabs"}) {
                auto items = body.find(key);
                if (items == body.end() || !items->is_array()) continue;
                for (auto const& item : *items) {
                    for (auto field : {"snippet", "heading", "name", "title"}) {
                        auto it = item.find(field);
                        if (it == item.end() || !it->is_string()) continue;
                        if (!display.empty()) display += '\n';
                        if (item.contains("offset")) display += "@" + item.at("offset").dump() + "  ";
                        display += it->get_ref<std::string const&>();
                    }
                    if (display.size() > 1024) break;
                }
            }
            if (display.empty()) display = Encode(body);
            if (display.size() > 1024) {
                std::size_t end = 1024;
                while (end && (static_cast<unsigned char>(display[end]) & 0xc0) == 0x80) --end;
                display.resize(end);
                display += "…";
            }
            return Utf8Text(Json(std::move(display)));
        }

        std::string Retained(Json const& body) {
            Json fields{{"omitted", "Earlier tool output was trimmed. Read the relevant range again if needed."}};
            for (auto key : {"link", "next_offset", "next_before", "total_bytes"})
                if (auto value = body.find(key); value != body.end()) fields[key] = *value;
            return Encode(fields);
        }
    }

    void AppendVerbatim(std::string& destination, std::string_view label, std::string_view text) {
        const auto backticks = FenceLength(text, '`');
        const auto tildes = FenceLength(text, '~');
        const auto fence = backticks <= tildes ? std::string(backticks, '`') : std::string(tildes, '~');
        if (!destination.empty()) destination += "\n\n";
        destination += label;
        destination += '\n';
        destination += fence + "text\n";
        destination += text;
        if (!text.ends_with('\n')) destination += '\n';
        destination += fence;
    }

    ToolOutput FormatToolOutput(Json body) {
        ToolOutput output{.preview = Preview(body), .failed = body.contains("error")};
        std::string excerpts;
        auto extract = [&](Json& object, char const* key, std::string_view label) {
            auto value = object.find(key);
            if (value == object.end() || !value->is_string()) return;
            AppendVerbatim(excerpts, label, Utf8Text(*value));
            object.erase(value);
        };
        extract(body, "error", "Tool error");
        extract(body, "text", "Text");
        for (auto [key, field] : {std::pair{"matches", "snippet"}, std::pair{"headings", "heading"}}) {
            auto items = body.find(key);
            if (items == body.end() || !items->is_array()) continue;
            for (std::size_t i = 0; i < items->size(); ++i) {
                auto label = std::string(key) + "[" + std::to_string(i) + "]." + field;
                extract((*items)[i], field, label);
            }
        }
        if (excerpts.empty()) output.text = Encode(body);
        else if (body.empty()) output.text = std::move(excerpts);
        else output.text = "Metadata (JSON)\n" + Encode(body) + "\n\n" + excerpts;
        if (output.text.size() >= 1024) {
            auto retained = Retained(body);
            if (retained.size() < output.text.size()) output.retained = std::move(retained);
        }
        return output;
    }
}
