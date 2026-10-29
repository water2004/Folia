// Mermaid 12.2.0 Journey parser and DB. MIT; see ../LICENSE.
export module folia.mermaid.journey;
export import folia.mermaid.frontend;
import std;
export namespace folia::mermaid {
struct JourneyTask {std::u16string section,text;double score=0;std::vector<std::u16string> people;};
struct JourneyDocument {std::u16string title,acc_title,acc_description;std::vector<std::u16string> sections,actors;std::vector<JourneyTask> tasks;};
std::expected<JourneyDocument,std::string> parse_journey(std::u16string_view);
}
