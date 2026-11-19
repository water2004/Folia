export module folia.math.output.common:types;
export import folia.math.core.tree.factory_types;
export import folia.math.core.mml_tree.mml_document;
export import folia.math.core.dom_adaptor;
export import folia.math.util.style_json;
export import folia.math.util.styles;
export import folia.math.output.common.font_data;
export import folia.math.output.common.line_bbox;
import std;

export namespace folia::math::output::common {
class CommonOutputJax;
class CommonWrapper;
class CommonWrapperFactory;
class CommonMo;
class CommonTextNode;
class CommonMtable;
class Linebreaks;
class LayoutJob;

struct HtmlMeasurement {
    AbstractDOMAdaptor &adaptor;
    DOMNodeRef parent;
    DOMNodeRef container;
};

enum class InlineBreakPolicy : std::uint8_t { opportunistic, forced };

struct ExtendedMetrics {
    double em = 16;
    double ex = 8;
    double containerWidth = 1280;
    double scale = 1;
    std::string family;
    bool display = false;
};

struct LinebreakOptions {
    bool inlineBreaks = true;
    std::string width = "100%";
    double lineleading = .2;
};

struct UnknownBBox {
    double w = 0, h = 0, d = 0;
};

struct MetricRequest {
    std::string family;
    bool display = false;
    bool includeFamily = true;
};

struct UnknownTextRequest {
    std::u16string text;
    std::string variant;
    CssFontData font;
};

struct CommonOptions {
    double scale = 1;
    double minScale = .5;
    double exFactor = .5;
    bool mtextInheritFont = false;
    bool merrorInheritFont = false;
    std::string mtextFont;
    std::string merrorFont = "serif";
    bool mathmlSpacing = false;
    std::vector<std::string> skipAttributes;
    std::string displayAlign = "center";
    std::u16string displayIndent = u"0";
    std::string displayOverflow = "overflow";
    LinebreakOptions linebreaks;
    enum class HtmlHDW : std::uint8_t { automatic, use, force, ignore };
    HtmlHDW htmlHDW = HtmlHDW::automatic;
    // Glyph outline stroke in ems: MathJax 4.1.3 SVG's default blacker=3
    // uses a 1000-unit em. Zero explicitly disables glyph thickening.
    double glyphStrokeWidth = .003;
};

struct CommonOutputJaxConfig {
    ExtendedMetrics metrics;
    CommonOptions options;
    FontRepository const &fonts;
    std::string faceName;
    InlineBreakPolicy inlineBreakPolicy = InlineBreakPolicy::opportunistic;
    std::optional<HtmlMeasurement> htmlMeasurement;
};

class MeasurementTarget {
  public:
    virtual ~MeasurementTarget() = default;
    virtual ExtendedMetrics measureMetrics(MetricRequest const &) = 0;
    virtual UnknownBBox measureUnknownText(UnknownTextRequest const &) = 0;
};

struct NoStretch {};

struct ActiveStretch {
    std::uint32_t sourceCodepoint{};
    std::optional<std::uint32_t> selectedCodepoint;
};

using StretchState = std::variant<NoStretch, ActiveStretch>;

enum class CommonDiagnosticCode : std::uint8_t { unknown_math_variant };

struct CommonDiagnostic {
    CommonDiagnosticCode code;
    mml::NodeKind node_kind;
    std::string value;
    friend bool operator==(CommonDiagnostic const &, CommonDiagnostic const &) = default;
};
} // namespace folia::math::output::common
