#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "support/folia_test.hpp"
import folia.markdown.ast;
import folia.markdown.block_source;
import folia.markdown.block_tree;
import folia.core.command;
import folia.markdown.document;
import folia.core.document_edit;
import folia.core.document_operation_apply;
import folia.markdown.document_text;
import folia.core.document_transaction;
import folia.core.document_history;
import folia.markdown.ids;
import folia.core.editor;
import folia.markdown.inline_cst;
import folia.markdown.inline_document;
import folia.markdown.instrumentation;
import folia.markdown.metadata;
import folia.markdown.parser;
import folia.core.render_builder;
import folia.core.render_model;
import folia.markdown.selection;
import folia.markdown.serializer;
import folia.core.source_editor;
import folia.core.source_patch;
import folia.core.source_projection;
import folia.markdown.text_edit;
import folia.core.theme;
import folia.markdown.utf;
using namespace folia;
using namespace boost::ut;

#include "editing/support/patch_history.hpp"

suite editing_history_source_patch_structure_tests = [] {

    "literal_source_structural_patches_preserve_history_and_unaffected_blocks"_test=[] {
        struct Case {std::string source;std::u32string replacement;};
        for (const auto& test:std::vector<Case>{{"# title\n\nbody",U"> quote"},{"- item\n\nbody",U"## heading"},
            {"> [!NOTE] title\n> body\n\ntail",U"- [x] task"},{"| a | b |\n|---|---|\n|x|y|\n\ntail",U"```cpp\ncode\n```"},
            {"[^n]: text\n\ntail",U"$unfinished"},{"````\ncode\n````\n\ntail",U"~~unfinished"}}) {
            Editor editor(test.source);
            const auto original=patch_document_state(editor.document());
            const auto tail=editor.document().root.children.back().id;
            expect(editor.enter_source_mode());
            const auto end=editor.source_editor()->source().find(U"\n\n");
            expect(fatal(end!=std::u32string::npos));
            editor.set_source_selection({0,end});
            reset_core_operation_counters();
            expect(editor.replace_source({0,end},test.replacement))<<test.source;
            patch_expect_local_counters();
            expect(bool(editor.document().root.children.back().id==tail));
            const auto changed=patch_document_state(editor.document());
            expect(editor.exit_source_mode());
            expect(editor.undo())<<test.source;
            expect(patch_document_state(editor.document())==original)<<test.source;
            expect(editor.redo());expect(patch_document_state(editor.document())==changed);
            patch_expect_valid(editor);
        }
    };

    "source_patches_preserve_and_refresh_reference_definitions_outside_the_window"_test=[] {
        Editor editor("[a][r]\n\nmiddle\n\nother\n\n[r]: /url");
        const auto original=patch_document_state(editor.document());
        expect(editor.enter_source_mode());
        expect(editor.replace_source({1,2},U"b"));
        expect(editor.symbols().links.size()==1_u);
        if (!editor.symbols().links.empty()) expect(editor.symbols().links.front().href=="/url");
        expect(editor.undo());expect(patch_document_state(editor.document())==original);
        const auto target=editor.source_editor()->source().find(U"/url");
        expect(editor.replace_source({target,target+4},U"/next"));
        expect(editor.symbols().links.size()==1_u);
        if (!editor.symbols().links.empty()) expect(editor.symbols().links.front().href=="/next");
        expect(editor.undo());expect(patch_document_state(editor.document())==original);
    };

    "source_patch_character_edits_restore_nested_unicode_and_physical_endings"_test=[] {
        for (const auto& input:std::vector<std::string>{"", "\r\n", "abc\r\ndef\rghi\n",
            "# heading\n\n**bold** _italic_", "- [x] task\n  - nested", "> [!NOTE] title\n> text",
            "[^n]: footnote\n\nref[^n]", "| a | b |\n|---|---|\n|x|y|", "![alt](url)",
            "<div>\ntext\n</div>", "$$\nx+1\n$$"}) {
            Editor editor(input);expect(fatal(editor.enter_source_mode()));
            const auto original=patch_document_state(editor.document());
            const auto source=editor.source_editor()->source();
            for (const auto& insertion:std::vector<std::u32string>{U"*",U"\U0001f642",U"e\u0301",U"\r\n",U"[t](",U" "}) {
                const auto offset=source.size()/2;
                editor.set_source_selection(SourceSelection::caret(offset));
                auto expected=source;expected.insert(offset,insertion);
                reset_core_operation_counters();
                const auto applied=editor.replace_source({offset,offset},insertion);
                expect(applied)<<"input "<<input<<" inserted "<<cps_to_utf8(insertion)<<" offset "<<offset;
                if (!applied) {
                    Editor parsed(cps_to_utf8(expected));
                    expect(false)<<"full parser output "<<std::quoted(parsed.markdown_utf8())
                        <<" expected "<<std::quoted(cps_to_utf8(expected));
                    return;
                }
                patch_expect_local_counters();
                expect(bool(editor.source_editor()->source()==expected));
                const auto changed=patch_document_state(editor.document());
                const auto rendered=patch_render_state(editor);
                expect(fatal(editor.undo()));expect(patch_document_state(editor.document())==original);
                expect(bool(editor.source_editor()->source()==source));
                expect(bool(editor.source_editor()->selection()==SourceSelection::caret(offset)));
                expect(fatal(editor.redo()));expect(patch_document_state(editor.document())==changed);
                expect(patch_render_state(editor)==rendered);patch_expect_valid(editor);
                expect(fatal(editor.undo()));
            }
        }
    };

    "structural_source_batch_keeps_untouched_roots_between_edits"_test=[] {
        Editor editor("first\n\nmiddle\n\nlast\n\ntail");
        const auto original=patch_document_state(editor.document());
        const auto middle=editor.document().root.children[1].id;
        const auto tail=editor.document().root.children.back().id;
        expect(fatal(editor.enter_source_mode()));
        const auto last=editor.source_editor()->source().find(U"last");
        const std::vector<SourceReplacement> replacements{{{0,5},U"# one\n\n# two"},{{last,last+4},U"> other"}};
        reset_core_operation_counters();
        expect(fatal(editor.replace_all_source(replacements)));patch_expect_local_counters();
        expect(bool(editor.document().root.children[2].id==middle));
        expect(bool(editor.document().root.children.back().id==tail));
        const auto changed=patch_document_state(editor.document());
        expect(fatal(editor.undo()));expect(patch_document_state(editor.document())==original);
        expect(fatal(editor.redo()));expect(patch_document_state(editor.document())==changed);
        patch_expect_valid(editor);
    };

    "source_history_keeps_untouched_line_ids_and_exact_projected_selection"_test=[] {
        for (auto text : {"first\nsecond", "**math** $x+1$\n"}) {
            Editor editor(text);
            expect(fatal(editor.enter_source_mode()));
            auto end=editor.source_editor()->source().size();
            editor.set_source_selection(SourceSelection::caret(end));
            auto before=editor.source_editor()->projected_selection();
            auto lines=editor.source_editor()->lines();
            std::vector<NodeId> ids;
            for (auto const& line:lines) ids.push_back(line.id);
            expect(fatal(editor.execute_command(Command::InsertText(U" prefix"))));
            auto selected=editor.source_editor()->projected_selection();
            auto edited=editor.source_editor()->source();
            reset_core_operation_counters();
            for (int i=0;i<10;++i) {
                expect(fatal(editor.undo()));
                expect(bool(editor.source_editor()->projected_selection()==before));
                expect(fatal(editor.redo()));
                expect(bool(editor.source_editor()->projected_selection()==selected));
                expect(bool(editor.source_editor()->source()==edited));
                expect(editor.source_editor()->lines().size()==ids.size());
                for (std::size_t line=0;line<ids.size();++line)
                    expect(bool(editor.source_editor()->lines()[line].id==ids[line]));
            }
            auto counters=read_core_operation_counters();
            expect(counters.full_document_parses==0_u);
            expect(counters.full_document_serializations==0_u);
        }
    };

    "source_frontmatter_metadata_is_part_of_the_reversible_patch"_test=[] {
        Editor editor("---\ntitle: before\n---\n\nbody");
        const auto original=editor.document().metadata;
        expect(fatal(editor.enter_source_mode()));
        const auto title=editor.source_editor()->source().find(U"before");
        expect(fatal(editor.replace_source({title,title+6},U"after")));
        expect(editor.document().metadata.title==std::optional<std::string>{"after"});
        expect(fatal(editor.exit_source_mode()));expect(fatal(editor.undo()));
        expect(bool(editor.document().metadata==original));expect(fatal(editor.redo()));
        expect(editor.document().metadata.title==std::optional<std::string>{"after"});
        expect(fatal(editor.enter_source_mode()));expect(fatal(editor.replace_source({0,3},U"plain")));
        expect(!editor.document().metadata.title);expect(fatal(editor.undo()));
        expect(editor.document().metadata.title==std::optional<std::string>{"after"});
    };

};
