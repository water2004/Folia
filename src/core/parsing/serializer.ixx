// Canonical public serializer facade. The one action/event visitor owns
// Markdown formatting; this module preserves the established public API and
// projection types while exposing only finalized text and source maps.
export module folia.core.serializer;
import std;
import folia.core.ast;
import folia.core.document;
import folia.core.ids;
import folia.core.serializer_visitor;
import folia.core.serializer_writer;
import folia.core.text_edit;
import folia.core.utf;

export namespace folia {

struct SerializedSourceMap {
    NodeId container_id{};
    // Index i stores the serialized Markdown offset for source offset i.
    std::vector<std::size_t> source_to_serialized;
};

struct SerializedMarkdownProjection {
    std::u32string text;
    std::vector<SerializedSourceMap> source_maps;
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
    auto emission = serializer_detail::serialize_blocks(blocks);
    return std::move(emission.text);
}

inline SerializedMarkdownProjection serialize_markdown_projection(
    EditorDocument const& document) {
    return serializer_facade_detail::to_public_projection(
        serializer_detail::serialize_document(document));
}

inline std::u32string serialize_markdown_cps(EditorDocument const& document) {
    return serialize_markdown_projection(document).text;
}

inline std::string serialize_markdown(EditorDocument const& document) {
    return cps_to_utf8(serialize_markdown_cps(document));
}

} // namespace folia
