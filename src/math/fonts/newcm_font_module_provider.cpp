module folia.math.fonts.newcm_font_module_provider;
import folia.math.output.common.font_path;
import folia.math.output.scene.types;
import std;
namespace folia::math::fonts {
using namespace output::common;
using namespace output::svg;
struct NewcmFontRequest::State {
    std::shared_ptr<NewcmFontTables const> tables;
    mutable std::mutex mutex;
    mutable std::map<SvgFontGlyphKey, NewcmGlyphData> loaded;
};
NewcmFontRequest::NewcmFontRequest(std::shared_ptr<NewcmFontTables const> tables)
    : state_(std::make_unique<State>()) {
    if (!tables)
        throw std::invalid_argument("NewCM font tables are absent");
    state_->tables = std::move(tables);
}
NewcmFontRequest::~NewcmFontRequest() = default;
std::size_t NewcmFontRequest::glyphCount() const noexcept { return state_->tables->glyphCount(); }
bool NewcmFontRequest::contains(std::string_view variant, std::uint32_t codepoint) const {
    return state_->tables->contains({std::string(variant), codepoint});
}
std::vector<SvgFontGlyphKey> NewcmFontRequest::keys() const { return state_->tables->keys(); }
std::vector<std::pair<SvgFontGlyphKey, SvgFontGlyphKey>> NewcmFontRequest::aliases() const {
    return state_->tables->aliases();
}
std::size_t NewcmFontRequest::loadedGlyphCount() const noexcept {
    std::scoped_lock lock(state_->mutex);
    return state_->loaded.size();
}
std::size_t NewcmFontRequest::loadedResourceCount() const noexcept {
    std::scoped_lock lock(state_->mutex);
    return static_cast<std::size_t>(std::ranges::count_if(state_->loaded, [](auto const &entry) {
        return entry.second.glyph && entry.second.glyph->resource;
    }));
}

std::vector<NewcmGlyphData const *>
NewcmFontRequest::read(std::span<SvgFontGlyphKey const> keys) const {
    std::vector<NewcmGlyphData const *> result(keys.size());
    read(keys, result);
    return result;
}

void NewcmFontRequest::read(std::span<SvgFontGlyphKey const> keys,
                            std::span<NewcmGlyphData const *> result) const {
    std::scoped_lock lock(state_->mutex);
    std::set<SvgFontGlyphKey> missing;
    for (std::size_t i = 0; i < keys.size(); ++i) {
        const auto found = state_->loaded.find(keys[i]);
        result[i] = found == state_->loaded.end() ? nullptr : &found->second;
        if (!result[i])
            missing.insert(keys[i]);
    }
    if (!missing.empty()) {
        const std::vector<SvgFontGlyphKey> requested(missing.begin(), missing.end());
        const auto records = state_->tables->lookup(requested);
        for (std::size_t i = 0; i < requested.size(); ++i) {
            NewcmGlyphData data;
            if (auto row = records[i].glyph) {
                FontGlyph glyph;
                glyph.metrics = {row->metrics[0], row->metrics[1], row->metrics[2], row->metrics[3],
                                 row->metrics[4], row->metrics[5], row->metrics[6]};
                glyph.path_defined = row->path_defined;
                glyph.composition.assign(row->composition,
                                         row->composition + row->composition_size);
                if (row->path_defined) {
                    auto decoded =
                        *row->path
                            ? decodeFontOutlinePrefix(std::string("M") + row->path + "Z", 1000.0)
                            : FontOutlineDecodeResult{output::scene::GlyphOutline{}, std::nullopt};
                    glyph.outline_error = decoded.error;
                    glyph.resource = output::scene::GlyphResource::make(
                        std::string("MathJaxNewcm/") + row->variant, std::move(decoded.outline));
                }
                data.glyph = std::move(glyph);
            }
            if (auto row = records[i].svg) {
                SvgFontGlyphSource source{row->path_defined, row->serialized_path, {}};
                if (row->content_defined)
                    source.content_data = row->content;
                data.svg = std::move(source);
            }
            state_->loaded.emplace(requested[i], std::move(data));
        }
    }
    for (std::size_t i = 0; i < keys.size(); ++i)
        if (!result[i])
            result[i] = &state_->loaded.at(keys[i]);
}
FontGlyph const *NewcmFontRequest::glyph(std::string_view variant, std::uint32_t codepoint) const {
    const SvgFontGlyphKey key{std::string(variant), codepoint};
    NewcmGlyphData const *value = nullptr;
    read(std::span{&key, std::size_t{1}}, std::span{&value, std::size_t{1}});
    return value->glyph ? &*value->glyph : nullptr;
}
SvgFontGlyphSource const *NewcmFontRequest::find(SvgFontGlyphKey const &key) const {
    NewcmGlyphData const *value = nullptr;
    read(std::span{&key, std::size_t{1}}, std::span{&value, std::size_t{1}});
    return value->svg ? &*value->svg : nullptr;
}

std::shared_ptr<NewcmFontSource const> newcmFontSource() {
    static std::mutex mutex;
    static std::weak_ptr<NewcmFontSource const> source;
    std::scoped_lock lock(mutex);
    if (auto current = source.lock())
        return current;
    auto tables = std::make_shared<NewcmFontTables>();
    auto result = std::make_shared<NewcmFontSource>();
    result->metadata = installNewcmFontData(*tables);
    result->tables = std::move(tables);
    source = result;
    return result;
}
NewcmFontDataBundle makeNewcmFontData(std::shared_ptr<NewcmFontSource const> source) {
    if (!source || !source->tables)
        throw std::invalid_argument("NewCM font source is absent");
    auto request = std::make_shared<NewcmFontRequest>(source->tables);
    auto face = source->metadata;
    face.glyphSource = request;
    FontRepositoryBuilder builder;
    builder.addFace(std::move(face));
    SvgFontResourcesBuilder svgBuilder;
    svgBuilder.setSource(std::move(request));
    return {std::move(builder).freeze(), std::move(svgBuilder).freeze()};
}
NewcmProviderCoverage newcmProviderCoverage(output::common::FontRepository const &repository) {
    const auto &face = repository.face("MathJaxNewcm");
    std::size_t direct = 0, remaps = 0;
    std::set<output::scene::GlyphResource const *> resources;
    for (auto const &[_, variant] : face.variants) {
        direct += variant.glyphs.size();
        remaps += variant.remap.size();
        for (auto const &[__, glyph] : variant.glyphs)
            if (glyph.resource)
                resources.insert(glyph.resource.get());
    }
    if (auto request = std::dynamic_pointer_cast<NewcmFontRequest const>(face.glyphSource)) {
        direct += request->glyphCount();
        return {.source_dynamic_module_count = face.source_dynamic_module_count,
                .source_provider_trace_events = face.source_provider_trace_events,
                .native_actual_direct_glyph_records = direct,
                .native_actual_remap_records = remaps,
                .native_actual_glyph_resource_objects = request->loadedResourceCount()};
    }
    return {.source_dynamic_module_count = face.source_dynamic_module_count,
            .source_provider_trace_events = face.source_provider_trace_events,
            .native_actual_direct_glyph_records = direct,
            .native_actual_remap_records = remaps,
            .native_actual_glyph_resource_objects = resources.size()};
}
} // namespace folia::math::fonts
