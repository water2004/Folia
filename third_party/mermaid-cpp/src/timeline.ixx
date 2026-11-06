// Mermaid 12.2.0 Timeline parser and DB. MIT; see ../LICENSE.
export module folia.mermaid.timeline;
export import folia.mermaid.frontend;
import std;
export namespace folia::mermaid {
struct TimelineTask {std::u16string section,text;std::vector<std::u16string> events;};
struct TimelineDocument {std::u16string title,acc_title,acc_description,direction=u"LR";std::vector<std::u16string> sections;std::vector<TimelineTask> tasks;};
std::expected<TimelineDocument,std::string> parse_timeline(std::u16string_view);
}
