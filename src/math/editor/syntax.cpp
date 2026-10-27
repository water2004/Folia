module;
#include "tree_sitter/api.h"
module folia.math.editor.syntax;
import std;
extern "C" TSLanguage const* tree_sitter_folia_tex();

namespace folia::math::editor {
    namespace {
        struct ParserDeleter { void operator()(TSParser* p) const { ts_parser_delete(p); } };
        struct TreeDeleter { void operator()(TSTree* p) const { ts_tree_delete(p); } };
        struct EncodedSource {
            std::string utf8;
            std::vector<std::size_t> toSource;
            explicit EncodedSource(std::u32string_view source) {
                for (std::size_t i = 0; i < source.size(); ++i) {
                    auto c = static_cast<std::uint32_t>(source[i]);
                    if (c > 0x10FFFF || (c >= 0xD800 && c <= 0xDFFF))
                        throw std::invalid_argument("TeX editing source contains an invalid Unicode scalar");
                    auto append = [&](std::uint32_t byte) { utf8.push_back(static_cast<char>(byte)); toSource.push_back(i); };
                    if (c < 0x80) append(c);
                    else if (c < 0x800) { append(0xC0 | (c >> 6)); append(0x80 | (c & 63)); }
                    else if (c < 0x10000) { append(0xE0 | (c >> 12)); append(0x80 | ((c >> 6) & 63)); append(0x80 | (c & 63)); }
                    else { append(0xF0 | (c >> 18)); append(0x80 | ((c >> 12) & 63)); append(0x80 | ((c >> 6) & 63)); append(0x80 | (c & 63)); }
                }
                toSource.push_back(source.size());
                if (utf8.size() > (std::numeric_limits<std::uint32_t>::max)())
                    throw std::length_error("TeX editing source exceeds Tree-sitter's input limit");
            }
            SourceRange Range(TSNode node) const {
                return {toSource.at(ts_node_start_byte(node)), toSource.at(ts_node_end_byte(node))};
            }
        };
        TSPoint PointAt(std::string_view source, std::size_t end) {
            TSPoint point{};
            for (std::size_t i = 0; i < end; ++i)
                if (source[i] == '\n') { ++point.row; point.column = 0; } else ++point.column;
            return point;
        }
        bool Continuation(char c) { return (static_cast<unsigned char>(c) & 0xC0) == 0x80; }
        TSInputEdit ChangedRange(std::string_view before, std::string_view after) {
            std::size_t start = 0, oldEnd = before.size(), newEnd = after.size();
            while (start < oldEnd && start < newEnd && before[start] == after[start]) ++start;
            while (start && ((start < before.size() && Continuation(before[start]))
                         || (start < after.size() && Continuation(after[start])))) --start;
            while (oldEnd > start && newEnd > start && before[oldEnd - 1] == after[newEnd - 1]) { --oldEnd; --newEnd; }
            while (oldEnd < before.size() && Continuation(before[oldEnd])) { ++oldEnd; ++newEnd; }
            return {static_cast<std::uint32_t>(start), static_cast<std::uint32_t>(oldEnd), static_cast<std::uint32_t>(newEnd),
                PointAt(before, start), PointAt(before, oldEnd), PointAt(after, newEnd)};
        }
        TSNode Field(TSNode node, char const* name) {
            return ts_node_child_by_field_name(node, name, static_cast<std::uint32_t>(std::char_traits<char>::length(name)));
        }
        bool Present(TSNode node) { return !ts_node_is_null(node) && !ts_node_is_missing(node); }
        struct Delimiter { SourceRange range; int family; bool open; std::string_view name{}; };
        std::optional<std::pair<int, bool>> PlainDelimiter(std::string_view type) {
            if (type == "{") return {{0, true}};
            if (type == "}") return {{0, false}};
            if (type == "(") return {{1, true}};
            if (type == ")") return {{1, false}};
            if (type == "[") return {{2, true}};
            if (type == "]") return {{2, false}};
            return std::nullopt;
        }
        SyntaxAnalysis Analyze(TSNode root, EncodedSource const& source) {
            SyntaxAnalysis result;
            result.hasErrors = ts_node_has_error(root);
            struct Visit { TSNode node; bool text, environment, delimiter; };
            std::vector<Visit> pending{{root, false, false, false}};
            std::vector<Delimiter> delimiters;
            while (!pending.empty()) {
                auto [node, text, environment, delimiter] = pending.back(); pending.pop_back();
                if (!Present(node)) continue;
                std::string_view type = ts_node_type(node);
                auto count = ts_node_child_count(node);
                if (type == "left_delimiter" || type == "right_delimiter") {
                    auto symbol = Field(node, "delimiter");
                    auto range = source.Range(Present(symbol) ? symbol : node);
                    delimiters.push_back({range, 3, type == "left_delimiter"});
                }
                if (type == "begin" || type == "end") {
                    auto name = Field(node, "name");
                    if (Present(name)) delimiters.push_back({source.Range(node), 4, type == "begin",
                        std::string_view(source.utf8).substr(ts_node_start_byte(name), ts_node_end_byte(name) - ts_node_start_byte(name))});
                }
                if (type == "sized_delimiter") {
                    auto symbol = Field(node, "delimiter");
                    if (Present(symbol) && ts_node_child_count(symbol))
                        if (auto shape = PlainDelimiter(ts_node_type(ts_node_child(symbol, 0))))
                            delimiters.push_back({source.Range(symbol), shape->first, shape->second});
                }
                if (count == 0) {
                    auto range = source.Range(node);
                    if (range.start == range.end) continue;
                    TokenKind kind = TokenKind::Operator;
                    if (type == "command" || type == "incomplete_command" || type.starts_with("\\")) kind = TokenKind::Command;
                    else if (type == "comment") kind = TokenKind::Comment;
                    else if (type == "number") kind = TokenKind::Number;
                    else if (type == "identifier") kind = environment ? TokenKind::Environment : text ? TokenKind::Text : TokenKind::Identifier;
                    else if (PlainDelimiter(type) || delimiter) kind = TokenKind::Delimiter;
                    result.tokens.push_back({range, kind});
                    if (!delimiter) if (auto shape = PlainDelimiter(type))
                        delimiters.push_back({range, shape->first, shape->second});
                    continue;
                }
                for (std::uint32_t i = count; i > 0; --i) {
                    auto child = ts_node_child(node, i - 1);
                    auto field = ts_node_field_name_for_child(node, i - 1);
                    bool content = type == "text" && field && std::string_view(field) == "content";
                    pending.push_back({child, text || content, environment || type == "environment_name",
                        delimiter || type == "left_delimiter" || type == "right_delimiter" || type == "sized_delimiter"});
                }
            }
            // Pair syntax tokens, including tokens recovered beneath ERROR
            // nodes. Escaped braces and comments are never raw-source scanned.
            std::ranges::sort(delimiters, {}, [](auto const& item) { return item.range.start; });
            std::vector<Delimiter> open;
            for (auto const& token : delimiters) {
                if (token.open) open.push_back(token);
                else if (!open.empty() && open.back().family == token.family && open.back().name == token.name) {
                    result.pairs.push_back({open.back().range, token.range}); open.pop_back();
                } else result.unmatched.push_back(token.range);
            }
            for (auto const& token : open) result.unmatched.push_back(token.range);
            return result;
        }
    }

