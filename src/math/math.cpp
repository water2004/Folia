module folia.math;
import folia.math.adaptors.lite_adaptor;
import folia.math.cancellation;
import folia.math.core.math_item;
import folia.math.fonts.newcm_font_module_provider;
import folia.math.handlers.html.html_dom_strings;
import folia.math.input.tex.parser;
import folia.math.input.tex.default_packages;
import folia.math.output.common;
import folia.math.output.scene;
import folia.math.output.svg.wrapper_factory;
import folia.math.output.svg.writer;
import folia.math.util.utf16;
import std;

namespace folia::math {
namespace common = output::common;
namespace scene = output::scene;
namespace svg = output::svg;

namespace {
struct Warnings final : tex::WarningSink {
    void warn(std::u16string_view text) override {
        std::cerr << utf16::encode(text) << '\n';
    }
};

std::unique_ptr<common::Linebreaks> makeLinebreakVisitor(common::CommonWrapperFactory &factory) {
    return std::make_unique<common::LinebreakVisitor>(factory);
}


class LiteMeasurement final : public common::MeasurementTarget {
    adaptors::lite::LiteAdaptor &adaptor_;

  public:
    explicit LiteMeasurement(adaptors::lite::LiteAdaptor &adaptor) noexcept : adaptor_(adaptor) {}

    common::ExtendedMetrics measureMetrics(common::MetricRequest const &) override {
        throw std::logic_error("pinned NewCM requests must use the supplied font metrics");
    }

    common::UnknownBBox measureUnknownText(common::UnknownTextRequest const &request) override {
        const std::array<DOMNodeRef, 1> children{adaptor_.text(request.text)};
        const auto text = adaptor_.node(u"text", {}, children, u"http://www.w3.org/2000/svg");
        const auto [width, height] = adaptor_.nodeSize(text, 1000, true);
        (void)height;
        return {width, .75, .2};
    }
};

class SceneOutput final : public core::MathOutputJaxService {
    common::FontRepository const &fonts_;
    RenderOptions options_;
    LiteMeasurement measurement_;
    DOMDocumentRef htmlDocument_;
    common::HtmlMeasurement htmlMeasurement_;

    common::CommonOutputJaxConfig config() const;

  public:
    SceneOutput(common::FontRepository const &, adaptors::lite::LiteAdaptor &, RenderOptions);
    scene::Scene typeset(core::MathItem &, mml::MmlDocument &) override;
    scene::Scene escaped(core::MathItem &, mml::MmlDocument &) override;
    BBox getBBox(core::MathItem &, mml::MmlDocument &) override;
};
} // namespace

struct Renderer::State {
    std::thread::id owner = std::this_thread::get_id();
    bool unusable = false;
    std::shared_ptr<fonts::NewcmFontSource const> fontSource = fonts::newcmFontSource();
    adaptors::lite::LiteAdaptor adaptor;
    tex::ConfigurationHandler configurations;
    tex::MapHandler maps;
    tex::MacroCallbacks callbacks{{}, {}};
    tex::TexOptionValues texOptions;
    std::shared_ptr<handlers::html::HTMLDomStringsOptions> htmlOptions =
        std::make_shared<handlers::html::HTMLDomStringsOptions>();
    Warnings warnings;
    std::unique_ptr<tex::TeX> input;

    RenderResult render(std::u16string_view, std::stop_token, RenderOptions, fonts::NewcmFontDataBundle const&);
    scene::Scene renderScene(std::u16string_view, std::stop_token, RenderOptions);
    std::u16string decode(std::string_view, std::stop_token, RenderOptions) const;
    void validateOptions(RenderOptions const &) const;

    State() {
        tex::DefaultPackages::initialize(configurations, maps, htmlOptions);

        texOptions.packages = tex::DefaultPackages::selections();
        texOptions.tagSide = u"right";
        input = std::make_unique<tex::TeX>(configurations, maps, callbacks, warnings, texOptions);
    }

    void checkThread() const {
        if (owner != std::this_thread::get_id())
            throw RenderError("WrongThread", "Renderer used from a non-owning thread");
    }

