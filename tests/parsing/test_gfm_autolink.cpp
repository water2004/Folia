#include <algorithm>
#include <functional>
#include <ranges>
#include <string>
#include <vector>

#include "support/folia_test.hpp"

import folia.markdown.inline_cst;
import folia.markdown.inline_document;
import folia.markdown.inline_parser;
import folia.markdown.utf;
import folia.markdown.gfm_autolink;

using namespace boost::ut;
using namespace folia;

suite gfm_autolink_tests = [] {
    "recognizes_published_gfm_url_email_and_xmpp_examples"_test = [] {
        struct Case { std::u32string source; std::size_t start; std::u32string shown; std::u32string href; };
        const std::vector<Case> cases{
            {U"www.commonmark.org", 0, U"www.commonmark.org", U"http://www.commonmark.org"},
            {U"Visit www.commonmark.org/help for information", 6, U"www.commonmark.org/help", U"http://www.commonmark.org/help"},
            {U"www.google.com/search?q=Markup+(business)))", 0, U"www.google.com/search?q=Markup+(business)", U"http://www.google.com/search?q=Markup+(business)"},
            {U"http://commonmark.org", 0, U"http://commonmark.org", U"http://commonmark.org"},
            {U"foo@bar.baz", 0, U"foo@bar.baz", U"mailto:foo@bar.baz"},
            {U"hello+xyz@mail.example", 0, U"hello+xyz@mail.example", U"mailto:hello+xyz@mail.example"},
            {U"mailto:a.b-c_d@a.b.", 0, U"mailto:a.b-c_d@a.b", U"mailto:a.b-c_d@a.b"},
            {U"xmpp:foo@bar.baz/txt/bin", 0, U"xmpp:foo@bar.baz/txt", U"xmpp:foo@bar.baz/txt"},
        };
        for (const auto& test : cases) {
            const auto match = scan_gfm_autolink(test.source, test.start, test.source.size());
            expect(fatal(match.has_value()));
            expect(test.source.substr(match->range.start, match->range.length()) == test.shown);
            expect(match->content_range == match->range);
            expect(match->href == test.href);
        }
    };

    "trims_gfm_delimiters_and_entities_and_rejects_invalid_hosts"_test = [] {
        const std::u32string punct = U"www.google.com/a.b.";
        auto match = scan_gfm_autolink(punct, 0, punct.size());
        expect(fatal(match.has_value()));
        expect(punct.substr(match->range.start, match->range.length()) == U"www.google.com/a.b");
        const std::u32string entity = U"www.google.com/search?q=x&hl;";
        match = scan_gfm_autolink(entity, 0, entity.size());
        expect(fatal(match.has_value()));
        expect(entity.substr(match->range.start, match->range.length()) == U"www.google.com/search?q=x");
        const std::u32string invalid = U"hello@mail+xyz.example";
        expect(!scan_gfm_autolink(invalid, 0, invalid.size()).has_value());
        const std::u32string invalid_suffix = U"a.b-c_d@a.b-";
        expect(!scan_gfm_autolink(invalid_suffix, 0, invalid_suffix.size()).has_value());
        const std::u32string underscore_host = U"www.example._com";
        expect(!scan_gfm_autolink(underscore_host, 0, underscore_host.size()).has_value());
        const std::u32string earlier_underscore = U"www._xxx.yyy.zzz";
        expect(scan_gfm_autolink(earlier_underscore, 0, earlier_underscore.size()).has_value());
        const std::u32string short_but_dotted = U"www.example";
        expect(scan_gfm_autolink(short_but_dotted, 0, short_but_dotted.size()).has_value());
        const std::u32string missing_domain = U"www.";
        expect(!scan_gfm_autolink(missing_domain, 0, missing_domain.size()).has_value());
    };

    "matches_all_fourteen_published_gfm_autolink_examples"_test = [] {
        struct Case { std::u32string input; std::u32string linked; };
        const std::vector<Case> cases{
            {U"www.commonmark.org", U"www.commonmark.org"},
            {U"Visit www.commonmark.org/help for more information.", U"www.commonmark.org/help"},
            {U"Visit www.commonmark.org.", U"www.commonmark.org"},
            {U"www.google.com/search?q=Markup+(business)))", U"www.google.com/search?q=Markup+(business)"},
            {U"www.google.com/search?q=(business))+ok", U"www.google.com/search?q=(business))+ok"},
            {U"www.google.com/search?q=commonmark&hl=en", U"www.google.com/search?q=commonmark&hl=en"},
            {U"www.google.com/search?q=commonmark&hl;", U"www.google.com/search?q=commonmark"},
            {U"www.commonmark.org/he<lp", U"www.commonmark.org/he"},
            {U"https://encrypted.google.com/search?q=Markup+(business))", U"https://encrypted.google.com/search?q=Markup+(business)"},
            {U"foo@bar.baz", U"foo@bar.baz"},
            {U"hello+xyz@mail.example", U"hello+xyz@mail.example"},
            {U"a.b-c_d@a.b.", U"a.b-c_d@a.b"},
            {U"xmpp:foo@bar.baz/txt@bin.com", U"xmpp:foo@bar.baz/txt@bin.com"},
            {U"xmpp:foo@bar.baz/txt/bin", U"xmpp:foo@bar.baz/txt"},
        };
        for (const auto& test : cases) {
            const auto start = test.input.find(test.linked);
            const auto match = scan_gfm_autolink(test.input, start, test.input.size());
            expect(fatal(match.has_value()));
            expect(test.input.substr(match->range.start, match->range.length()) == test.linked);
        }
        const std::u32string invalid = U"hello@mail+xyz.example";
        expect(!scan_gfm_autolink(invalid, 0, invalid.size()).has_value());
    };

    "parser_emits_lossless_gfm_nodes_only_in_allowed_contexts"_test = [] {
        InlineParseContext context;
        const auto source = U"Visit www.commonmark.org. [www.example.com](https://target.test) ![www.image.test](img.png) <span>www.html.test</span> <a href=\"https://target.test\">www.anchor.test</a>";
        const auto tree = parse_inline(source, context);
        expect(serialize_lossless(tree, source) == source);
        std::vector<std::string> hrefs;
        std::function<void(const InlineCstNodes&)> visit = [&](const InlineCstNodes& nodes) {
            for (const auto& node : nodes) {
                if (node.kind == InlineCstKind::Autolink) hrefs.push_back(node.semantics().href);
                visit(node.children);
            }
        };
        visit(tree.nodes);
        const std::vector<std::string> expected_hrefs{
            "http://www.commonmark.org", "http://www.image.test", "http://www.html.test"};
        expect(hrefs == expected_hrefs);
        context.dialect.gfm.autolinks = false;
        const auto commonmark = parse_inline(U"www.commonmark.org", context);
        expect(std::ranges::none_of(commonmark.nodes, [](const InlineCstNode& node) { return node.kind == InlineCstKind::Autolink; }));
    };

    "parser_decodes_and_encodes_bare_href_without_changing_source"_test = [] {
        InlineParseContext context;
        const auto source = U"http://google.com/å www.google.com/a_b www.google.com/a*b www.google.com/?q=x&amp;y www.google.com/?q=x&amp;";
        const auto tree = parse_inline(source, context);
        expect(serialize_lossless(tree, source) == source);
        std::vector<std::string> hrefs;
        for (const auto& node : tree.nodes) {
            if (node.kind == InlineCstKind::Autolink) hrefs.push_back(node.semantics().href);
        }
        expect(hrefs.size() == 5_u);
        expect(hrefs[0] == "http://google.com/%C3%A5");
        expect(hrefs[1] == "http://www.google.com/a_b");
        expect(hrefs[2] == "http://www.google.com/a*b");
        expect(hrefs[3] == "http://www.google.com/?q=x&y");
        expect(hrefs[4] == "http://www.google.com/?q=x&");

        const auto emoji_source = U"http://🍄.ga/ http://x🍄.ga/";
        const auto emoji_tree = parse_inline(emoji_source, context);
        expect(serialize_lossless(emoji_tree, emoji_source) == emoji_source);
        expect(emoji_tree.nodes[0].semantics().href == "http://%F0%9F%8D%84.ga/");
        expect(emoji_tree.nodes[2].semantics().href == "http://x%F0%9F%8D%84.ga/");
    };

    "bare_url_scan_stops_at_inline_code_and_emphasis_nodes"_test = [] {
        InlineParseContext context;
        const std::u32string_view source = U"http://example.com/a`code`b http://example.com/a*bold*";
        const auto tree = parse_inline(source, context);
        expect(serialize_lossless(tree, source) == source);
        expect(tree.nodes.size() >= 5_u);
        expect(tree.nodes[0].kind == InlineCstKind::Autolink);
        expect(source.substr(tree.nodes[0].range.start, tree.nodes[0].range.length()).compare(U"http://example.com/a") == 0);
        expect(tree.nodes[1].kind == InlineCstKind::CodeSpan);
        expect(tree.nodes[3].kind == InlineCstKind::Autolink);
        expect(source.substr(tree.nodes[3].range.start, tree.nodes[3].range.length()).compare(U"http://example.com/a") == 0);
        expect(tree.nodes[4].kind == InlineCstKind::Emphasis);
    };

    "bare_url_boundaries_follow_existing_mixed_inline_syntax_pairs"_test = [] {
        InlineParseContext context;
        const std::u32string_view source = U"http://example.test/a\\(math\\)tail http://example.test/b~strike~tail http://example.test/c**strong**tail";
        const auto tree = parse_inline(source, context);
        expect(serialize_lossless(tree, source) == source);

        std::vector<std::u32string_view> autolinks;
        std::size_t math_nodes = 0;
        std::size_t strike_nodes = 0;
        std::size_t strong_nodes = 0;
        std::function<void(const InlineCstNodes&)> visit = [&](const InlineCstNodes& nodes) {
            for (const auto& node : nodes) {
                if (node.kind == InlineCstKind::Autolink)
                    autolinks.push_back(source.substr(node.range.start, node.range.length()));
                if (node.kind == InlineCstKind::InlineMath) ++math_nodes;
                if (node.kind == InlineCstKind::Strikethrough) ++strike_nodes;
                if (node.kind == InlineCstKind::Strong) ++strong_nodes;
                visit(node.children);
            }
        };
        visit(tree.nodes);
        const std::vector<std::u32string_view> expected{
            U"http://example.test/a", U"http://example.test/b", U"http://example.test/c"};
        expect(autolinks == expected);
        expect(math_nodes == 1_u);
        expect(strike_nodes == 1_u);
        expect(strong_nodes == 1_u);
    };
};
