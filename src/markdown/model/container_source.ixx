// folia.markdown.container_source — serializer-only provenance for physical lines.
export module folia.markdown.container_source;
import std;
import folia.markdown.ids;
import folia.markdown.instrumentation;

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

    ContainerSource() = default;
    ContainerSource(const ContainerSource& other) : lines(other.lines) {
        record_container_prefix_copy(lines.size());
    }
    ContainerSource& operator=(const ContainerSource& other) {
        if (this != &other) {
            lines = other.lines;
            invalidate_owner_lookup();
            record_container_prefix_copy(lines.size());
        }
        return *this;
    }
    ContainerSource(ContainerSource&&) noexcept = default;
    ContainerSource& operator=(ContainerSource&&) noexcept = default;
    bool operator==(const ContainerSource& other) const { return lines == other.lines; }

    // Prefix provenance is still the exact, authoritative line sequence. This
    // compact derived lookup only identifies the records owned by one leaf;
    // editing that leaf must not copy every prefix of an enclosing quote.
    // Small item-local tables need neither a heap allocation nor an index.
    void prepare_owner_lookup() const {
        if (lines.size() <= small_lookup_limit || owner_lookup_) return;
        auto indices = std::make_unique<std::vector<std::size_t>>(lines.size());
        std::iota(indices->begin(), indices->end(), std::size_t{0});
        std::ranges::sort(*indices, {}, [this](std::size_t index) {
            record_container_prefix_probe();
            return lines[index].anchor.owner_id.v;
        });
        owner_lookup_ = std::move(indices);
    }

    void invalidate_owner_lookup() { owner_lookup_.reset(); }

    template<class Visitor>
    void visit_owner(NodeId owner, Visitor&& visitor) const {
        if (lines.size() <= small_lookup_limit) {
            for (const auto& line : lines) {
                record_container_prefix_probe();
                if (line.anchor.owner_id == owner) visitor(line);
            }
            return;
        }
        prepare_owner_lookup();
        const auto project = [this](std::size_t index) {
            record_container_prefix_probe();
            return lines[index].anchor.owner_id.v;
        };
        const auto found = std::ranges::equal_range(*owner_lookup_, owner.v, {}, project);
        for (const auto index : found) visitor(lines[index]);
    }

private:
    static constexpr std::size_t small_lookup_limit = 32;
    mutable std::unique_ptr<std::vector<std::size_t>> owner_lookup_;
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
