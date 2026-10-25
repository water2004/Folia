export module folia.math.fonts.newcm_font_module_provider;
export import folia.math.fonts.newcm_font_data;
import std;

export namespace folia::math::fonts {
struct NewcmGlyphData {
    std::optional<output::common::FontGlyph> glyph;
    std::optional<output::svg::SvgFontGlyphSource> svg;
};
class NewcmFontRequest final : public output::common::FontGlyphLookup,
                               public output::svg::SvgFontGlyphLookup {
    struct State;
    std::unique_ptr<State> state_;
    // Shared batch loader; scalar callers supply one stack slot, so repeated
    // Common/SVG borrows do not allocate a result container. Sizes must match.
    void read(std::span<output::svg::SvgFontGlyphKey const> keys,
              std::span<NewcmGlyphData const *> result) const;

  public:
    explicit NewcmFontRequest(std::shared_ptr<NewcmFontTables const> tables);
    ~NewcmFontRequest();
    NewcmFontRequest(NewcmFontRequest const &) = delete;
    NewcmFontRequest &operator=(NewcmFontRequest const &) = delete;
    // One loading API: a single key is a batch of one. Returned views are valid
    // only for this request; completed Scene glyph resources own their values.
    std::vector<NewcmGlyphData const *>
    read(std::span<output::svg::SvgFontGlyphKey const> keys) const;
    std::vector<output::svg::SvgFontGlyphKey> keys() const;
    std::vector<std::pair<output::svg::SvgFontGlyphKey, output::svg::SvgFontGlyphKey>>
    aliases() const;
    std::size_t loadedGlyphCount() const noexcept;
    std::size_t loadedResourceCount() const noexcept;
    std::size_t glyphCount() const noexcept override;
    bool contains(std::string_view variant, std::uint32_t codepoint) const override;
    output::common::FontGlyph const *glyph(std::string_view variant,
                                           std::uint32_t codepoint) const override;
    output::svg::SvgFontGlyphSource const *
    find(output::svg::SvgFontGlyphKey const &key) const override;
};

struct NewcmFontSource {
    output::common::FontFaceData metadata;
    std::shared_ptr<NewcmFontTables const> tables;
};
std::shared_ptr<NewcmFontSource const> newcmFontSource();
struct NewcmProviderCoverage {
    std::size_t source_dynamic_module_count{};
    std::size_t source_provider_trace_events{};
    std::size_t native_actual_direct_glyph_records{};
    std::size_t native_actual_remap_records{};
    std::size_t native_actual_glyph_resource_objects{};
};
struct NewcmFontDataBundle {
    output::common::FontRepository fonts;
    output::svg::SvgFontResources svg_resources;
};
// Publishes one font request over pinned read-only records. Source trace fields
// are provenance metadata. Available glyph/remap records are separate from
// resource objects actually materialized by this request.
NewcmFontDataBundle
makeNewcmFontData(std::shared_ptr<NewcmFontSource const> source = newcmFontSource());
NewcmProviderCoverage newcmProviderCoverage(output::common::FontRepository const &repository);
} // namespace folia::math::fonts
