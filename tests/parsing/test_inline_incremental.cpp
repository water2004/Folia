#include <random>
#include "support/folia_test.hpp"
import folia.markdown.inline_document;
import folia.markdown.inline_cst;
import folia.markdown.ids;
import folia.markdown.text_edit;
import folia.markdown.inline_parser;
import folia.markdown.inline_reparse;
import folia.markdown.instrumentation;
import folia.core.inline_source_edit;

using namespace folia;
using namespace boost::ut;

namespace {
bool SameSyntax(InlineCstNodes const& a, InlineCstNodes const& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i].kind != b[i].kind || a[i].range != b[i].range || a[i].status != b[i].status
            || a[i].delimiter_ranges() != b[i].delimiter_ranges() || a[i].semantics() != b[i].semantics()
            || !SameSyntax(a[i].children, b[i].children)) return false;
    }
    return true;
}

bool SameTokens(InlineCstTree const& a, InlineCstTree const& b) {
    return std::ranges::equal(a.tokens, b.tokens, [](auto x, auto y) {
        return x.kind() == y.kind() && x.end() == y.end();
    });
}

suite incremental_inline_tests = [] {
    "paged inline token splices preserve distant offsets and token kinds"_test = [] {
        InlineTokens tokens;
        for (std::size_t end = 1; end <= 800; ++end)
            tokens.emplace_back(TokenKind::Text, end);
        constexpr auto distant = std::size_t{1} << 30;
        tokens.emplace_back(TokenKind::Escape, distant);
        tokens.emplace_back(TokenKind::Entity, distant + 5);
        auto original = tokens;

        InlineTokens replacement;
        replacement.emplace_back(TokenKind::Whitespace, 4);
        replacement.emplace_back(TokenKind::Text, 14);
        tokens.replace(249, 261, replacement, 249, 2);

        expect(tokens.size() == 792u);
        expect(tokens[248].end() == 249u);
        expect(tokens[249].kind() == TokenKind::Whitespace);
        expect(tokens[249].end() == 253u);
        expect(tokens[250].end() == 263u);
        expect(tokens[251].end() == 264u);
        expect(tokens[790].kind() == TokenKind::Escape);
        expect(tokens[790].end() == distant + 2);
        expect(tokens[791].end() == distant + 7);
        expect(original[800].end() == distant);
        expect(!tokens.only_kinds(512, tokens.size(), 1u << static_cast<unsigned>(TokenKind::Text)));
    };

    "incremental inline edits agree with complete parsing through temporary syntax"_test = [] {
        std::vector<std::u32string> sources{
            U"alpha beta gamma\nnext **bold** line", U"alpha beta gamma",
            U"**alpha *beta gamma* delta** ~~epsilon zeta~~",
            U"==alpha **beta** gamma== and ==unfinished",
            U"[missing] [known] [name](url) ![alt](url)", U"before <span unfinished after",
            U"word *a\nb* and **unclosed", U"text &amp; \\*more* $x$",
            U"x www.example.org y\nlast line", U"\U0001f642中文 e\u0301 words\r\nnext",
            U"alpha [label](<url> 'title') omega", U"<b>plain text</b>"};
        std::vector<std::u32string> replacements{U"x", U"", U"**", U"==", U"=", U" ", U"\n", U"$", U"&amp;",
            U"<", U"[", U"\\", U"www.example.org", U"\U0001f642", U"\u0301"};
        std::mt19937 random(913274);
        for (int trial = 0; trial < 5000; ++trial) {
            auto const& source = sources[random() % sources.size()];
            auto mode = static_cast<InlineSyntaxMode>(random() % 3);
            InlineParseContext context; context.syntax_mode = mode;
            context.resolve_link_label = [](std::string const& label) -> std::optional<InlineLinkDef> {
                return label == "known" ? std::optional<InlineLinkDef>{{"target", {}}} : std::nullopt;
            };
            InlineDocument document{source, {}, mode};
            reparse_inline_document(document, context);
            auto original = document.tree;
            auto start = random() % (source.size() + 1);
            auto end = (std::min)(source.size(), start + random() % 5);
            TextEdit edit{NodeId{7}, {start, end}, replacements[random() % replacements.size()]};
            auto applied = apply_inline_source_edit(NodeId{7}, document, edit, context);
            auto reference = parse_inline(document.source, context);
            expect(fatal(SameSyntax(document.tree.nodes, reference.nodes))) << trial;
            expect(fatal(SameTokens(document.tree, reference))) << trial;
            expect(fatal(bool(flatten_tokens(document.tree, document.source) == document.source))) << trial;
            apply_inline_source_edit(NodeId{7}, document, applied.inverse, context);
            expect(fatal(bool(document.source == source))) << trial;
            expect(fatal(SameSyntax(document.tree.nodes, original.nodes))) << trial;
            expect(fatal(SameTokens(document.tree, original))) << trial;
        }
    };

    "incremental inline edits retain nested presentation wrappers and exact delimiter ranges"_test = [] {
        for (auto const& delimiters : {std::pair{U"**", U"**"}, std::pair{U"_", U"_"},
            std::pair{U"~~", U"~~"}, std::pair{U"==", U"=="}, std::pair{U"**a *", U"* z**"}}) {
            InlineDocument document;
            document.source = delimiters.first;
            auto begin = document.source.size();
            while (document.source.size() < 1024 * 1024) document.source += U"ordinary words ";
            document.source += U"last";
            document.source += delimiters.second;
            document.source += U" after [link](url) $x$";
            auto originalSource = document.source;
            InlineParseContext context;
            reparse_inline_document(document, context);
            expect(fatal(!document.tree.nodes.empty()
                && document.tree.nodes.front().status == ParseStatus::Complete
                && !document.tree.nodes.front().children.empty()));
            auto originalTree = document.tree;
            auto identities = inline_source_edit_detail::node_identities(document.tree.nodes);
            reset_core_operation_counters();
            auto applied = apply_inline_source_edit(NodeId{7}, document,
                {NodeId{7}, {begin + 3, begin + 4}, U"XY"}, context);
            auto counters = read_core_operation_counters();
            expect(counters.inline_reparses == 1_u);
            expect(counters.inline_scanned_characters < 128_u);
            expect(fatal(applied.inline_identities && applied.inline_identities->preserved));
            expect(std::ranges::equal(inline_source_edit_detail::node_identities(document.tree.nodes), identities));
            auto reference = parse_inline(document.source, context);
            expect(SameSyntax(document.tree.nodes, reference.nodes));
            expect(SameTokens(document.tree, reference));
            // The saved tree shares delimiter metadata; editing must detach it.
            auto originalReference = parse_inline(originalSource, context);
            expect(SameSyntax(originalTree.nodes, originalReference.nodes));
            auto inverse = apply_inline_source_edit(NodeId{7}, document, applied.inverse, context);
            expect(fatal(inverse.inline_identities && inverse.inline_identities->preserved));
            expect(bool(document.source == originalSource));
            expect(SameSyntax(document.tree.nodes, originalTree.nodes));
            expect(SameTokens(document.tree, originalTree));
            expect(std::ranges::equal(inline_source_edit_detail::node_identities(document.tree.nodes), identities));
        }
    };

    "literal edits reparse a bounded token window and keep compact identity history"_test = [] {
        for (bool physicalBreaks : {false, true}) {
            InlineDocument document;
            while (document.source.size() < 1024 * 1024)
                document.source += physicalBreaks ? U"Sample paragraph with words.\n" : U"Sample paragraph with words. ";
            InlineParseContext context;
            reparse_inline_document(document, context);
            auto identities = inline_source_edit_detail::node_identities(document.tree.nodes);
            reset_core_operation_counters();
            auto applied = apply_inline_source_edit(NodeId{7}, document, {NodeId{7}, {3, 3}, U"X"}, context);
            auto counters = read_core_operation_counters();
            expect(counters.inline_reparses == 1_u);
            expect(counters.inline_scanned_characters < 128_u);
            expect(fatal(applied.inline_identities.has_value()));
            expect(applied.inline_identities->preserved);
            expect(applied.inline_identities->before.empty() && applied.inline_identities->after.empty());
            expect(std::ranges::equal(inline_source_edit_detail::node_identities(document.tree.nodes), identities));
            auto reference = parse_inline(document.source, context);
            expect(SameSyntax(document.tree.nodes, reference.nodes));
            expect(SameTokens(document.tree, reference));
        }
    };
};
}
