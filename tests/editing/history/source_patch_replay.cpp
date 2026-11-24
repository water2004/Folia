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

suite editing_history_source_patch_replay_tests = [] {

    "source_patch_batch_validates_original_text_and_reverses_exactly"_test=[] {
        std::u32string text=U"a\r\nb\U0001f642c";
        const auto original=text;
        SourcePatch patch{{{{4,5},U"**x**",U"\U0001f642"},{{0,1},U"Z",U"a"}},
            SourceSelection::caret(1),SourceSelection::caret(6)};
        expect(apply_source_patch(text,patch));
        expect(bool(text==U"Z\r\nb**x**c"));
        expect(apply_source_patch(text,patch,false));
        expect(bool(text==original));
        patch.edits.front().removed=U"q";
        expect(!apply_source_patch(text,patch));expect(bool(text==original));
    };

    "history_patch_restores_inline_identity_and_marker_spelling"_test=[] {
        for (const auto& input:std::vector<std::string>{"abc","*abc*","_abc_","**abc**","__abc__","**",
            "**abc","a***b***c","~~abc~~","~~abc","`abc`","`abc","[t](url)","[t](<url>)",
            "[t](url 'title')","[t](","before ![alt](url)","$abc$","$abc","\\*abc\\*","&amp;","a\\**b*"}) {
            Editor editor(input);
            const auto initial=patch_document_state(editor.document());
            const auto selected=editor.selection();
            expect(editor.execute_command(patch_insert(U"x")))<<input;
            const auto changed=patch_document_state(editor.document());
            reset_core_operation_counters();
            expect(editor.undo())<<input;patch_expect_local_counters();
            expect(patch_document_state(editor.document())==initial)<<input;
            expect(bool(editor.selection()==selected));
            expect(editor.redo())<<input;
            expect(patch_document_state(editor.document())==changed)<<input;
            patch_expect_valid(editor);
        }
    };

    "one_history_replays_rendered_and_source_patches_across_mode_switches"_test=[] {
        Editor editor("abc\n\ndef");
        auto selected=editor.selection();selected.anchor.source_offset=selected.active.source_offset=1;editor.set_selection(selected);
        const auto initial=patch_document_state(editor.document());
        expect(editor.execute_command(patch_insert(U"X")));
        const auto rendered=patch_document_state(editor.document());
        expect(editor.enter_source_mode());
        reset_core_operation_counters();
        expect(editor.undo());patch_expect_local_counters();
        expect(bool(editor.source_editor()->source()==U"abc\n\ndef"));
        expect(bool(editor.source_editor()->selection()==SourceSelection::caret(1)));
        expect(patch_document_state(editor.document())==initial);
        expect(editor.redo());expect(patch_document_state(editor.document())==rendered);
        editor.set_source_selection(SourceSelection::caret(2));
        expect(editor.execute_command(patch_insert(U"Y")));
        const auto source=patch_document_state(editor.document());
        reset_core_operation_counters();
        expect(editor.exit_source_mode());patch_expect_local_counters();
        expect(editor.undo());expect(patch_document_state(editor.document())==rendered);
        expect(editor.undo());expect(patch_document_state(editor.document())==initial);
        expect(editor.redo());expect(editor.redo());expect(patch_document_state(editor.document())==source);
        patch_expect_valid(editor);
    };

    "switching_views_keeps_redo_but_a_new_source_patch_discards_it"_test=[] {
        Editor editor("abc");
        expect(editor.execute_command(patch_insert(U"x")));expect(editor.undo());
        expect(editor.enter_source_mode());expect(editor.has_redo());
        expect(editor.exit_source_mode());expect(editor.has_redo());
        expect(editor.enter_source_mode());expect(editor.replace_source({0,0},U"y"));
        expect(!editor.has_redo());expect(editor.undo());
        expect(bool(editor.source_editor()->source()==U"abc"));
    };

    "random_source_patch_history_restores_every_exact_state"_test=[] {
        std::mt19937 random(0x50415443);
        const std::u32string alphabet=U"ab*_`$\\&#=>-[]() \r\n\U0001f642\u0301";
        for (const auto& input:std::vector<std::string>{"abc\n\ntail","> quote\n\n- item", "[a][r]\n\n[r]: /url"}) {
            Editor editor(input);expect(fatal(editor.enter_source_mode()));
            std::vector<std::string> states{patch_document_state(editor.document())};
            for (std::size_t step=0;step<100;++step) {
                const auto before=editor.source_editor()->source();
                const auto start=std::size_t(random()%(before.size()+1));
                const auto length=std::size_t(random()%((std::min)(std::size_t{4},before.size()-start)+1));
                const auto insertion=std::u32string(1,alphabet[random()%alphabet.size()]);
                if (std::u32string_view(before).substr(start,length)==insertion) continue;
                const SourceSelection selected{start+length,start};editor.set_source_selection(selected);
                auto expected=before;expected.replace(start,length,insertion);
                reset_core_operation_counters();
                const auto applied=editor.replace_source({start,start+length},insertion);
                expect(applied)
                    <<"step "<<step<<" input "<<cps_to_utf8(before)<<" range "<<start<<":"<<length<<" inserted "<<cps_to_utf8(insertion);
                if (!applied) {
                    Editor parsed(cps_to_utf8(expected));
                    std::ostringstream codepoints;codepoints<<std::hex;
                    for (auto value:before) codepoints<<static_cast<std::uint32_t>(value)<<',';
                    expect(false)<<"before codepoints "<<codepoints.str()<<" inserted codepoint "
                        <<static_cast<std::uint32_t>(insertion.front())<<" full parser output "
                        <<std::quoted(parsed.markdown_utf8())<<" expected "<<std::quoted(cps_to_utf8(expected));
                    return;
                }
                patch_expect_local_counters();
                expect(fatal(bool(editor.source_editor()->source()==expected)));
                const auto changed=patch_document_state(editor.document());
                const auto rendered=patch_render_state(editor);
                expect(fatal(editor.undo()));expect(fatal(patch_document_state(editor.document())==states.back()))<<step;
                expect(bool(editor.source_editor()->selection()==selected));
                expect(fatal(editor.redo()));expect(fatal(patch_document_state(editor.document())==changed))<<step;
                expect(patch_render_state(editor)==rendered);patch_expect_valid(editor);
                states.push_back(changed);
            }
            for (auto i=states.size();i>1;--i) {
                expect(fatal(editor.undo()));expect(fatal(patch_document_state(editor.document())==states[i-2]));
            }
            expect(!editor.has_undo());
            for (std::size_t i=1;i<states.size();++i) {
                expect(fatal(editor.redo()));expect(fatal(patch_document_state(editor.document())==states[i]));
            }
        }
    };

    "patch_replay_failure_is_atomic_and_history_capacity_is_bounded"_test=[] {
        Editor editor("**abc**");
        const auto original=patch_document_state(editor.document());
        auto transaction=editor.execute_document_insert_text(editor.selection(),U"x");
        expect(fatal(transaction.has_value()));
        expect(fatal(editor.undo()));
        auto operations=transaction->operations;
        DocumentTextOperation invalid;
        invalid.forward={NodeId{999999},{0,0},U"bad"};invalid.inverse={NodeId{999999},{0,3},U""};
        operations.emplace_back(invalid);
        const auto allocation=editor.document().next_node_id;
        auto document=editor.document();
        expect(!apply_document_operations(document,operations,true));
        expect(patch_document_state(document)==original);expect(document.next_node_id==allocation);

        DocumentHistory history(2);TextSelection selection=editor.selection();
        for (std::size_t i=0;i<3;++i) {
            auto edit=document_insert_text(document,selection,U"z");expect(fatal(edit.has_value()));
            selection=edit->selection_after;history.push(*edit);
        }
        expect(history.undo(document,selection));expect(history.undo(document,selection));
        expect(!history.undo(document,selection));expect(history.redo(document,selection));
        auto edit=document_insert_text(document,selection,U"q");expect(fatal(edit.has_value()));history.push(*edit);
        expect(!history.has_redo());
    };

    "rendered_patch_commands_restore_nested_source_selection_and_visuals"_test=[] {
        for (const auto& input:std::vector<std::string>{"abc", "# abc", "- abc", "- [x] abc", "> abc",
            "> [!NOTE] abc\n> body", "[^n]: abc", "| abc | def |\n|---|---|\n|x|y|"}) {
            for (auto kind:{CommandKind::InsertText,CommandKind::DeleteSelection,CommandKind::ToggleStrong,CommandKind::Paste}) {
                Editor editor(input);
                auto selected=editor.selection();selected.anchor.source_offset=1;selected.active.source_offset=2;
                editor.set_selection(selected);
                const auto original=patch_document_state(editor.document());
                const auto visual=patch_render_state(editor);
                Command command;command.kind=kind;command.text=U"\U0001f642e\u0301";
                expect(fatal(editor.execute_command(command)));
                const auto changed=patch_document_state(editor.document());
                const auto changed_visual=patch_render_state(editor);
                const auto changed_selection=editor.selection();
                expect(fatal(editor.enter_source_mode()));
                reset_core_operation_counters();
                expect(fatal(editor.undo()));patch_expect_local_counters();
                expect(patch_document_state(editor.document())==original)<<input<<int(kind);
                expect(patch_render_state(editor)==visual)<<input<<int(kind);
                expect(fatal(editor.exit_source_mode()));expect(bool(editor.selection()==selected));
                expect(fatal(editor.redo()));expect(patch_document_state(editor.document())==changed);
                expect(patch_render_state(editor)==changed_visual);expect(bool(editor.selection()==changed_selection));
                patch_expect_valid(editor);
            }
        }
    };

};
