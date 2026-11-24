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

suite editing_history_source_patch_raw_blocks_tests = [] {

    "source_edits_on_the_last_blank_line_before_an_orphan_math_body"_test=[] {
        const std::u32string source = U"earlier\n\nintro\n\\[\na\n\\]\n\n\n\nb\n\\]\n\nend";
        const auto offset=source.find(U"\nb\n");
        for (auto insertion : {U"x", U"\\["}) {
            Editor editor(cps_to_utf8(source));
            expect(fatal(editor.enter_source_mode()));
            const auto before=SourceSelection::caret(offset);
            editor.set_source_selection(before);
            reset_core_operation_counters();
            expect(editor.replace_source({offset,offset},insertion)) << cps_to_utf8(insertion);
            patch_expect_local_counters();
            auto expected=source;expected.insert(offset,insertion);
            expect(bool(editor.source_editor()->source()==expected));
            expect(editor.undo());
            expect(bool(editor.source_editor()->source()==source));
            expect(editor.source_editor()->selection()==before);
            expect(editor.redo());
            expect(bool(editor.source_editor()->source()==expected));
        }
    };

    "closing_display_math_mid_block_makes_its_tail_editable"_test=[] {
        const std::string source = "earlier\n\n\\[\na\n\n\n\n\nb\n\\]\n\nend";
        for (bool paste : {false, true}) {
            Editor editor(source);
            const auto owner=editor.document().root.children[1].id;
            const auto initial=TextSelection::caret({owner,5});
            editor.set_selection(initial);
            auto before=patch_document_state(editor.document());
            reset_core_operation_counters();
            if (paste) expect(fatal(editor.execute_document_paste_text(initial,U"\\]").has_value()));
            else {
                expect(fatal(editor.execute_document_insert_text(initial,U"\\").has_value()));
                expect(fatal(editor.execute_document_insert_text(editor.selection(),U"]").has_value()));
            }
            patch_expect_local_counters();
            const auto cut = editor.markdown_utf8();
            auto expected=source;expected.insert(source.find("\na\n")+3,"\\]");
            expect(cut==expected);
            const auto& blocks=editor.document().root.children;
            const auto tail=std::ranges::find_if(blocks,[](const auto& block){
                return block.kind==BlockKind::Paragraph && block.inline_content.source==U"b\n\\]";
            });
            expect(fatal(tail!=blocks.end() && tail!=blocks.begin()));
            expect(bool(std::prev(tail)->kind==BlockKind::Paragraph && std::prev(tail)->inline_content.source.empty()));
            const auto blank=TextSelection::caret({std::prev(tail)->id,0});
            editor.set_selection(blank);
            expect(fatal(editor.execute_document_insert_text(blank,U"x").has_value()));
            const auto offset=expected.find("\nb\n");
            auto typed=expected;typed.insert(offset,"x");
            expect(editor.markdown_utf8()==typed);
            expect(editor.undo());
            expect(editor.selection()==blank);
            auto repaired=expected;repaired.insert(offset,"\\[");
            reset_core_operation_counters();
            if (paste) expect(fatal(editor.execute_document_paste_text(blank,U"\\[").has_value()));
            else {
                expect(fatal(editor.execute_document_insert_text(blank,U"\\").has_value()));
                expect(fatal(editor.execute_document_insert_text(editor.selection(),U"[").has_value()));
            }
            patch_expect_local_counters();
            const auto closed=[](auto const& block) {
                return block.kind==BlockKind::MathBlock && block.block_source.tree().complete_closing;
            };
            expect(std::ranges::count_if(editor.document().root.children,closed)==2);
            expect(editor.markdown_utf8()==repaired);
            expect(editor.undo());
            if (!paste) expect(editor.undo());
            expect(editor.selection()==blank);
            expect(editor.undo());
            if (!paste) expect(editor.undo());
            expect(patch_document_state(editor.document())==before);
        }
    };

    "splitting_and_reopening_quoted_math_preserves_original_quote_prefixes"_test=[] {
        const std::string source = ">\\[\n> a\n>\n>\n>\n>\n>b\n> \\]";
        Editor editor(source);
        const auto owner=editor.document().root.children.front().children.front().id;
        const auto initial=TextSelection::caret({owner,5});
        editor.set_selection(initial);
        expect(fatal(editor.execute_document_insert_text(initial,U"\\]").has_value()));
        const std::string cut = ">\\[\n> a\n>\\]\n>\n>\n>\n>b\n> \\]";
        expect(editor.markdown_utf8()==cut) << editor.markdown_utf8();
        const auto& children=editor.document().root.children.front().children;
        const auto body=std::ranges::find_if(children,[](const auto& block){
            return block.kind==BlockKind::Paragraph && block.inline_content.source==U"b\n\\]";
        });
        expect(fatal(body!=children.end() && body!=children.begin()));
        const auto blank=TextSelection::caret({std::prev(body)->id,0});
        editor.set_selection(blank);
        expect(fatal(editor.execute_document_paste_text(blank,U"\\[").has_value()));
        expect(editor.markdown_utf8()==">\\[\n> a\n>\\]\n>\n>\n>\\[\n>b\n> \\]") << editor.markdown_utf8();
        expect(editor.undo()); expect(editor.markdown_utf8()==cut);
        expect(editor.undo()); expect(editor.markdown_utf8()==source);
    };

    "source_fence_context_expands_only_when_following_blocks_change"_test=[] {
        Editor editor("first\n\nsecond\n\nthird\n\ntail");
        const auto original=patch_document_state(editor.document());
        expect(editor.enter_source_mode());
        reset_core_operation_counters();
        expect(editor.replace_source({0,0},U"```\n"));patch_expect_local_counters();
        expect(editor.document().root.children.size()==1_u);
        expect(editor.document().root.children.front().kind==BlockKind::CodeBlock);
        expect(editor.undo());expect(patch_document_state(editor.document())==original);
        expect(bool(editor.source_editor()->source()==U"first\n\nsecond\n\nthird\n\ntail"));
        expect(editor.redo());patch_expect_valid(editor);
    };

};