    struct SyntaxSession::State {
        std::unique_ptr<TSParser, ParserDeleter> parser{ts_parser_new()};
        std::unique_ptr<TSTree, TreeDeleter> tree;
        std::u32string source;
        std::string utf8;
        SyntaxAnalysis analysis;
        std::size_t parses = 0;
        State() {
            if (!parser || !ts_parser_set_language(parser.get(), tree_sitter_folia_tex()))
                throw std::runtime_error("TeX editing grammar could not initialize");
        }
    };
    SyntaxSession::SyntaxSession() : state(std::make_unique<State>()) {}
    SyntaxSession::~SyntaxSession() = default;
    std::size_t SyntaxSession::ParseCount() const noexcept { return state->parses; }
    SyntaxAnalysis const& SyntaxSession::Update(std::u32string_view source) {
        if (state->tree && state->source == source) return state->analysis;
        EncodedSource encoded(source);
        auto previous = std::unique_ptr<TSTree, TreeDeleter>(state->tree ? ts_tree_copy(state->tree.get()) : nullptr);
        if (previous) {
            auto edit = ChangedRange(state->utf8, encoded.utf8);
            ts_tree_edit(previous.get(), &edit);
        }
        auto tree = std::unique_ptr<TSTree, TreeDeleter>(ts_parser_parse_string(state->parser.get(), previous.get(),
            encoded.utf8.data(), static_cast<std::uint32_t>(encoded.utf8.size())));
        if (!tree) throw std::runtime_error("TeX editing parse could not complete");
        auto analysis = Analyze(ts_tree_root_node(tree.get()), encoded);
        state->tree = std::move(tree);
        state->source = source; state->utf8 = std::move(encoded.utf8);
        state->analysis = std::move(analysis); ++state->parses;
        return state->analysis;
    }
    std::optional<BracketMatch> SyntaxAnalysis::MatchAt(std::size_t caret) const {
        std::optional<BracketMatch> best;
        auto consider = [&](SourceRange at, std::optional<SourceRange> mate) {
            if (caret < at.start || caret > at.end) return;
            if (!best || at.end - at.start < best->at.end - best->at.start) best = BracketMatch{at, mate};
        };
        for (auto const& pair : pairs) { consider(pair.open, pair.close); consider(pair.close, pair.open); }
        for (auto range : unmatched) consider(range, std::nullopt);
        return best;
    }
}
