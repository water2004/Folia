// Source assembly and indentation context for code and math leaves.
// It owns no Markdown grammar and never creates a nested Parser.
export module folia.markdown.parser_code_math_leaf_source;
import std;
import folia.markdown.block_source;
import folia.markdown.parser_source_session;
import folia.markdown.parser_leaf_line_view;

export namespace folia::parser_source_detail {

struct LeafAcceptedLine {
    std::size_t source_begin = 0;
    std::size_t source_end = 0;
};

// Code and math blocks use the existing BlockSourceDocument parser. This
// accumulator only replaces the nested-parser substring with root spans and
// keeps the old recognizer's explicit EOL ownership decision.
class CodeMathLeafSource {
    LeafSourceAccumulator source_;
    std::vector<BlockLineIndentation> indentation_contexts_;
public:
    explicit CodeMathLeafSource(const RootPhysicalSource& root)
        : source_(root) {}

    std::size_t size() const { return source_.size(); }

    LeafAcceptedLine append_content_range(
        const LeafLine& line, std::size_t begin, std::size_t end) {
        const auto source_begin = size();
        source_.append_content_range(line, begin, end);
        return {source_begin, size()};
    }

    LeafAcceptedLine append(const LeafLine& line, bool include_ending) {
        const auto begin = size();
        source_.append_content(line);
        if (include_ending) source_.append_ending(line);
        return {begin, size()};
    }

    void append_ending(const LeafLine& line) { source_.append_ending(line); }

    LeafAcceptedLine append_indented(const LeafLine& line, bool include_ending) {
        const auto begin = size();
        const auto cursor = line.content_cursor;
        indentation_contexts_.push_back({begin, cursor.physical_column, cursor.residual_columns});
        source_.append_content(line);
        if (include_ending) source_.append_ending(line);
        return {begin, size()};
    }

    // Offsets are into the final block-local source. Delegate both mappings to
    // the shared accumulator so removed-prefix boundary affinity stays uniform.
    std::optional<std::size_t> physical_offset(std::size_t source_offset) const {
        return source_.physical_offset(source_offset);
    }

    std::optional<PhysicalSourceRange> physical_range(std::size_t begin, std::size_t end) const {
        return source_.physical_range(begin, end);
    }

    std::u32string materialize() const { return source_.materialize(); }
    LeafSourceAccumulator const& view() const { return source_; }
    std::vector<BlockLineIndentation> take_indentation_contexts() {
        return std::move(indentation_contexts_);
    }
};

// The old code recognizers advance to the next line after accepting it. Keep
// this helper explicit so the closing fence's EOL can remain outside the
// BlockSourceDocument while still being accepted by the frame engine.
inline void append_opening_and_body_line(
    CodeMathLeafSource& source, const LeafLine& line) {
    source.append(line, true);
}
inline void append_indented_code_line(
    CodeMathLeafSource& source, const LeafLine& line) {
    source.append_indented(line, true);
}
inline void append_fence_body_line(
    CodeMathLeafSource& source, const LeafLine& line) {
    source.append(line, true);
}
inline void append_fence_closing_line_without_ending(
    CodeMathLeafSource& source, const LeafLine& line) {
    source.append(line, false);
}
inline void append_math_closing_line_without_ending(
    CodeMathLeafSource& source, const LeafLine& line) {
    source.append(line, false);
}

inline BlockSourceDocument finish_code_math_source(
    CodeMathLeafSource& source, BlockSourceKind kind,
    std::optional<std::size_t> block_source_end = std::nullopt) {
    auto materialized = source.materialize();
    if (block_source_end) {
        if (*block_source_end > materialized.size())
            throw std::out_of_range("block source end is outside accumulated leaf source");
        materialized.resize(*block_source_end);
    }
    return make_block_source(std::move(materialized), kind, source.take_indentation_contexts());
}

}
