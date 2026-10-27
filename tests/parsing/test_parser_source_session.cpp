#include <cstddef>
#include <string>
#include <vector>
#include "support/folia_test.hpp"
import folia.markdown.parser_source_session;
import folia.markdown.ids;
import folia.markdown.container_source;
using namespace folia::parser_source_detail;
using namespace boost::ut;

suite parser_source_session = [] {
    "parser_root_line_table_preserves_cr_lf_crlf_and_unicode_offsets"_test = [] {
        const std::u32string source=U"a\r\nb\rc\n\U0001f600\u0301";
        const RootPhysicalSource root(source);
        expect(root.lines().size()==4u);
        expect(root.line(0).begin==0u && root.line(0).content_end==1u && root.line(0).ending_end==3u);
        expect(root.line(1).begin==3u && root.line(1).content_end==4u && root.line(1).ending_end==5u);
        expect(root.line(2).begin==5u && root.line(2).content_end==6u && root.line(2).ending_end==7u);
        expect(root.line(3).begin==7u && root.line(3).content_end==9u);
        expect(root.line_at(2)==0u && root.line_at(7)==3u && root.line_at(source.size())==3u);
        auto cursor=root.begin_line(3);
        expect(root.peek(cursor)==U'\U0001f600');
        root.advance(cursor);
        expect(cursor.offset==8u && root.peek(cursor)==U'\u0301');
    };
    "parser_leaf_view_materializes_only_its_original_source_spans"_test = [] {
        const std::u32string source=U"> a\r\n> b";
        const RootPhysicalSource root(source);
        LeafSourceView leaf(root);
        leaf.append({2,3}); leaf.append({3,5}); leaf.append({7,8});
        expect(leaf.materialize()==U"a\r\nb");
        expect(leaf.pieces().size()==2u);
        expect(leaf.physical_offset(0)==2u);
        expect(leaf.physical_offset(1)==3u);
        expect(leaf.physical_offset(2)==4u);
        expect(leaf.physical_offset(3)==7u);
        expect(leaf.physical_offset(4)==8u);
        expect(!leaf.physical_offset(5));
        expect(root.source().data()==source.data());
        LeafSourceView empty(root); empty.append({5,5});
        expect(empty.materialize().empty() && empty.physical_offset(0)==5u);
    };
    "parser_physical_cursors_reuse_one_line_at_large_depth"_test = [] {
        std::u32string source;
        for (int index=0;index<50000;++index) source+=U"> ";
        source+=U'a';
        const RootPhysicalSource root(source);
        auto cursor=root.begin_line(0);
        bool markers=true;
        for (int index=0;index<50000;++index) {
            markers=markers && root.peek(cursor)==U'>';
            root.advance(cursor); root.advance(cursor);
        }
        expect(markers && root.peek(cursor)==U'a');
        expect(cursor.offset==100000u && cursor.physical_column==100000u);
        expect(root.lines().size()==1u);
    };
    "parser_physical_tab_residual_context_invents_no_source_characters"_test = [] {
        const std::u32string source=U">\ta";
        const RootPhysicalSource root(source);
        auto cursor=root.begin_line(0);
        root.advance(cursor);
        const auto before=cursor.physical_column;
        root.advance(cursor);
        cursor.residual_columns=cursor.physical_column-before-1;
        expect(cursor.offset==2u && cursor.physical_column==4u && cursor.residual_columns==2u);
        const auto indent=root.indentation(cursor,3);
        expect(indent.columns==2u && indent.end==2u && indent.residual_columns==0u);
        root.consume_indentation(cursor,indent);
        expect(root.peek(cursor)==U'a' && cursor.offset==2u);
        LeafSourceView leaf(root); leaf.append({cursor.offset,3});
        expect(leaf.materialize()==U"a");
    };
    "parser_source_ownership_binding_uses_nearest_owner_and_original_ties"_test = [] {
        SourceOwnershipIndex index;
        index.add_source(folia::NodeId{20}, {{2,5},4});
        index.add_source(folia::NodeId{21}, {{3,6},0});
        index.add_source(folia::NodeId{22}, {{1,1},0});
        index.seal();
        auto first=index.resolve(0,6);
        expect(first && first->owner_id==folia::NodeId{22} && first->offset==0u);
        auto overlapping=index.resolve(3,6);
        expect(overlapping && overlapping->owner_id==folia::NodeId{20} && overlapping->offset==5u);
        auto later=index.resolve(5,6);
        expect(later && later->owner_id==folia::NodeId{21} && later->offset==2u);
    };
    "parser_prefix_bind_keeps_own_spelling_without_descendant_body_copy"_test = [] {
        const std::u32string source=U"> > a\n";
        const RootPhysicalSource root(source);
        SourceOwnershipIndex index;
        index.add_source(folia::NodeId{10}, {{4,5},0});
        index.seal();
        const std::vector<PendingContainerPrefix> prefixes{
            {0,{{0,2}},2,false,folia::ContainerPrefixRole::Literal},
            {0,{{2,4}},4,false,folia::ContainerPrefixRole::Literal}};
        const auto bound=index.bind(root,prefixes);
        expect(bound.lines.size()==2u);
        if (bound.lines.size()==2) {
            expect(bound.lines[0].text==U"> " && bound.lines[1].text==U"> ");
            expect(bound.lines[0].anchor.owner_id==folia::NodeId{10});
            expect(bound.lines[1].anchor==bound.lines[0].anchor);
        }
    };
    "parser_prefix_binding_separator_and_smallest_syntax_range_fallback"_test = [] {
        SourceOwnershipIndex index;
        index.add_separator(folia::NodeId{30},{0,4});
        index.add_syntax_range(folia::NodeId{40},{0,10});
        index.add_syntax_range(folia::NodeId{41},{2,8});
        index.seal();
        const auto separator=index.resolve(2,2);
        expect(separator && separator->part==folia::ContainerAnchorPart::Separator && separator->offset==2u);
        const auto syntax=index.resolve(4,6);
        expect(syntax && syntax->owner_id==folia::NodeId{41} && syntax->part==folia::ContainerAnchorPart::End);
    };

    "parser_scope_gap_excludes_container_syntax_once"_test = [] {
        const std::u32string source=U"> a\n> \n> b";
        RootPhysicalSource root(source);
        ContainerPrefixExclusions excluded;
        excluded.add({0,2}); excluded.add({4,6}); excluded.add({7,9});
        const auto pieces=excluded.retained({3,9});
        std::u32string gap;
        for (const auto piece : pieces) gap.append(root.text(piece));
        expect(gap==U"\n\n");
        expect(pieces.size()==2u);
        SourceOwnershipIndex index;
        std::size_t offset=0;
        for (const auto piece : pieces) { index.add_separator(folia::NodeId{30},piece,offset); offset+=piece.size(); }
        index.seal();
        const auto anchor=index.resolve(6,6);
        expect(anchor && anchor->owner_id==folia::NodeId{30} && anchor->offset==1u);
    };

};
