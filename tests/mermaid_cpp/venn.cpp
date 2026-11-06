#include "support/folia_test.hpp"
import folia.mermaid.venn_layout;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;

suite mermaid_venn = [] {
    "mermaid_venn_parser_preserves_labels_and_defaults"_test = [] {
        auto result = parse_venn(u"venn-beta\nset \"A one\" [\"First label\"]\n"
            u"  text one [One]\nset B : 8\nunion B,\"A one\" [Shared]\n"
            u"style B,\"A one\" fill:rgba(1,2,3,0.5),color:#333\n");
        expect(fatal(result.has_value()));
        expect(result->areas.size() == 3_u);
        expect(static_cast<bool>(result->areas[0].label == u"First label"));
        expect(result->areas[0].size == 10.0);
        expect(result->areas[1].size == 8.0);
        expect(result->areas[2].size == 2.5);
        expect(static_cast<bool>(result->areas[2].sets == VennSets{u"A one", u"B"}));
        expect(static_cast<bool>(result->texts[0].sets == VennSets{u"A one"}));
        expect(static_cast<bool>(result->styles.at({u"A one", u"B"}).at(u"fill") == u"rgba(1,2,3,0.5)"));
    };
    "mermaid_venn_rejects_invalid_source"_test = [] {
        for (auto source : {u"venn-beta\nunion A,B", u"venn-beta\nset A\nunion A",
            u"venn-beta\nset A [\"Missing]", u"venn-beta\nset A : -2", u"venn-beta\nset A\nstyle A fill"}) {
            expect(!parse_venn(source));
        }
    };
    "mermaid_venn_implicit_pair_geometry"_test = [] {
        auto result = parse_venn(u"venn-beta\nset A\nset B\nset C\nunion A,B,C[Shared]");
        expect(fatal(result.has_value()));
        auto regions = layout_venn(*result, 800, 450, 15);
        expect(fatal(regions.has_value()));
        expect(regions->size() == 7_u);
        expect((*regions)[3].arcs.size() == 3_u);
        expect((*regions)[4].arcs.size() == 2_u);
        expect((*regions)[5].arcs.size() == 2_u);
        expect((*regions)[6].arcs.size() == 2_u);
    };
    "mermaid_venn_rejects_more_than_three_nonempty_sets"_test = [] {
        auto result = parse_venn(u"venn-beta\nset A\nset B\nset C\nset D");
        expect(fatal(result.has_value()));
        auto regions = layout_venn(*result, 800, 450, 15);
        expect(fatal(!regions.has_value()));
        expect(static_cast<bool>(regions.error() == "Venn layout supports up to three nonempty sets"));
    };
};
}
