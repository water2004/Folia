// Mermaid 12.2.0 Cynefin beta typed model and parser. MIT; see ../LICENSE.
export module folia.mermaid.cynefin;
import std;
export namespace folia::mermaid {
enum class CynefinDomainName { complex, complicated, clear, chaotic, confusion };
struct CynefinItem { std::u16string label; };
struct CynefinDomain { CynefinDomainName name{}; std::vector<CynefinItem> items; };
struct CynefinTransition { CynefinDomainName from{}, to{}; std::optional<std::u16string> label; };
struct CynefinDocument {
    std::u16string title, acc_title, acc_description;
    std::vector<CynefinDomain> domains;
    std::vector<CynefinTransition> transitions;
};
std::expected<CynefinDocument,std::string> parse_cynefin(std::u16string_view source);
}
