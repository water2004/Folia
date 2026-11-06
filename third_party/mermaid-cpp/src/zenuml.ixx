// Derived from @zenuml/core 3.49.2; MIT, see ../ZENUML-LICENSE.
export module folia.mermaid.zenuml;
import std;
export namespace folia::mermaid {
enum class ZenKind { sync, async, creation, returned, loop, alternative };
struct ZenParticipant { std::u16string name, label, type; };
struct ZenStatement { ZenKind kind{}; std::u16string from,to,text,label,assignment,condition; std::vector<ZenStatement> children,otherwise; std::size_t begin{},end{}; bool body{}; };
struct ZenDocument { std::u16string title; std::vector<ZenParticipant> participants; std::vector<ZenStatement> statements; };
struct ZenCoordinate { std::size_t begin{},end{}; ZenKind kind{}; double top{},height{}; };
struct ZenLayout { std::vector<double> positions,widths; std::vector<std::vector<double>> constraints; std::vector<ZenCoordinate> vertical; double height{},width{}; };
using ZenMeasure=std::function<double(std::u16string_view)>;
std::expected<ZenDocument,std::string> parse_zenuml(std::u16string_view);
std::expected<ZenLayout,std::string> layout_zenuml(const ZenDocument&,const ZenMeasure&);
}
