export module folia.math.editor.syntax;
import std;

export namespace folia::math::editor {
    // All public offsets count Unicode scalar values in the supplied TeX
    // source. UTF-8 byte positions stay inside the Tree-sitter boundary.
    struct SourceRange {
        std::size_t start = 0, end = 0;
        friend bool operator==(SourceRange const&, SourceRange const&) = default;
    };
    enum class TokenKind { Command, Number, Identifier, Operator, Delimiter, Comment, Text, Environment };
    struct SyntaxToken {
        SourceRange range; TokenKind kind;
        friend bool operator==(SyntaxToken const&, SyntaxToken const&) = default;
    };
    struct BracketPair {
        SourceRange open, close;
        friend bool operator==(BracketPair const&, BracketPair const&) = default;
    };
    struct BracketMatch { SourceRange at; std::optional<SourceRange> mate; };
    struct SyntaxAnalysis {
        std::vector<SyntaxToken> tokens;
        std::vector<BracketPair> pairs;
        std::vector<SourceRange> unmatched;
        bool hasErrors = false;
        std::optional<BracketMatch> MatchAt(std::size_t caret) const;
    };
    // A small incremental editing parser. It never loads fonts, expands
    // macros, constructs MML or performs mathematical layout.
    class SyntaxSession {
    public:
        SyntaxSession();
        ~SyntaxSession();
        SyntaxSession(SyntaxSession const&) = delete;
        SyntaxSession& operator=(SyntaxSession const&) = delete;
        SyntaxAnalysis const& Update(std::u32string_view source);
        std::size_t ParseCount() const noexcept;
    private:
        struct State;
        std::unique_ptr<State> state;
    };
}
