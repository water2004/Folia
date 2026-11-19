// Derived from @zenuml/core 3.49.2; MIT, see ../ZENUML-LICENSE.
export module folia.mermaid.zenuml;
import std;
export namespace folia::mermaid {
enum class ZenKind {
    sync,
    async,
    creation,
    returned,
    loop,
    alternative,
    optional,
    parallel,
    critical,
    section,
    try_catch
};
constexpr bool zen_fragment(ZenKind kind) {
    return kind >= ZenKind::loop;
}
struct ZenParticipant {
    std::u16string name, label, type;
};
struct ZenSection;
struct ZenStatement {
    ZenKind kind{};
    std::u16string from, to, text, label, assignment, condition, comment;
    std::vector<ZenStatement> children;
    std::vector<ZenSection> sections;
    std::size_t begin{}, end{};
    bool body{};
};
struct ZenSection {
    std::u16string label;
    std::vector<ZenStatement> statements;
};
struct ZenDocument {
    std::u16string title;
    std::vector<ZenParticipant> participants;
    std::vector<ZenStatement> statements;
};
struct ZenCoordinate {
    std::size_t begin{}, end{};
    ZenKind kind{};
    double top{}, height{};
};
struct ZenVerticalLayout {
    std::vector<ZenCoordinate> coordinates;
    double height{};
};
struct ZenLayout {
    std::vector<double> positions, widths;
    std::vector<std::vector<double>> constraints;
    std::vector<ZenCoordinate> vertical;
    double height{}, width{};
};
using ZenMeasure = std::function<double(std::u16string_view)>;
std::expected<ZenDocument, std::string> parse_zenuml(std::u16string_view);
ZenVerticalLayout layout_zenuml_vertical(const ZenDocument &);
std::expected<ZenLayout, std::string> layout_zenuml(const ZenDocument &, const ZenMeasure &);
} // namespace folia::mermaid
