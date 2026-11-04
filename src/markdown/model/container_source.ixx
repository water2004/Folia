// folia.markdown.container_source — serializer-only provenance for physical lines.
export module folia.markdown.container_source;
import std;
import folia.markdown.ids;

export namespace folia {

enum class ContainerAnchorPart { Source, Start, End, Separator };

struct ContainerPrefixAnchor {
    NodeId owner_id{};
    ContainerAnchorPart part = ContainerAnchorPart::Source;
    std::size_t offset = 0;
    bool operator==(const ContainerPrefixAnchor&) const = default;
};

enum class ContainerPrefixRole { Literal, ItemOpening };

struct ContainerLinePrefix {
    ContainerPrefixAnchor anchor;
    std::u32string text;
    bool lazy = false;
    ContainerPrefixRole role = ContainerPrefixRole::Literal;
    bool operator==(const ContainerLinePrefix&) const = default;
};

struct ContainerSource {
    std::vector<ContainerLinePrefix> lines;
    bool operator==(const ContainerSource&) const = default;
};

// Consume an already recognized item marker. A task checkbox belongs to the
// first body line and does not add columns to the list continuation prefix.
inline std::u32string list_continuation_prefix(std::u32string_view marker, bool task_item) {
    if (task_item) marker = marker.substr(0, marker.find(U'['));
    std::size_t columns = 0;
    for (auto cp : marker) columns += cp == U'\t' ? 4 - columns % 4 : 1;
    if (marker.empty()) columns = 2;
    else if (marker.back() != U' ' && marker.back() != U'\t') ++columns;
    return std::u32string(columns, U' ');
}

}
