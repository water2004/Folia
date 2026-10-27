// HTML leaf source and direct maps to the original physical source
// back to the shared root physical source. This owns no HTML grammar.
export module folia.markdown.parser_html_source_view;
import std;
import folia.markdown.parser_source_session;
import folia.markdown.parser_leaf_line_view;

export namespace folia::parser_source_detail {

class HtmlSourceView {
    std::u32string source_;
    std::vector<LeafSourcePiece> pieces_;
    const LeafSourceAccumulator* mapping_ = nullptr;
    std::optional<std::size_t> empty_physical_offset_;
public:
    explicit HtmlSourceView(const LeafSourceAccumulator& leaf)
        : source_(leaf.materialize()),
          pieces_(leaf.pieces().begin(), leaf.pieces().end()), mapping_(&leaf) {
        if (source_.size() != leaf.size())
            throw std::logic_error("HTML leaf materialization length mismatch");
        if (pieces_.empty()) empty_physical_offset_ = leaf.physical_offset(0);
    }

    std::u32string_view source() const noexcept { return source_; }
    std::size_t size() const noexcept { return source_.size(); }
    const LeafSourceAccumulator& physical_map() const noexcept { return *mapping_; }
    std::u32string take_source() && { return std::move(source_); }

    std::optional<std::size_t> physical_offset(std::size_t local) const {
        if (local > source_.size()) return std::nullopt;
        if (pieces_.empty()) return empty_physical_offset_;
        if (local == source_.size()) return pieces_.back().physical.end;
        const auto next = std::ranges::upper_bound(
            pieces_, local, {}, &LeafSourcePiece::source_begin);
        if (next == pieces_.begin()) return std::nullopt;
        const auto& piece = *std::prev(next);
        return piece.physical.begin + local - piece.source_begin;
    }

    std::optional<std::size_t> physical_offset_upstream(std::size_t local) const {
        if (local > source_.size()) return std::nullopt;
        if (pieces_.empty()) return empty_physical_offset_;
        if (local == 0) return pieces_.front().physical.begin;
        for (const auto& piece : pieces_) {
            const auto local_end = piece.source_begin + piece.physical.size();
            if (local <= local_end)
                return piece.physical.begin + local - piece.source_begin;
        }
        return pieces_.back().physical.end;
    }

    std::optional<PhysicalSourceRange> physical_range(
        std::size_t local_begin, std::size_t local_end) const {
        if (local_end < local_begin || local_end > source_.size()) return std::nullopt;
        const auto begin = physical_offset(local_begin);
        const auto end = physical_offset_upstream(local_end);
        if (!begin || !end || *end < *begin) return std::nullopt;
        // This range intentionally spans removed container prefixes between
        // its mapped endpoints. Leaf ownership remains piecewise and exact.
        return PhysicalSourceRange{*begin, *end};
    }

    std::optional<std::u32string_view> slice(
        std::size_t local_begin, std::size_t local_end) const noexcept {
        if (local_end < local_begin || local_end > source_.size()) return std::nullopt;
        return std::u32string_view(source_).substr(local_begin, local_end - local_begin);
    }
};

}
