// Folia's native Packet/Pie source-to-render boundary. Mermaid 12.2.0, MIT.
export module folia.mermaid.render_entry;
import std;
export import folia.mermaid.frontend;
export import folia.mermaid.packet_svg;
export import folia.mermaid.pie_svg;

export namespace folia::mermaid {
struct PacketRenderInput { PacketLayout layout; PacketSvgTheme theme; };
struct PieRenderInput { PieDocument document; PieSvgTheme theme; PieSvgConfig config; };
struct NativeDiagram {
    PreparedDiagram prepared;
    std::variant<PacketRenderInput, PieRenderInput> content;
};
struct DiagramSvg { std::string svg; double width{}, height{}; };

// Resolves supported family configuration and parses the prepared source.
// Family font lists remain unresolved until the platform selects an installed
// font. No browser HTML/DOM sanitizer is emulated: metadata must be plain text.
std::expected<NativeDiagram, std::string> prepare_native_diagram(
    std::u16string_view source, const ConfigSession&, FrontendLimits limits = {});

// Decode parser-protected decimal/common named entity escapes for measurement.
// Parsing/deduplication retains the original spelling, as the official DB does.
std::expected<std::u16string, std::string> diagram_display_text(std::u16string_view);

// Measurement uses the selected Pie font and decoded final legend/title text.
// Packet does not need measured widths. Output includes plain accessibility
// metadata; no DOM id or event bindings are required by Folia's SVG consumer.
std::expected<DiagramSvg, std::string> render_native_diagram(
    const NativeDiagram&, const PieMeasurements& measurements = {});
}
