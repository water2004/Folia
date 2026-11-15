#include "support/folia_test.hpp"

import folia.markdown.block_source;
import folia.markdown.text_edit;
import folia.markdown.source_offset_map;
import folia.markdown.parser;
import folia.markdown.ast;
import folia.markdown.serializer;
import folia.markdown.utf;
import folia.core.render_builder;
import folia.core.render_model;
import folia.core.theme;

using namespace folia;
using namespace boost::ut;

suite block_source_offset_storage_tests = [] {
    "block_source_offset_storage_linear_math_keeps_only_one_exact_run"_test = [] {
        const auto content = std::u32string(1'000'000, U'x');
        const auto document = make_block_source(U"$$\n" + content + U"\n$$", BlockSourceKind::DollarMath);
        expect(document.tree().content == content);
        const auto& offsets = document.tree().content_to_source;
        expect(offsets.size() == content.size() + 1);
        expect(offsets.run_count() == 1_u);
        expect(offsets.allocated_bytes() == 0_u);
        expect(offsets.front() == 3_u);
        expect(offsets.back() == content.size() + 3);
        for (const auto offset : {0u, 1u, 499'999u, 999'999u, 1'000'000u})
            expect(block_source_offset_for_content(document, offset) == offset + 3);
        expect(flatten_block_source_tokens(document) == document.source());
    };

    "block_source_offset_storage_keeps_indentation_CRLF_and_Unicode_boundaries"_test = [] {
        const auto document = make_block_source(U"    A\r\n\tB\n    \U0001F680e\u0301", BlockSourceKind::IndentedCode);
        expect(document.tree().content == U"A\nB\n\U0001F680e\u0301");
        const std::vector<std::size_t> expected{4, 5, 8, 9, 14, 15, 16, 17};
        expect(std::ranges::equal(document.tree().content_to_source, expected));
        expect(block_source_backward_delete_range(document, 17) == std::optional{SourceRange{15, 17}});
        expect(block_source_forward_delete_range(document, 15) == std::optional{SourceRange{15, 17}});
        expect(flatten_block_source_tokens(document) == document.source());
    };

    "block_source_offset_storage_keeps_HTML_entity_and_tag_gaps"_test = [] {
        const std::u32string source = U"<pre><code>A&amp;B<br>C&lt;D</code></pre>";
        const auto document = make_block_source(source, BlockSourceKind::HtmlCode);
        expect(document.tree().content == U"A&B\nC<D");
        const std::vector<std::size_t> expected{
            source.find(U'A'), source.find(U"&amp;"), source.find(U'B'), source.find(U"<br>"),
            source.find(U'C'), source.find(U"&lt;"), source.find(U'D'), source.find(U"</code>")};
        expect(std::ranges::equal(document.tree().content_to_source, expected));
        for (std::size_t index = 0; index < expected.size(); ++index)
            expect(block_source_offset_for_content(document, index) == expected[index]);
        expect(block_source_forward_delete_range(document, expected[1])
            == std::optional{SourceRange{expected[1], expected[2]}});
        expect(flatten_block_source_tokens(document) == source);
    };

    "block_source_offset_storage_render_copy_and_reparse_are_independent"_test = [] {
        const std::string source = "  ```text\r\n  first\r\n second\r\n  ```\r\n";
        auto parsed = parse_text(1, source);
        expect(fatal(parsed.document.root.children.size() == 1));
        auto& block = parsed.document.root.children.front();
        expect(fatal(block.kind == BlockKind::CodeBlock));
        const auto original = block.block_source.tree().content_to_source;
        const auto model = build_render_model(parsed.document, parsed.outline, parsed.symbols, default_theme_profile());
        expect(fatal(model.blocks.size() == 1));
        expect(model.blocks.front().special().content_to_source == original);
        expect(serialize_markdown(parsed.document) == source);
        block.block_source.source() = U"```text\nx\n```";
        reparse_block_source(block.block_source);
        expect(block.block_source.tree().content_to_source.run_count() == 1_u);
        expect(block.block_source.tree().content_to_source.allocated_bytes() == 0_u);
        expect(model.blocks.front().special().content_to_source == original);
        expect(model.blocks.front().special().raw_source != block.block_source.source());
    };
};
