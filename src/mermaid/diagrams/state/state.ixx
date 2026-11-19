// Mermaid 12.2.0 State parser/DB. MIT; see ../LICENSE.
export module folia.mermaid.state;
export import folia.mermaid.flowchart;
import std;
export namespace folia::mermaid {
// State retains its family shape names; the renderer projects measured boxes
// into the shared graph layout without translating source into Flowchart text.
struct StateAnnotation { std::u16string state, text, position; };
struct StateParseResult { FlowchartInput document; std::vector<StateAnnotation> annotations; };
std::expected<StateParseResult,std::string> parse_state_document(std::u16string_view);
std::expected<FlowchartInput,std::string> parse_state(std::u16string_view);
}
