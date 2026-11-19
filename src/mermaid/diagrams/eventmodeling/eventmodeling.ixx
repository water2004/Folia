// Mermaid 12.2.0 Event Modeling syntax. MIT; see ../LICENSE.
export module folia.mermaid.eventmodeling;
import std;
export namespace folia::mermaid {
struct EventmodelingFrame {
    std::u16string id, kind, entity, data, data_reference;
    std::vector<std::u16string> sources;
    bool reset = false, has_data = false, data_from_reference = false;
    std::size_t line = 0;
};
struct EventmodelingDiagram { std::u16string title, acc_title, acc_description; std::vector<EventmodelingFrame> frames; };
std::expected<EventmodelingDiagram,std::string> parse_eventmodeling(std::u16string_view source);
}
