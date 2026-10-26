module folia.math.fonts.newcm_font_data;
import std;
namespace folia::math::fonts {
using namespace output::common;
using namespace output::svg;
struct NewcmFontTables::State {
    std::vector<std::span<NewcmFontGlyphRow const>> tables;
    std::vector<std::span<NewcmSvgAliasRow const>> aliases;
    std::map<std::string_view, std::vector<std::span<NewcmFontGlyphRow const>>, std::less<>> variants;
    std::map<std::string_view, std::vector<std::span<NewcmSvgAliasRow const>>, std::less<>> aliasVariants;
    std::size_t recordCount = 0;

    NewcmFontGlyphRow const *row(SvgFontGlyphKey const &key) const {
        const auto variant = variants.find(key.variant);
        if (variant == variants.end()) return nullptr;
        for (auto table = variant->second.rbegin(); table != variant->second.rend(); ++table) {
            auto found = std::lower_bound(
                table->begin(), table->end(), key, [](auto const &row, auto const &target) {
                    return row.code < target.codepoint;
                });
            if (found != table->end() && found->code == key.codepoint)
                return &*found;
        }
        return nullptr;
    }
    NewcmFontGlyphRow const *svgRow(SvgFontGlyphKey const &key,
                                   NewcmFontGlyphRow const *direct) const {
        const auto variant = aliasVariants.find(key.variant);
        if (variant == aliasVariants.end()) return direct;
        for (auto table = variant->second.rbegin(); table != variant->second.rend(); ++table) {
            auto found = std::lower_bound(
                table->begin(), table->end(), key, [](auto const &row, auto const &target) {
                    return row.from_codepoint < target.codepoint;
                });
            if (found != table->end() && found->from_codepoint == key.codepoint)
                return row({found->to_variant, found->to_codepoint});
        }
        return direct;
    }
};

NewcmFontTables::NewcmFontTables() : state_(std::make_unique<State>()) {}
NewcmFontTables::~NewcmFontTables() = default;
void NewcmFontTables::addRows(std::span<NewcmFontGlyphRow const> rows) {
    state_->tables.push_back(rows);
    // Tables are sorted by (variant, codepoint). Borrow only each variant's
    // range, preserving module registration order and late-extension priority.
    // This index contains no glyph bodies or decoded outlines.
    for (std::size_t begin = 0; begin < rows.size();) {
        const std::string_view variant = rows[begin].variant;
        auto end = begin + 1;
        while (end < rows.size() && rows[end].variant == variant) ++end;
        state_->variants[variant].push_back(rows.subspan(begin, end - begin));
        begin = end;
    }
    state_->recordCount += rows.size();
}
void NewcmFontTables::addAliases(std::span<NewcmSvgAliasRow const> rows) {
    state_->aliases.push_back(rows);
    for (std::size_t begin = 0; begin < rows.size();) {
        const std::string_view variant = rows[begin].from_variant;
        auto end = begin + 1;
        while (end < rows.size() && rows[end].from_variant == variant) ++end;
        state_->aliasVariants[variant].push_back(rows.subspan(begin, end - begin));
        begin = end;
    }
}
void NewcmFontTables::setGlyphCount(std::size_t count) { state_->recordCount = count; }
std::size_t NewcmFontTables::glyphCount() const noexcept { return state_->recordCount; }
bool NewcmFontTables::contains(SvgFontGlyphKey const &key) const {
    return state_->row(key) != nullptr;
}
std::vector<NewcmGlyphRecord> NewcmFontTables::lookup(std::span<SvgFontGlyphKey const> keys) const {
    std::vector<NewcmGlyphRecord> result;
    result.reserve(keys.size());
    for (auto const &key : keys) {
        const auto direct = state_->row(key);
        result.push_back({direct, state_->svgRow(key, direct)});
    }
    return result;
}
std::vector<SvgFontGlyphKey> NewcmFontTables::keys() const {
    std::set<SvgFontGlyphKey> keys;
    for (auto table : state_->tables)
        for (auto const &row : table)
            keys.insert({row.variant, row.code});
    return {keys.begin(), keys.end()};
}
std::vector<std::pair<SvgFontGlyphKey, SvgFontGlyphKey>> NewcmFontTables::aliases() const {
    std::map<SvgFontGlyphKey, SvgFontGlyphKey> aliases;
    for (auto table : state_->aliases)
        for (auto const &row : table)
            aliases.insert_or_assign(SvgFontGlyphKey{row.from_variant, row.from_codepoint},
                                     SvgFontGlyphKey{row.to_variant, row.to_codepoint});
    return {aliases.begin(), aliases.end()};
}

#include "newcm_font_data_typed.inc"
FontFaceData installNewcmFontData(NewcmFontTables &tables) { return makeNewcmFace(tables); }
} // namespace folia::math::fonts
