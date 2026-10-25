// Presentation identities supplied by the CST-to-render-model projection.
// A character spelling alone never authorizes a marker replacement.
export module folia.core.syntax_marker;
import std;

export namespace folia {
enum class SyntaxMarker : std::uint8_t {
    None, Star, DoubleStar, Underscore, DoubleUnderscore, Tilde, DoubleTilde,
    Backtick, DoubleBacktick, TripleBacktick, Dollar, DoubleDollar,
    MathOpen, MathClose, MathBracketOpen, MathBracketClose, Count
};

inline constexpr std::array<std::string_view, static_cast<std::size_t>(SyntaxMarker::Count)> syntax_marker_names{
    "", "star", "doubleStar", "underscore", "doubleUnderscore", "tilde", "doubleTilde",
    "backtick", "doubleBacktick", "tripleBacktick", "dollar", "doubleDollar",
    "mathOpen", "mathClose", "mathBracketOpen", "mathBracketClose"
};

struct SyntaxMarkerTheme {
    float size_em = 1.05f;
    float gap_em = .30f;
    std::array<std::string, static_cast<std::size_t>(SyntaxMarker::Count)> svg;
};
}
