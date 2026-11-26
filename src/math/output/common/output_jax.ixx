export module folia.math.output.common:output_jax;
export import :types;
export import :wrapper;
export import :wrapper_factory;
export import :linebreak_visitor;
export import folia.math.core.mml_tree.mml_document;
export import folia.math.output.scene;
import std;

export namespace folia::math::output::common {
class CommonMtable;
using LinebreakFactory = std::unique_ptr<Linebreaks> (*)(CommonWrapperFactory &);

class CommonOutputJax {
    mml::MmlDocument &document_; // sole document borrow; root stays in document
    FontRepository const &fonts_;
    FontFaceData const &face_;
    ExtendedMetrics metrics_;
    CommonOptions options_;
    InlineBreakPolicy inlineBreakPolicy_;
    MeasurementTarget &measurement_;
    std::optional<HtmlMeasurement> htmlMeasurement_;
    // No linebreak constraint exists until getLinebreakWidth runs. NaN keeps
    // width comparisons inactive, as in Source's unset mathematical width.
    double containerWidth_ = std::numeric_limits<double>::quiet_NaN();
    std::optional<double> reusableWidthFrom_ = 0.0;
    bool usesContainerLinebreakWidth_ = false;
    // Reverse destruction: visitor first, then factory and its wrappers.
    std::unique_ptr<CommonWrapperFactory> factory_;
    std::unique_ptr<Linebreaks> linebreaks_;
    std::unordered_map<std::string, std::unordered_map<std::u16string, UnknownBBox>> unknownCache_;
    std::unordered_map<mml::MmlNode *, CommonWrapper *> wrappers_;
    std::vector<CommonDiagnostic> diagnostics_;
    CommonWrapper *table_{};

    friend class CommonMtable;
    friend class CommonWrapper;
    friend class LayoutJob;
    void recordUnknownMathVariant(mml::NodeKind, std::string);
    // Source CommonMtable::findContainer is the only caller.
    void setTable(CommonWrapper &); // called only from CommonMtable::findContainer
    void finishInlineFrames(scene::Builder &, CommonWrapper &, std::span<SceneNode const>,
                            scene::FrameLayout &);
    void finishScene(scene::Builder &, CommonWrapper &, std::span<SceneNode const>);

  public:
    CommonOutputJax(mml::MmlDocument &, CommonOutputJaxConfig, MeasurementTarget &,
                    CommonWrapperCatalog, LinebreakFactory);
    virtual ~CommonOutputJax() = default;
    CommonOutputJax(CommonOutputJax const &) = delete;
    CommonOutputJax &operator=(CommonOutputJax const &) = delete;

    static BBox getBBox(mml::MmlDocument &, mml::MmlNode &subtree, CommonOutputJaxConfig,
                        MeasurementTarget &, CommonWrapperCatalog, LinebreakFactory);

    CommonWrapperFactory &factory() noexcept;
    mml::MmlDocument &document() const noexcept;
    double pxPerEm() const;
    double containerWidth() const noexcept;
    std::optional<double> reusableWidthFrom() const noexcept { return reusableWidthFrom_; }
    bool usesContainerLinebreakWidth() const noexcept { return usesContainerLinebreakWidth_; }
    void requireExactContainerWidth() noexcept { reusableWidthFrom_.reset(); }
    void requireMinimumContainerWidth(double width) noexcept {
        if (reusableWidthFrom_) *reusableWidthFrom_ = std::max(*reusableWidthFrom_, width);
    }
    CommonWrapperFactory const &factory() const noexcept;
    FontRepository const &fonts() const noexcept;
    FontFaceData const &face() const noexcept;
    [[nodiscard]] scene::Scene buildScene(CommonWrapper &root);
    ExtendedMetrics const &currentMetrics() const noexcept;
    CommonOptions const &options() const noexcept;
    HtmlMeasurement const &htmlMeasurement() const;
    LinebreakOptions const &currentLinebreakOptions() const noexcept;
    Linebreaks &linebreaks() const noexcept;
    CommonWrapper *wrapperFor(mml::MmlNode const &) const noexcept;
    void registerWrapper(mml::MmlNode &, CommonWrapper &);
    CommonWrapper *table() const noexcept;
    std::vector<CommonDiagnostic> const &diagnostics() const noexcept;

    virtual bool forceInlineBreaks() const;
    virtual double getLinebreakWidth();
    virtual void markInlineBreaks(mml::MmlNode &);
    virtual void unmarkInlineBreaks(mml::MmlNode &);
    virtual ExtendedMetrics getMetricsFor(MetricRequest const &);
    virtual UnknownBBox measureText(std::u16string_view, std::string_view variant,
                                    CssFontData = {});
    UnknownBBox measureTextWithCache(UnknownTextRequest const &);
    virtual UnknownBBox measureTextNodeWithCache(UnknownTextRequest const &);
    virtual style_json::StyleJson cssFontStyles(CssFontData const &,
                                                style_json::StyleJson = {}) const;
    virtual CssFontData getFontData(styles::Styles const &);
    virtual void addFontStyles(style_json::StyleJsonSheet &);
    virtual void addWrapperStyles(style_json::StyleJsonSheet &);
    virtual void addClassStyles(mml::NodeKind, style_json::StyleJsonSheet &);
};

class LayoutJob final {
    CommonOutputJax jax_; // owns factory then visitor
  public:
    LayoutJob(mml::MmlDocument &, CommonOutputJaxConfig, MeasurementTarget &, CommonWrapperCatalog,
              LinebreakFactory);
    LayoutJob(LayoutJob const &) = delete;
    LayoutJob &operator=(LayoutJob const &) = delete;
    mml::MmlDocument &document() const noexcept;
    CommonOutputJax &jax() noexcept;
    CommonOutputJax const &jax() const noexcept;
    CommonWrapperFactory &factory() noexcept;
    CommonWrapper &build(); // obtains the root from document; stores no second root
    [[nodiscard]] scene::Scene buildScene(); // freezes an owned snapshot; retains no MML root
};
} // namespace folia::math::output::common
