#include <string>
#include <vector>
#include <unordered_set>
#include "support/folia_test.hpp"
import folia.markdown.ast;
import folia.markdown.block_tree;
import folia.markdown.document;
import folia.markdown.document_text;
import folia.markdown.inline_cst;
import folia.markdown.parser;
import folia.markdown.serializer;
using namespace folia;
using namespace boost::ut;

suite parser_container_frame_tests=[] {
    "container_frame_sources_and_csts_remain_exact"_test=[] {
        for (const auto& source:std::vector<std::string>{
            "> > a\n> > b\n> tail\n", "> a\n# heading\n",
            "- \n  body\n", "- a\n\n- b\n", "- first\n\n  second\n",
            "> [!NOTE] title\n> body\n", "[^n]: first\n    second\n",
            "$$x$$tail\n", "[TOC]tail\n", "![a](u)\n---\n",
            "![a](u)\ntext\n", "[a][r]\n\n> [r]: /url\n",
            "> - x\r\n>   y\r\n", "-\ttext\n\tmore\n"}) {
            auto parsed=parse_text(1,source);
            expect(serialize_markdown(parsed.document)==source);
            std::unordered_set<std::uint64_t> ids;
            walk_blocks(parsed.document.root,[&](const BlockNode& block) {
                expect(ids.insert(block.id.v).second);
                if (const auto* owner=editable_inline_document(block)) {
                    expect(flatten_tokens(owner->tree,owner->source)==owner->source);
                    expect(serialize_lossless(owner->tree,owner->source)==owner->source);
                }
            });
        }
    };
    "container_frame_empty_item_header_has_one_real_body"_test=[] {
        auto parsed=parse_text(1,"- \n  body\n");
        auto const& root=parsed.document.root;
        expect(fatal(root.children.size()==1u));
        auto const& list=root.children.front();
        expect(fatal(list.kind==BlockKind::List && list.children.size()==1u));
        auto const& item=list.children.front();
        expect(fatal(item.children.size()==1u));
        expect(item.children.front().inline_content.source==U"body");
    };
    "container_frame_lazy_line_cannot_swallow_interrupting_heading"_test=[] {
        auto parsed=parse_text(1,"> a\n# heading\n");
        expect(fatal(parsed.document.root.children.size()==2u));
        expect(parsed.document.root.children[0].kind==BlockKind::BlockQuote);
        expect(parsed.document.root.children[1].kind==BlockKind::Heading);
    };
    "container_frame_quoted_tab_residual_controls_heading_indentation"_test=[] {
        const std::string literal_source="> a\n>\t  # literal\n";
        auto literal=parse_text(1,literal_source);
        expect(fatal(literal.document.root.children.size()==1u));
        const auto& quote=literal.document.root.children.front();
        expect(fatal(quote.kind==BlockKind::BlockQuote && quote.children.size()==1u));
        if (quote.children.size()==1u) {
            expect(quote.children.front().kind==BlockKind::Paragraph);
            expect(quote.children.front().inline_content.source==U"a\n  # literal");
        }
        expect(serialize_markdown(literal.document)==literal_source);

        const std::string heading_source="> a\n>\t # heading\n";
        auto heading=parse_text(1,heading_source);
        expect(fatal(heading.document.root.children.size()==1u));
        const auto& heading_quote=heading.document.root.children.front();
        expect(fatal(heading_quote.kind==BlockKind::BlockQuote
            && heading_quote.children.size()==2u));
        if (heading_quote.children.size()==2u) {
            expect(heading_quote.children.back().kind==BlockKind::Heading);
            expect(heading_quote.children.back().inline_content.source==U"heading");
        }
        expect(serialize_markdown(heading.document)==heading_source);
    };
    "container_frame_lazy_quote_tab_residual_interrupts_list_paragraph"_test=[] {
        const std::string source = "> -   a\n>\t# heading\n";
        auto parsed = parse_text(1, source);
        expect(fatal(parsed.document.root.children.size() == 1u));
        const auto& quote = parsed.document.root.children.front();
        expect(fatal(quote.kind == BlockKind::BlockQuote && quote.children.size() == 2u));
        if (quote.children.size() == 2u) {
            const auto& list = quote.children.front();
            expect(fatal(list.kind == BlockKind::List && list.children.size() == 1u));
            if (list.kind == BlockKind::List && list.children.size() == 1u) {
                expect(fatal(list.children.front().children.size() == 1u));
                if (list.children.front().children.size() == 1u) {
                    expect(list.children.front().children.front().kind == BlockKind::Paragraph);
                    expect(list.children.front().children.front().inline_content.source == U"a");
                }
            }
            expect(quote.children.back().kind == BlockKind::Heading);
            expect(quote.children.back().inline_content.source == U"heading");
        }
        expect(serialize_markdown(parsed.document) == source);
    };
    "container_frame_markdown_image_display_follows_complete_leaf_grammar"_test=[] {
        auto setext=parse_text(1,"![a](u)\n---\n");
        expect(fatal(setext.document.root.children.size()==1u));
        expect(setext.document.root.children.front().kind==BlockKind::Heading);
        auto paragraph=parse_text(1,"![a](u)\ntext\n");
        expect(fatal(paragraph.document.root.children.size()==1u));
        expect(paragraph.document.root.children.front().kind==BlockKind::Paragraph);
    };
};