    output::scene::Scene buildScene(std::u16string_view source, RenderOptions options,
                                   fonts::NewcmFontDataBundle const& fontData) {
        cancellation::checkpoint(cancellation::CheckpointStage::TeX);
        mml::MmlDocument document;
        tex::TeXRequest request(*input, document.factory());
        SceneOutput output(fontData.fonts, adaptor, options);
        core::AbstractMathItem expression(std::u16string(source), input.get(),
                                          options.display ? core::MathDisplay::display_math
                                                          : core::MathDisplay::inline_math);
        expression.setMetrics(options.em, options.em * .5, options.containerWidth, 1);
        expression.compile(document, {&output, &adaptor});
        if (!expression.root)
            throw std::logic_error("TeX input produced no MathItem root");
        document.setRoot(*expression.root);
        cancellation::checkpoint(cancellation::CheckpointStage::TeX);
        expression.typeset(output, document);
        cancellation::checkpoint(cancellation::CheckpointStage::Scene);
        if (!expression.typesetRoot)
            throw std::logic_error("Common output did not produce an owned Scene");
        return std::move(*expression.typesetRoot);
    }
};

namespace {
SceneOutput::SceneOutput(common::FontRepository const &fonts, adaptors::lite::LiteAdaptor &adaptor,
                         RenderOptions options)
    : fonts_(fonts), options_(options), measurement_(adaptor),
      htmlDocument_(adaptor.parse(u"", u"text/html")),
      htmlMeasurement_{
          adaptor, adaptor.parent(adaptor.body(htmlDocument_)),
          adaptor.node(u"mjx-container",
                       std::array<AttributeData, 2>{{{u"class", u"MathJax"}, {u"jax", u"SVG"}}})} {}

common::CommonOutputJaxConfig SceneOutput::config() const {
    common::CommonOptions commonOptions;
    commonOptions.displayOverflow = "linebreak";
    commonOptions.linebreaks.inlineBreaks = true;
    common::ExtendedMetrics metrics{options_.em, options_.em * .5, options_.containerWidth, 1,
                                    {},          options_.display};
    common::CommonOutputJaxConfig result{metrics, std::move(commonOptions), fonts_, "MathJaxNewcm"};
    result.inlineBreakPolicy = common::InlineBreakPolicy::forced;
    result.htmlMeasurement.emplace(htmlMeasurement_);
    return result;
}

scene::Scene SceneOutput::typeset(core::MathItem &math, mml::MmlDocument &document) {
    if (!math.root)
        throw std::logic_error("Common-to-Scene typeset requires a compiled MathItem root");
    if (document.root() != math.root)
        document.setRoot(*math.root);
    cancellation::checkpoint(cancellation::CheckpointStage::Layout);
    common::LayoutJob job(document, config(), measurement_, svg::SvgWrapperFactory::defaultNodes(),
                          makeLinebreakVisitor);
    auto result = job.buildScene();
    cancellation::checkpoint(cancellation::CheckpointStage::Scene);
    return result;
}

scene::Scene SceneOutput::escaped(core::MathItem &, mml::MmlDocument &) {
    throw svg::UnsupportedError("escaped math output is outside the TeX renderer contract");
}

BBox SceneOutput::getBBox(core::MathItem &math, mml::MmlDocument &document) {
    if (!math.root)
        throw std::logic_error("Common BBox requires a compiled MathItem root");
    return common::CommonOutputJax::getBBox(document, *math.root, config(), measurement_,
                                            svg::SvgWrapperFactory::defaultNodes(),
                                            makeLinebreakVisitor);
}

} // namespace

RenderResult Renderer::State::render(std::u16string_view source, std::stop_token stop,
                                     RenderOptions options, fonts::NewcmFontDataBundle const& fontData) {
    cancellation::Request request(stop, options.deadline);
    checkThread();
    if (unusable)
        throw RenderError("SessionUnusable", "Renderer session is unusable after cancellation");
    try {
        request.checkpoint();
    } catch (cancellation::Cancelled const &) {
        throw RenderError("Cancelled", "Math rendering cancelled before the request started");
    }
    validateOptions(options);
    cancellation::Scope scope(request);
    try {
        auto completedScene = buildScene(source, options, fontData);
        cancellation::checkpoint(cancellation::CheckpointStage::Serialization);
        svg::SvgWriteOptions writeOptions;
        writeOptions.font_cache = svg::FontCacheMode::none;
        writeOptions.font_resources = &fontData.svg_resources;
        auto svgText = svg::write(completedScene, writeOptions);
        request.checkpoint();
        return {std::move(svgText), std::move(completedScene)};
    } catch (cancellation::Cancelled const &) {
        unusable = true;
        throw RenderError("Cancelled", "Math rendering cancelled; renderer session is unusable");
    }
}

scene::Scene Renderer::State::renderScene(std::u16string_view source, std::stop_token stop,
                                          RenderOptions options) {
    cancellation::Request request(stop, options.deadline);
    checkThread();
    if (unusable)
        throw RenderError("SessionUnusable", "Renderer session is unusable after cancellation");
    try {
        request.checkpoint();
    } catch (cancellation::Cancelled const &) {
        throw RenderError("Cancelled", "Math rendering cancelled before the request started");
    }
    validateOptions(options);
    cancellation::Scope scope(request);
    try {
        const auto fontData = fonts::makeNewcmFontData(fontSource);
        auto result = buildScene(source, options, fontData);
        request.checkpoint();
        return result;
    } catch (cancellation::Cancelled const &) {
        unusable = true;
        throw RenderError("Cancelled", "Math rendering cancelled; renderer session is unusable");
    }
}

void Renderer::State::validateOptions(RenderOptions const &options) const {
    if (!std::isfinite(options.em) || options.em <= 0)
        throw RenderError("InvalidArgument", "Renderer em must be finite and positive");
    if (!std::isfinite(options.containerWidth) || options.containerWidth <= 0)
        throw RenderError("InvalidArgument",
                          "Renderer container width must be finite and positive");
}

std::u16string Renderer::State::decode(std::string_view text, std::stop_token stop,
                                       RenderOptions options) const {
    cancellation::Request request(stop, options.deadline);
    checkThread();
    if (unusable)
        throw RenderError("SessionUnusable", "Renderer session is unusable after cancellation");
    try {
        request.checkpoint();
    } catch (cancellation::Cancelled const &) {
        throw RenderError("Cancelled", "Math rendering cancelled before the request started");
    }
    validateOptions(options);
    cancellation::Scope scope(request);
    try {
        return utf16::decode(text).text;
    } catch (cancellation::Cancelled const &) {
        throw RenderError("Cancelled", "Math rendering cancelled before the request started");
    }
}

Renderer::Renderer() : state_(std::make_unique<State>()) {}
Renderer::~Renderer() = default;

std::vector<RenderOutcome> Renderer::renderBatch(std::span<RenderInput const> requests) {
    state_->checkThread();
    std::vector<RenderOutcome> results;
    results.reserve(requests.size());
    if (requests.empty()) return results;
    // This is the only font-body owner for the batch. The persistent parser
    // has no reference to it; returned Scenes own only their used resources.
    const auto fontData = fonts::makeNewcmFontData(state_->fontSource);
    for (auto const& request : requests) {
        auto options = request.options;
        if (request.timeout > std::chrono::milliseconds::zero()) {
            const auto deadline = std::chrono::steady_clock::now() + request.timeout;
            if (!options.deadline || deadline < *options.deadline) options.deadline = deadline;
        }
        try {
            results.push_back({state_->render(request.tex, request.stop, options, fontData), {}});
        } catch (...) {
            results.push_back({{}, std::current_exception()});
        }
    }
    return results;
}

RenderResult Renderer::render(std::u16string_view source, RenderOptions options) {
    return render(source, std::stop_token{}, options);
}
RenderResult Renderer::render(std::string_view source, RenderOptions options) {
    return render(source, std::stop_token{}, options);
}
RenderResult Renderer::render(std::u16string_view source, std::stop_token stop,
                              RenderOptions options) {
    const RenderInput request{source, stop, options};
    auto result = renderBatch(std::span{&request, std::size_t{1}});
    if (result.front().error) std::rethrow_exception(result.front().error);
    return std::move(*result.front().result);
}
RenderResult Renderer::render(std::string_view source, std::stop_token stop,
                              RenderOptions options) {
    auto decoded = state_->decode(source, stop, options);
    return render(decoded, stop, options);
}

scene::Scene Renderer::renderScene(std::u16string_view source, RenderOptions options) {
    return renderScene(source, std::stop_token{}, options);
}
scene::Scene Renderer::renderScene(std::string_view source, RenderOptions options) {
    return renderScene(source, std::stop_token{}, options);
}
scene::Scene Renderer::renderScene(std::u16string_view source, std::stop_token stop,
                                   RenderOptions options) {
    return state_->renderScene(source, stop, options);
}
scene::Scene Renderer::renderScene(std::string_view source, std::stop_token stop,
                                   RenderOptions options) {
    auto decoded = state_->decode(source, stop, options);
    return renderScene(decoded, stop, options);
}

std::string Renderer::toSvg(std::u16string_view source, RenderOptions options) {
    return render(source, options).svg;
}
std::string Renderer::toSvg(std::string_view source, RenderOptions options) {
    return render(source, options).svg;
}
std::string Renderer::toSvg(std::u16string_view source, std::stop_token stop,
                            RenderOptions options) {
    return render(source, stop, options).svg;
}
std::string Renderer::toSvg(std::string_view source, std::stop_token stop, RenderOptions options) {
    return render(source, stop, options).svg;
}
} // namespace folia::math
