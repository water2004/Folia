#include "support/folia_test.hpp"
import folia.completion;

namespace {
using namespace boost::ut;
using namespace folia::completion;
suite completion_edit_tests = [] {
    "model replies contain literal replacement text and no model-owned source range"_test = [] {
        Context context{{}, "\\frac{", "}{}"};
        auto edit = decode_edit_response(U"<edit>a}{b}</edit>", context);
        expect(fatal(edit.has_value()));
        expect(bool(edit->before == U"}{")); expect(bool(edit->after == U"a}{b"));
        expect(!decode_edit_response(U"unframed text", {}));
        expect(!decode_edit_response(U"<edit>unfinished", {}));
        expect(!decode_edit_response(UR"({"oldText":"","newText":"x"})", {}));
        expect(decode_edit_response(U"", {})->empty());
        expect(decode_edit_response(U"<edit></edit>", {})->empty());
        auto deletion = decode_edit_response(U"<edit></edit>", Context{{}, "", "OLD"});
        expect(deletion && deletion->before == U"OLD" && deletion->after.empty());
        auto newline = decode_edit_response(U"<edit>\n\n```rust\n</edit>", {});
        expect(fatal(newline.has_value())); expect(newline->after.starts_with(U"\n\n"));
        auto tex = decode_edit_response(UR"(<edit>{n}+\frac{1}{\sqrt{2}}</edit>)", {});
        expect(tex && tex->after == UR"({n}+\frac{1}{\sqrt{2}})");
        auto windowsLines = decode_edit_response(U"<edit>a\r\nb\rc</edit>", {});
        expect(windowsLines && windowsLines->after == U"a\nb\nc");
        auto unicode = decode_edit_response(U"<edit>🙂é</edit>", {});
        expect(unicode && unicode->after == U"🙂é");
    };
    "the editable window is local Unicode source and copied right context stays outside the edit"_test = [] {
        Context c{{}, "x_", "} outside the editable region\nnext line"};
        expect(editable_suffix(c).size() <= editable_tail_characters);
        auto edit = decode_edit_response(U"<edit>{n} outside the editable region</edit>",
            Context{{}, "x_", "} outside the editable region\nnext line"});
        expect(edit && edit->before.empty() && edit->after == U"{n");
        Context huge{{}, "", std::string(1024, 'x')};
        expect(editable_suffix(huge).size() == editable_tail_characters);
    };
    "prompt presents exact chronological document text without forcing a language"_test = [] {
        Context c{"earlier\n\n", "line one\nabc_", "} right\nnext", 9, 7, "\n\nafter"};
        auto p = make_prompt(c);
        expect(p.context + p.cursor == "Document:\n" + c.preceding + c.prefix + p.edit_start + "} right" + p.edit_end + "\nnext" + c.following);
        expect(p.instructions.find("oldText") == p.instructions.npos);
        expect(p.instructions.find("JSON") == p.instructions.npos);
        expect(p.instructions.find("Markdown") == p.instructions.npos && p.instructions.find("TeX") == p.instructions.npos);
        auto collision = c; collision.prefix += "<edit></edit>";
        auto framed = make_prompt(collision);
        expect(framed.edit_start != "<edit>" && framed.edit_end != "</edit>");
    };
    "literal replies preserve arbitrary Unicode right context without advancing over copied text"_test = [] {
        std::mt19937 random(20261105);
        std::u32string alphabet = U"ab{}\\_ 🙂é\n";
        for (int round = 0; round < 300; ++round) {
            std::u32string suffix, inserted;
            for (unsigned count = random() % 90; count; --count) suffix += alphabet[random() % alphabet.size()];
            for (unsigned count = random() % 24; count; --count) inserted += alphabet[random() % alphabet.size()];
            std::size_t end = 0;
            while (end < suffix.size() && end < editable_tail_characters && suffix[end] != U'\n') ++end;
            auto context = capture(suffix, 0);
            auto edit = decode_edit_response(U"<edit>" + inserted + suffix.substr(0, end) + U"</edit>", context);
            expect(fatal(edit.has_value()));
            expect(edit->before.empty()); expect(bool(edit->after == inserted));
        }
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
    "fixed following blocks share the existing context budget and remain ahead of local edits"_test = [] {
        std::u32string huge(10 * 1024 * 1024, U'\U0001f642');
        std::array<std::u32string_view, 2> upper{huge, U"stable upper"}, lower{U"stable lower", huge};
        auto budget = make_budget(7000, 128);
        auto first = capture(U"abc", 1, upper, budget, lower);
        auto next = capture(U"aXYZbc", 4, upper, budget, lower);
        expect(!first.following.empty());
        expect(first.following.starts_with("\n\nstable lower"));
        expect(first.preceding.size() <= budget.preceding_bytes && first.following.size() <= budget.following_bytes);
        expect(first.prefix.size() + first.suffix.size() + first.preceding.size() + first.following.size() <= 7000_u);
        expect(make_prompt(first).context == make_prompt(next).context);
        auto changedLower = lower; changedLower[0] = U"updated lower";
        expect(make_prompt(capture(U"abc", 1, upper, budget, changedLower)).cursor != make_prompt(first).cursor);
    };
};
}
