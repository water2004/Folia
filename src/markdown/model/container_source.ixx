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

}
