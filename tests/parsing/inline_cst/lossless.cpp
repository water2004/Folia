#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "support/folia_test.hpp"
import folia.markdown.ids;
import folia.markdown.dialect;
import folia.markdown.inline_cst;
import folia.markdown.inline_document;
import folia.markdown.inline_parser;
import folia.core.inline_source_edit;
import folia.markdown.selection;
import folia.markdown.text_edit;
import folia.markdown.utf;

using namespace folia;
using namespace boost::ut;

#include "parsing/support/inline_cst.hpp"

suite parsing_inline_cst_lossless_tests = [] {

"lossless representative editing states"_test = [] {
    const std::vector<std::u32string> sources{
        U"", U"abc", U"*abc*", U"_abc_", U"**abc**", U"__abc__",
        U"**", U"**abc", U"a***b***c", U"~~abc~~", U"~~abc",
        U"`abc`", U"`abc", U"[title](url)", U"[title](<url>)",
        U"[title](url \"name\")", U"[title](", U"![alt](url)",
        U"$abc$", U"$abc", U"\\*abc\\*", U"&amp;", U"a\\**b*",
        U"two  \nlines", U"two\\\nlines", U"'文本'", U"\"文本\"",
    };

    for (const auto& source : sources) {
        expect(is_lossless(source)) << "source=" << cps_to_utf8(source);
    }
};

"unclosed syntax remains structural"_test = [] {
    const std::vector<std::u32string> sources{U"**", U"**abc", U"*", U"`abc", U"[title](", U"$abc", U"~~abc"};
    for (const auto& source : sources) {
        const auto tree = parse_inline(source, test_context());
        const auto* incomplete = first_node(tree, InlineCstKind::Incomplete);
        expect(incomplete != nullptr) << "source=" << cps_to_utf8(source);
        if (incomplete) {
            expect(incomplete->status == ParseStatus::MissingCloser);
            expect(incomplete->range.valid_for(source.size()));
        }
    }
};

"original marker and link spellings survive"_test = [] {
    const auto stars_source = std::u32string{U"**abc**"};
    const auto underscores_source = std::u32string{U"__abc__"};
    const auto stars = parse_inline(stars_source, test_context());
    const auto underscores = parse_inline(underscores_source, test_context());
    const auto* star_strong = first_node(stars, InlineCstKind::Strong);
    const auto* underscore_strong = first_node(underscores, InlineCstKind::Strong);
    expect(star_strong != nullptr);
    expect(underscore_strong != nullptr);
    if (star_strong && underscore_strong) {
        expect(stars_source.substr(star_strong->delimiter_ranges().opening.start, star_strong->delimiter_ranges().opening.length()) == U"**");
        expect(underscores_source.substr(underscore_strong->delimiter_ranges().opening.start, underscore_strong->delimiter_ranges().opening.length()) == U"__");
    }

    const std::vector<std::u32string> links{U"[title](url)", U"[title](<url>)", U"[title](url 'name')"};
    for (const auto& link : links) {
        const auto tree = parse_inline(link, test_context());
        expect(first_node(tree, InlineCstKind::Link) != nullptr);
        expect(serialize_lossless(tree, link) == link);
    }
};

"every character boundary remains lossless after insert and delete"_test = [] {
    const std::vector<std::u32string> samples{
        U"abc", U"**abc**", U"__abc__", U"**", U"**abc", U"a***b***c",
        U"`abc`", U"`abc", U"[title](url)", U"[title](", U"$abc$", U"$abc",
        U"~~abc~~", U"~~abc", U"\\*abc\\*",
    };
    for (const auto& original : samples) {
        for (std::size_t offset = 0; offset <= original.size(); ++offset) {
            auto inserted = original;
            apply_text_edit(inserted, TextEdit{NodeId{1}, {offset, offset}, U"x"});
            expect(is_lossless(inserted));
        }
        for (std::size_t offset = 0; offset < original.size(); ++offset) {
            auto deleted = original;
            apply_text_edit(deleted, TextEdit{NodeId{1}, {offset, offset + 1}, U""});
            expect(is_lossless(deleted));
        }
    }
};

"random source edits preserve token partition"_test = [] {
    std::mt19937_64 random{0xC57C57u};
    const std::u32string alphabet = U"abc *_~`[]()!$\\&;<>\n'\"😀";
    for (std::size_t iteration = 0; iteration < 500; ++iteration) {
        std::u32string source;
        const auto length = static_cast<std::size_t>(random() % 80);
        for (std::size_t i = 0; i < length; ++i) source.push_back(alphabet[random() % alphabet.size()]);
        expect(is_lossless(source));

        const auto offset = static_cast<std::size_t>(random() % (source.size() + 1));
        if ((random() & 1u) != 0 || source.empty()) {
            apply_text_edit(source, TextEdit{NodeId{1}, {offset, offset}, U"x"});
        } else {
            const auto end = (std::min)(source.size(), offset + 1);
            apply_text_edit(source, TextEdit{NodeId{1}, {offset, end}, U""});
        }
        expect(is_lossless(source));
    }
};

"inline document serializes authoritative source"_test = [] {
    InlineDocument document{U"[title](<url>)", {}};
    reparse_inline_document(document, test_context());
    expect(document.serialize() == U"[title](<url>)");
    expect(flatten_tokens(document.tree, document.source) == document.source);
};

};