suite parser_container_opening_progress=[] {
    "container_frame_spaced_thematic_and_empty_openers_progress"_test=[] {
        for (const auto& source:std::vector<std::string>{"*\t*\t*\t\n", "* \t* \t* \t\n", "-\n", "-\n\ntext\n", "[^n]:\n"}) {
            auto parsed=parse_text(1,source);
            expect(serialize_markdown(parsed.document)==source);
            expect(!parsed.document.root.children.empty());
        }
        auto rule=parse_text(1,"*\t*\t*\t\n");
        expect(fatal(rule.document.root.children.size()==1u));
        expect(rule.document.root.children.front().kind==BlockKind::ThematicBreak);
        auto empty=parse_text(1,"-\n");
        expect(fatal(empty.document.root.children.size()==1u));
        expect(empty.document.root.children.front().kind==BlockKind::List);
    };
};

suite parser_list_frame_boundaries=[] {
    "container_frame_existing_list_accepts_all_sibling_markers"_test=[] {
        for (const auto& source:std::vector<std::string>{"1. foo\n2.\n3. bar\n", "1. foo\n2. bar\n3) baz\n", "* a\n*\n\n* c\n"}) {
            auto parsed=parse_text(1,source);
            expect(serialize_markdown(parsed.document)==source);
            expect(fatal(!parsed.document.root.children.empty()));
            expect(parsed.document.root.children.front().children.size()==(source[0]=='*' ? 3u : source.find("3)")!=std::string::npos ? 2u : 3u));
        }
    };
    "container_frame_blank_after_empty_item_closes_body"_test=[] {
        auto parsed=parse_text(1,"-\n\n  foo\n");
        expect(fatal(parsed.document.root.children.size()==2u));
        expect(parsed.document.root.children[0].kind==BlockKind::List);
        expect(parsed.document.root.children[1].kind==BlockKind::Paragraph);
        expect(serialize_markdown(parsed.document)=="-\n\n  foo\n");
    };
    "container_frame_retained_padding_applies_before_quote_recognition"_test=[] {
        auto parsed=parse_text(1,"-     > literal\n");
        expect(fatal(parsed.document.root.children.size()==1u));
        auto const& item=parsed.document.root.children.front().children.front();
        expect(fatal(item.children.size()==1u));
        expect(item.children.front().kind==BlockKind::CodeBlock);
        expect(serialize_markdown(parsed.document)=="-     > literal\n");
    };
};
