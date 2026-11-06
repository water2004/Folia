// Mermaid 12.2.0 Ishikawa grammar and DB projection. MIT; see ../LICENSE.
export module folia.mermaid.ishikawa;
import std;
export namespace folia::mermaid {
struct IshikawaNode { std::u16string text; std::vector<IshikawaNode> children; };
struct IshikawaDocument { IshikawaNode root; };
std::expected<IshikawaDocument,std::string> parse_ishikawa(std::u16string_view source);
}
