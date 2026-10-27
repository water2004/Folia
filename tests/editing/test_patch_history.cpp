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

namespace {

std::string patch_document_state(const EditorDocument& document) {
    std::ostringstream out;
    auto text=[&](std::u32string_view value){out<<std::quoted(cps_to_utf8(value))<<';';};
    text(document.trailing_line_ending);
    walk_blocks(document.root,[&](const BlockNode& block) {
        out<<block.id.v<<':'<<int(block.kind)<<':'<<block.children.size()<<':';
        out<<block.separator_before.has_value();if (block.separator_before) text(*block.separator_before);
        text(block.inline_content.source);out<<int(block.inline_content.syntax_mode);
        for (const auto& token:block.inline_content.tree.tokens) out<<':'<<token.packed_end_and_kind;
        std::vector<const InlineCstNode*> pending;
        for (auto i=block.inline_content.tree.nodes.size();i>0;--i) pending.push_back(&block.inline_content.tree.nodes[i-1]);
        while (!pending.empty()) {
            const auto* node=pending.back();pending.pop_back();
            out<<'['<<node->id.v<<':'<<int(node->kind)<<':'<<int(node->status)<<':'<<node->range.start<<':'<<node->range.end;
            const auto& sem=node->semantics();
            out<<std::quoted(sem.href)<<std::quoted(sem.title.value_or(""))<<std::quoted(sem.label)
                <<std::quoted(sem.target)<<std::quoted(sem.alt)<<std::quoted(sem.html_tag);
            if (node->delimited) {
                const auto& d=*node->delimited;
                out<<':'<<d.full.start<<':'<<d.full.end<<':'<<d.opening.start<<':'<<d.opening.end
                    <<':'<<d.content.start<<':'<<d.content.end<<':'<<d.closing.has_value();
                if (d.closing) out<<':'<<d.closing->start<<':'<<d.closing->end;
            }
            out<<']';
            for (auto i=node->children.size();i>0;--i) pending.push_back(&node->children[i-1]);
        }
        const auto& heading=block.text_special();
        out<<int(heading.level)<<std::quoted(heading.slug);text(heading.opening_marker);text(heading.closing_marker);
        const auto& list=block.list_special();out<<list.ordered<<list.start<<int(list.delimiter)<<list.tight;
        out<<block.item_special().checked;text(block.item_special().marker);
        out<<std::quoted(block.container_special().callout_kind)<<std::quoted(block.container_special().footnote_label);
        const auto& atomic=block.atomic_special();
        out<<atomic.code_indented<<int(atomic.math_delim)<<int(atomic.toc_marker)<<int(atomic.fmt)
            <<std::quoted(atomic.raw)<<int(atomic.unsup_reason)<<std::quoted(atomic.ext_name);
        if (atomic.reference_definition) out<<std::quoted(atomic.reference_definition->label)
            <<std::quoted(atomic.reference_definition->target.href)
            <<std::quoted(atomic.reference_definition->target.title.value_or(""));
        const auto& image=block.image_special();
        out<<std::quoted(image.src)<<std::quoted(image.image_alt)<<std::quoted(image.image_title.value_or(""))
            <<std::quoted(image.image_link.value_or(""));
        if (block.block_source.storage) {
            text(block.block_source.source());text(block_source_content(block.block_source));
            text(block_source_code_projection(block.block_source));
            for (const auto& context:block.block_source.storage->indentation_contexts)
                out<<context.source_offset<<':'<<context.physical_column<<':'<<context.residual_columns;
        }
        const auto& table=block.table_special();
        text(table.table_separator_source);for (auto alignment:table.table_aligns) out<<int(alignment);
        for (const auto& ending:table.table_internal_line_endings) text(ending);
        if (table.row_syntax) {
            for (auto owner:table.row_syntax->cell_owners) out<<owner.v<<',';
            for (auto owner:table.row_syntax->implicit_cell_owners) out<<owner.v<<',';
            for (const auto& gap:table.row_syntax->gaps) text(gap);
        }
        for (const auto& prefix:block.container_source().lines) {
            out<<prefix.anchor.owner_id.v<<':'<<int(prefix.anchor.part)<<':'<<prefix.anchor.offset
                <<':'<<prefix.lazy<<':'<<int(prefix.role);text(prefix.text);
        }
        if (block.has_html_source()) {
            text(block.html_special().source);
            for (const auto& slot:block.html_special().content_slots)
                out<<slot.owner_id.v<<':'<<slot.source_range.start<<':'<<slot.source_range.end;
        }
        out<<'\n';
    });
    return out.str();
}

std::string patch_render_state(const Editor& editor) {
    const auto model=build_render_model(editor.document(),editor.outline(),editor.symbols(),default_theme_profile());
    std::ostringstream out;
    auto text=[&](std::u32string_view value){out<<std::quoted(cps_to_utf8(value));};
    auto items=[&](const auto& self,const std::vector<InlineRenderItem>& nodes)->void {
        for (const auto& item:nodes) {
            out<<int(item.kind)<<':';text(item.text);text(item.special().display_text);
            out<<std::quoted(item.special().semantic().href)<<std::quoted(item.special().semantic().src);
            self(self,item.special().semantic().children);
        }
    };
    std::vector<const RenderBlock*> pending;
    for (auto i=model.blocks.size();i>0;--i) pending.push_back(&model.blocks[i-1]);
    while (!pending.empty()) {
        const auto* block=pending.back();pending.pop_back();
        out<<int(block->kind)<<':'<<block->child_blocks.size();items(items,block->inline_items);
        const auto& special=block->special();text(special.code_text);text(special.tex);
        out<<std::quoted(special.src)<<std::quoted(special.alt)<<std::quoted(special.raw);
        for (const auto& cell:special.table_cells) items(items,cell);
        for (auto i=block->child_blocks.size();i>0;--i) pending.push_back(&block->child_blocks[i-1]);
    }
    return out.str();
}

void patch_expect_valid(const Editor& editor) {
    expect(bool(validate_document(editor.document()).empty()));
    walk_blocks(editor.document().root,[&](const BlockNode& block) {
        if (const auto* source=editable_inline_document(block)) {
            expect(bool(flatten_tokens(source->tree,source->source)==source->source));
            expect(bool(serialize_lossless(source->tree,source->source)==source->source));
        }
    });
}

void patch_expect_local_counters() {
    const auto counters=read_core_operation_counters();
    expect(counters.full_document_parses==0_u);
    expect(counters.full_document_serializations==0_u);
    expect(counters.full_tree_transaction_diffs==0_u);
}

Command patch_insert(std::u32string text) {
    Command command;command.kind=CommandKind::InsertText;command.text=std::move(text);return command;
}

} // namespace

suite patch_history_tests=[] {
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

    "switching_views_keeps_redo_but_a_new_source_patch_discards_it"_test=[] {
        Editor editor("abc");
        expect(editor.execute_command(patch_insert(U"x")));expect(editor.undo());
        expect(editor.enter_source_mode());expect(editor.has_redo());
        expect(editor.exit_source_mode());expect(editor.has_redo());
        expect(editor.enter_source_mode());expect(editor.replace_source({0,0},U"y"));
        expect(!editor.has_redo());expect(editor.undo());
        expect(bool(editor.source_editor()->source()==U"abc"));
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
