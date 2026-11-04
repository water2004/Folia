#include "support/folia_test.hpp"
import folia.completion;

namespace {
using namespace boost::ut;
using namespace folia::completion;
suite completion_edit_tests = [] {
    "model edit uses exact following text rather than model supplied offsets"_test = [] {
        Context context{{}, "\\frac{", "}{}"};
        auto edit = decode_edit_response(UR"({"oldText":"}{}","newText":"a}{b}"})", context);
        expect(fatal(edit.has_value()));
        expect(bool(edit->before == U"}{}")); expect(bool(edit->after == U"a}{b}"));
        expect(!decode_edit_response(UR"({"oldText":"}","newText":"x"})", {}));
        expect(!decode_edit_response(UR"({"oldText":"","newText":"x","offset":99})", {}));
        expect(!decode_edit_response(U"unstructured text", {}));
        expect(!decode_edit_response(UR"({"oldText":"","newText":3})", {}));
        expect(decode_edit_response(U"", {})->empty());
        expect(decode_edit_response(UR"({"oldText":"","newText":""})", {})->empty());
        auto newline = decode_edit_response(UR"({"oldText":"","newText":"\n\n```rust\n"})", {});
        expect(fatal(newline.has_value())); expect(newline->after.ends_with(U'\n'));
        auto unicode = decode_edit_response(UR"({"oldText":"","newText":"\ud83d\ude42e\u0301"})", {});
        expect(fatal(unicode.has_value())); expect(bool(unicode->after == U"\U0001f642e\u0301"));
    };
    "diff keeps reused brackets as source and separates inserted arguments"_test = [] {
        Edit edit{U"}{}", U"a}{b}"};
        auto changes = diff(edit);
        expect(fatal(changes.size() == 2_u));
        expect(changes[0].before_begin == 0_u && changes[0].before_end == 0_u);
        expect(changes[1].before_begin == 2_u && changes[1].before_end == 2_u);
        auto prefix = prefix_edit(edit, 3);
        expect(bool(prefix.before == U"}{")); expect(bool(prefix.after == U"a}{"));
        Edit tail{edit.before.substr(prefix.before.size()), edit.after.substr(prefix.after.size())};
        auto text = edit.before; text.replace(0, prefix.before.size(), prefix.after);
        text.replace(prefix.after.size(), tail.before.size(), tail.after);
        expect(bool(text == edit.after));
        expect(bool(prefix_edit({U"old", U""}, 0).before == U"old"));
    };
    "bounded diff reconstructs exact replacements and all partial acceptance boundaries"_test = [] {
        std::mt19937 random(20261104);
        const std::u32string alphabet = U"a{}(\U0001f642\u0301) \n";
        for (int round = 0; round < 300; ++round) {
            Edit edit;
            for (auto* text : {&edit.before, &edit.after})
                for (unsigned count = random() % 20; count; --count) *text += alphabet[random() % alphabet.size()];
            auto actual = edit.before;
            auto changes = diff(edit);
            for (auto it = changes.rbegin(); it != changes.rend(); ++it)
                actual.replace(it->before_begin, it->before_end - it->before_begin,
                    edit.after.substr(it->after_begin, it->after_end - it->after_begin));
            expect(bool(actual == edit.after));
            for (std::size_t count = 0; count <= edit.after.size(); ++count) {
                auto prefix = prefix_edit(edit, count);
                auto result = edit.before;
                result.replace(0, prefix.before.size(), prefix.after);
                result.replace(prefix.after.size(), edit.before.size() - prefix.before.size(), edit.after.substr(count));
                expect(bool(result == edit.after));
            }
        }
    };
    "stable surrounding context precedes the changing editing region"_test = [] {
        std::u32string source = U"unchanged upper line\nedit here\nunchanged lower line\nmore context";
        auto caret = source.find(U"here");
        auto first = make_prompt(capture(source, caret));
        for (std::size_t count = 0; count < 25; ++count) {
            source.insert(caret, U"x"); ++caret;
            auto prompt = make_prompt(capture(source, caret));
            expect(prompt.instructions == first.instructions);
            expect(prompt.context == first.context);
            expect(prompt.cursor != first.cursor);
        }
        for (auto at = source.find(U"edit"); at < source.find(U"\nunchanged lower"); ++at)
            expect(make_prompt(capture(source, at)).context == first.context);
        source.replace(source.find(U"edit"), 4, U"EDIT");
        expect(make_prompt(capture(source, caret)).context == first.context);
    };
};
}
