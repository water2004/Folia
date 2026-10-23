#include <string>
#include <vector>

#include "support/folia_test.hpp"
import folia.core.search;
import folia.core.source_editor;
import folia.core.editor;
import folia.math.util.regexp_host;

using namespace folia;
using namespace boost::ut;

namespace {
SearchOptions exact_options() {
    SearchOptions options;
    options.case_sensitive = true;
    return options;
}
SearchOptions regex_options() {
    auto options = exact_options();
    options.regular_expression = true;
    return options;
}
}

suite search_engine_tests = [] {

"source search matches exact markdown and expands unicode regex captures"_test = [] {
    SourceEditor source(U"# **Alpha** <strong>中文 42</strong>");
    auto marker = search_text(source.source(), U"**", exact_options());
    expect(fatal(marker.valid()));
    expect(fatal(bool(marker.matches.size() == 2u)));
    auto tag = search_text(source.source(), U"<strong>", exact_options());
    expect(fatal(tag.valid()));
    expect(fatal(bool(tag.matches.size() == 1u)));

    auto regex = search_text_for_replacement(
        source.source(), U"(中文) ([0-9]+)", U"$2-$1", regex_options());
    expect(fatal(regex.valid()));
    expect(fatal(bool(regex.matches.size() == 1u)));
    if (!regex.matches.empty()) {
        expect(bool(regex.matches.front().matched_text == U"中文 42"));
        expect(bool(regex.matches.front().replacement.has_value()));
        if (regex.matches.front().replacement) {
            expect(bool(*regex.matches.front().replacement == U"42-中文"));
        }
    }
};

"invalid regular expressions are reported without partial matches"_test = [] {
    auto source = search_text(U"before needle after", U"([", regex_options());
    expect(!source.valid());
    expect(bool(source.matches.empty()));
};

"regex policies remain independent when search and math share the executable"_test = [] {
    auto options = regex_options();
    options.case_sensitive = false;
    const auto kelvin = search_text(U"KK", U"k", options);
    const auto long_s = search_text(U"Sſ", U"s", options);
    expect(kelvin.valid() && kelvin.matches.size() == 2u);
    expect(long_s.valid() && long_s.matches.size() == 2u);

    folia::math::regexp_host::RegExp math_k(u"k", u"i");
    folia::math::regexp_host::RegExp math_s(u"s", u"i");
    expect(math_k.exec(u"K").has_value());
    expect(math_s.exec(u"S").has_value());
    expect(!math_k.exec(u"K").has_value());
    expect(!math_s.exec(u"ſ").has_value());
};

"source replace all is one exact reversible flat-source transaction"_test = [] {
    Editor editor("one **one** <one>");
    expect(fatal(editor.enter_source_mode()));
    const auto* source=editor.source_editor();
    auto found = search_text_for_replacement(
        source->source(), U"one", U"two", exact_options());
    expect(fatal(found.valid()));
    std::vector<SourceReplacement> replacements;
    for (auto const& match : found.matches) {
        replacements.push_back({match.range, *match.replacement});
    }
    auto const before_selection = source->selection();
    expect(fatal(editor.replace_all_source(replacements)));
    expect(bool(source->source() == U"two **two** <two>"));
    expect(fatal(editor.undo()));
    expect(bool(source->source() == U"one **one** <one>"));
    expect(bool(source->selection() == before_selection));
    expect(fatal(editor.redo()));
    expect(bool(source->source() == U"two **two** <two>"));
};

};
