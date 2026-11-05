#include "support/completion_prompt.hpp"
#include "support/folia_test.hpp"
import folia.completion;

namespace {
using namespace boost::ut;
using namespace folia::completion;
suite completion_edit_tests = [] {
    "runtime prompt settings expand only instruction parameters and frame configured exchanges"_test = [] {
        auto settings = completion_tests::PromptSettings();
        settings.instructions = "Return ${edit_start} through ${edit_end}. ${policy} Limit ${max_characters}.";
        settings.policies[1] = "literal ${edit_end}";
        settings.region_name = "edit";
        settings.before_edit.clear(); settings.after_edit.clear();
        settings.examples = {{"he", "lo", " world", "<insert>l</insert>"}, {"call(", ")", ";", "<insert>value</insert>"}};
        Context context{{}, "<edit>$x_", "}$"};
        auto prompt = make_prompt(context, settings);
        expect(prompt.edit_start != "<edit>");
        expect(prompt.instructions.contains(prompt.edit_start) && prompt.instructions.contains(prompt.edit_end));
        expect(prompt.instructions.contains("literal ${edit_end}"));
        expect(prompt.cursor.starts_with(context.prefix));
        expect(prompt.examples.size() == 2_u);
        expect(prompt.examples[1].first == settings.document_prefix + "call(" + prompt.edit_start + ")" + prompt.edit_end + ";");
        expect(prompt.examples[1].second == "<insert>value</insert>");
        settings.instructions += " ${unknown}";
        expect(validate_prompt_settings(settings).has_value());
        expect(throws([&] { (void)make_prompt(context, settings); }));
        expect(validate_prompt_settings({}).has_value());
    };
    "model replies contain literal replacement text and no model-owned source range"_test = [] {
        Context context{{}, "\\frac{", "}{}"};
        auto edit = decode_edit_response(U"<replace>a}{b}</replace>", context);
        expect(fatal(edit.has_value()));
        expect(bool(edit->before == U"}{")); expect(bool(edit->after == U"a}{b"));
        expect(!decode_edit_response(U"unframed text", {}));
        expect(!decode_edit_response(U"<replace>unfinished", {}));
        expect(!decode_edit_response(UR"({"oldText":"","newText":"x"})", {}));
        expect(decode_edit_response(U"", {})->empty());
        expect(decode_edit_response(U"<insert></insert>", {})->empty());
        expect(!decode_edit_response(U"<edit>old reply</edit>", {}));
        auto deletion = decode_edit_response(U"<replace></replace>", Context{{}, "", "OLD"});
        expect(deletion && deletion->before == U"OLD" && deletion->after.empty());
        auto newline = decode_edit_response(U"<replace>\n\n```rust\n</replace>", {});
        expect(fatal(newline.has_value())); expect(newline->after.starts_with(U"\n\n"));
        auto tex = decode_edit_response(UR"(<replace>{n}+\frac{1}{\sqrt{2}}</replace>)", {});
        expect(tex && tex->after == UR"({n}+\frac{1}{\sqrt{2}})");
        auto windowsLines = decode_edit_response(U"<replace>a\r\nb\rc</replace>", {});
        expect(windowsLines && windowsLines->after == U"a\nb\nc");
        auto unicode = decode_edit_response(U"<replace>🙂é</replace>", {});
        expect(unicode && unicode->after == U"🙂é");
    };
    "insert operations never consume the marked right source while replace operations are explicit"_test = [] {
        Context context{{}, "$a_", "}$"};
        auto insert = decode_edit_response(U"<insert>{n+1</insert>", context);
        expect(fatal(insert.has_value()));
        expect(insert->before.empty()); expect(bool(insert->after == U"{n+1"));
        auto replace = decode_edit_response(U"<replace>{n+1}$</replace>", context);
        expect(bool(replace && replace->before.empty() && replace->after == insert->after));
        auto emptyInsert = decode_edit_response(U"<insert></insert>", context);
        expect(emptyInsert && emptyInsert->empty());
        auto emptyReplace = decode_edit_response(U"<replace></replace>", context);
        expect(emptyReplace && emptyReplace->before == U"}$" && emptyReplace->after.empty());
        auto literal = decode_edit_response(U"<insert><replace>literal</replace></insert>", context);
        expect(literal && literal->after == U"<replace>literal</replace>");
        expect(!decode_edit_response(U"<insert>x</replace>", context));
        expect(!decode_edit_response(U"<edit>insert\nx</edit>", context));
    };
    "the editable window is local Unicode source and copied right context stays outside the edit"_test = [] {
        Context c{{}, "x_", "} outside the editable region\nnext line"};
        expect(editable_suffix(c).size() <= editable_tail_characters);
        auto edit = decode_edit_response(U"<replace>{n} outside the editable region</replace>",
            Context{{}, "x_", "} outside the editable region\nnext line"});
        expect(edit && edit->before.empty() && edit->after == U"{n");
        Context huge{{}, "", std::string(1024, 'x')};
        expect(editable_suffix(huge).size() == editable_tail_characters);
    };
    "prompt presents exact chronological document text without forcing a language"_test = [] {
        Context c{"earlier\n\n", "line one\nabc_", "} right\nnext", 9, 7, "\n\nafter"};
        auto p = completion_tests::MakePrompt(c);
        auto const& settings = completion_tests::PromptSettings();
        expect(p.context + p.cursor == settings.document_prefix + c.preceding + c.prefix + settings.before_edit
            + p.edit_start + "} right" + p.edit_end + settings.after_edit + "\nnext" + c.following);
        expect(p.instructions.find("oldText") == p.instructions.npos);
        expect(p.instructions.find("JSON") == p.instructions.npos);
        expect(p.instructions.find("Markdown") == p.instructions.npos && p.instructions.find("TeX") == p.instructions.npos);
        auto collision = c; collision.prefix += p.edit_start + p.edit_end;
        auto framed = completion_tests::MakePrompt(collision);
        expect(framed.edit_start != p.edit_start && framed.edit_end != p.edit_end);
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
            auto edit = decode_edit_response(U"<replace>" + inserted + suffix.substr(0, end) + U"</replace>", context);
            expect(fatal(edit.has_value()));
            expect(edit->before.empty()); expect(bool(edit->after == inserted));
            auto insertion = decode_edit_response(U"<insert>" + inserted + U"</insert>", context);
            expect(fatal(insertion.has_value()));
            expect(insertion->before.empty()); expect(bool(insertion->after == inserted));
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
        auto first = completion_tests::MakePrompt(capture(source, caret));
        for (std::size_t count = 0; count < 25; ++count) {
            source.insert(caret, U"x"); ++caret;
            auto prompt = completion_tests::MakePrompt(capture(source, caret));
            expect(prompt.instructions == first.instructions);
            expect(prompt.context == first.context);
            expect(prompt.cursor != first.cursor);
        }
        for (auto at = source.find(U"edit"); at < source.find(U"\nunchanged lower"); ++at)
            expect(completion_tests::MakePrompt(capture(source, at)).context == first.context);
        source.replace(source.find(U"edit"), 4, U"EDIT");
        expect(completion_tests::MakePrompt(capture(source, caret)).context == first.context);
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
        expect(completion_tests::MakePrompt(first).context == completion_tests::MakePrompt(next).context);
        auto changedLower = lower; changedLower[0] = U"updated lower";
        expect(completion_tests::MakePrompt(capture(U"abc", 1, upper, budget, changedLower)).cursor != completion_tests::MakePrompt(first).cursor);
    };
};
}
