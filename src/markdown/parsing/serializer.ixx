// Canonical public serializer facade. The one action/event visitor owns
// Markdown formatting; this module preserves the established public API and
// projection types while exposing only finalized text and source maps.
export module folia.markdown.serializer;
import std;
import folia.markdown.selection;
import folia.markdown.ast;
import folia.markdown.document;
import folia.markdown.ids;
import folia.markdown.serializer_visitor;
import folia.markdown.serializer_writer;
import folia.markdown.text_edit;
import folia.markdown.utf;

export namespace folia {

// Formatting is used only for newly inserted syntax. Existing inline source
// remains character-exact, including its original destination/title spelling.
inline std::u32string serialize_link_destination(std::u32string_view destination, bool force_angle = false) {
    const auto angle = force_angle || destination.find_first_of(U" \t\r\n()<>\\") != std::u32string_view::npos;
    std::u32string result = angle ? U"<" : U"";
    for (auto c : destination) {
        if (c == U'\n') { result += U"%0A"; continue; }
        if (c == U'\r') { result += U"%0D"; continue; }
        if (c == U'\t') { result += U"%09"; continue; }
        if (c == U'\\' || c == U'<' || c == U'>' || c == U'|') result += U'\\';
        result += c;
    }
    if (angle) result += U'>';
    return result;
}

inline std::u32string serialize_link_label(std::u32string_view label) {
    // A label may itself contain Markdown formatting and balanced brackets.
    // Escape only unmatched brackets; retain existing source escapes verbatim.
    std::vector<std::size_t> openings;
    std::vector<bool> escape(label.size(), false);
    for (std::size_t index = 0; index < label.size(); ++index) {
        if (label[index] == U'\\' && index + 1 < label.size()) { ++index; continue; }
        if (label[index] == U'|') escape[index] = true;
        if (label[index] == U'[') openings.push_back(index);
        else if (label[index] == U']') {
            if (openings.empty()) escape[index] = true;
            else openings.pop_back();
        }
    }
    for (auto index : openings) escape[index] = true;
    std::u32string result;
    for (std::size_t index = 0; index < label.size(); ++index) {
        if (escape[index]) result += U'\\';
        result += label[index];
    }
    // A trailing escape must not consume the generated closing bracket.
    std::size_t trailing = 0;
    for (auto index = label.size(); index > 0 && label[index - 1] == U'\\'; --index) ++trailing;
    if (trailing % 2 != 0) result += U'\\';
    return result;
}

inline std::u32string serialize_link_source(std::u32string_view destination,
    std::u32string_view label, const std::optional<std::u32string>& title = {}) {
    auto result = U"[" + serialize_link_label(label) + U"](" + serialize_link_destination(destination);
    if (title) {
        result += U" \"";
        for (auto c : *title) {
            if (c == U'\\' || c == U'\"') result += U'\\';
            result += c;
        }
        result += U'\"';
    }
    result += U')';
    return result;
}

inline std::u32string serialize_image_source(std::u32string_view destination,
    std::u32string_view alt = {}, bool force_angle_destination = true) {
    std::u32string result = U"![";
    for (auto c : alt) {
        if (c == U'\\' || c == U'[' || c == U']' || c == U'|') result += U'\\';
        result += c;
    }
    result += U"](" + serialize_link_destination(destination, force_angle_destination) + U")";
    return result;
}

inline std::u32string serialize_html_link_source(std::u32string_view destination,
    std::u32string_view label_source, const std::optional<std::u32string>& title = {}) {
    auto result = U"<a href=\"" + serializer_detail::serializer_html_escape_attribute(destination) + U"\"";
    if (title) result += U" title=\"" + serializer_detail::serializer_html_escape_attribute(*title) + U"\"";
    return result + U">" + std::u32string(label_source) + U"</a>";
}

inline std::u32string serialize_html_image_source(std::u32string_view destination, std::u32string_view alt) {
    return U"<img src=\"" + serializer_detail::serializer_html_escape_attribute(destination)
        + U"\" alt=\"" + serializer_detail::serializer_html_escape_attribute(alt) + U"\">";
}

struct SerializedSourceMap {
    NodeId container_id{};
    // Index i stores the serialized Markdown offset for source offset i.
    std::vector<std::size_t> source_to_serialized;
};

using SerializedBlockRange=serializer_detail::SerializerBlockRange;

struct SerializedMarkdownProjection {
    std::u32string text;
    std::vector<SerializedSourceMap> source_maps;
    // Presentation boundaries of the requested direct blocks, not a second
    // position inside an editable owner. Used to update the source view locally.
    std::vector<SerializedBlockRange> blocks;
};

inline std::optional<std::size_t> serialized_offset_for_source_position(
    SerializedMarkdownProjection const& projection,
    TextPosition position) {
    for (const auto& map : projection.source_maps) {
        if (map.container_id != position.container_id || map.source_to_serialized.empty()) continue;
        const auto local = (std::min)(position.source_offset, map.source_to_serialized.size() - 1);
        return map.source_to_serialized[local];
    }
    return std::nullopt;
}

inline std::optional<TextPosition> source_position_for_serialized_offset(
    SerializedMarkdownProjection const& projection,
    std::size_t serialized_offset,
    TextAffinity affinity = TextAffinity::Downstream) {
    serialized_offset = (std::min)(serialized_offset, projection.text.size());
    std::optional<TextPosition> preceding;
    std::optional<TextPosition> following;
    std::size_t preceding_offset = 0;
    std::size_t following_offset = (std::numeric_limits<std::size_t>::max)();
    for (const auto& map : projection.source_maps) {
        for (std::size_t local = 0; local < map.source_to_serialized.size(); ++local) {
            const auto projected = map.source_to_serialized[local];
            const TextPosition candidate{map.container_id, local, affinity};
            if (projected == serialized_offset) return candidate;
            if (projected < serialized_offset && (!preceding || projected >= preceding_offset)) {
                preceding = candidate;
                preceding_offset = projected;
            }
            if (projected > serialized_offset && (!following || projected < following_offset)) {
                following = candidate;
                following_offset = projected;
            }
        }
    }
    if (affinity == TextAffinity::Upstream) return preceding ? preceding : following;
    return following ? following : preceding;
}

namespace serializer_facade_detail {

inline SerializedMarkdownProjection to_public_projection(
    serializer_detail::SerializerEmission emission) {
    SerializedMarkdownProjection projection;
    projection.text = std::move(emission.text);
    projection.blocks = std::move(emission.blocks);
    projection.source_maps.reserve(emission.source_maps.size());
    for (auto& map : emission.source_maps) {
        projection.source_maps.push_back({map.owner, std::move(map.source_to_serialized)});
    }
    return projection;
}

} // namespace serializer_facade_detail

// Serialize an already-structured Markdown fragment without treating it as a
// complete-document save. Clipboard copy and local block reparse use this path.
inline std::u32string serialize_markdown_fragment(BlockVec const& blocks) {
    auto emission = serializer_detail::serialize_blocks(blocks, 0,
        (std::numeric_limits<std::size_t>::max)(), false);
    return std::move(emission.text);
}

inline SerializedMarkdownProjection serialize_markdown_fragment_projection(
    BlockVec const& blocks, std::size_t begin=0,
    std::size_t end=(std::numeric_limits<std::size_t>::max)()) {
    return serializer_facade_detail::to_public_projection(
        serializer_detail::serialize_blocks(blocks,begin,end));
}

inline SerializedMarkdownProjection serialize_markdown_region_projection(
    const EditorDocument& document,std::size_t begin,std::size_t end) {
    auto emission=serializer_detail::serialize_blocks(document.root.children,begin,end);
    if (end==document.root.children.size()) serializer_detail::append_document_ending(emission,document);
    return serializer_facade_detail::to_public_projection(std::move(emission));
}

inline SerializedMarkdownProjection serialize_markdown_projection(
    EditorDocument const& document) {
    return serializer_facade_detail::to_public_projection(
        serializer_detail::serialize_document(document));
}

inline std::u32string serialize_markdown_cps(EditorDocument const& document) {
    return serializer_detail::serialize_document(document, false).text;
}

inline std::string serialize_markdown(EditorDocument const& document) {
    return cps_to_utf8(serialize_markdown_cps(document));
}

} // namespace folia
