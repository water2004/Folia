#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "support/folia_test.hpp"
import folia.markdown.ast;
import folia.markdown.parser;
import folia.markdown.serializer;
import folia.markdown.utf;

using namespace folia;
using namespace boost::ut;

suite image_source_roundtrip_tests = [] {
    "standalone_images_keep_the_original_inline_source"_test = [] {
        constexpr std::array<std::string_view, 4> inputs{
            "![*alt*](pic.png \"title\")\n",
            "![foo]\n\n[foo]: /url \"title\"\n",
            "[![alt **strong**](pic.png \"t\")](https://example.test)\n",
            "![foo *bar*]\n\n[foo *bar*]: train.jpg \"train & tracks\"\n",
        };

        std::uint64_t document_id = 1;
        for (const auto markdown : inputs) {
            const auto parsed = parse_text(document_id++, std::string(markdown));
            const auto found = std::ranges::find_if(
                parsed.document.root.children,
                [](const BlockNode& block) { return block.kind == BlockKind::ImageBlock; });
            expect(fatal(found != parsed.document.root.children.end()));
            if (found == parsed.document.root.children.end()) continue;

            const auto line_end = markdown.find_first_of("\r\n");
            const auto image_line = markdown.substr(0, line_end);
            const auto expected_inline = utf8_to_cps(image_line);
            expect(fatal(found->inline_content.source == expected_inline));
            expect(fatal(serialize_markdown(parsed.document) == markdown));
        }
    };
};
