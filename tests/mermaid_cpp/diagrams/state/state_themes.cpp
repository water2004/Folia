#include "support/folia_test.hpp"
import folia.mermaid.state_svg;
import folia.mermaid.number;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;
#include "mermaid_cpp/support/theme_fixture.hpp"

std::string inner_fill(std::string_view svg,std::string_view id) {
    const auto group=svg.find("<g class=\"state-group\" data-id=\""+std::string{id}+"\">");
    if(group==svg.npos)throw std::runtime_error("Missing composite group");
    const auto end=svg.find("</g>",group);
    const auto outer=svg.find("<rect ",group);
    const auto inner=svg.find("<rect ",outer+1);
    const auto fill=svg.find("fill=\"",inner);
    if(inner>=end||fill>=end)throw std::runtime_error("Missing composite body");
    const auto begin=fill+6;
    return std::string{svg.substr(begin,svg.find('"',begin)-begin)};
}

void verify_backgrounds(std::u16string_view source,
    std::span<const std::pair<std::string_view,bool>> expected) {
    // The expected parity is the upstream dataFetcher's alternating altFlag,
    // beginning false for top-level composites (synthetic root starts true).
    // Keep both the classic and neo-dark preset values unchanged.
    for(const auto theme:{u"default",u"neo-dark"}) {
        for(const auto layout:{u"dagre",u"elk"}) {
            auto session=ConfigSession::create();
            expect(fatal(session.has_value()));
            auto config=json(uR"({"look":"classic"})");
            config.mapping.emplace(u"theme",ConfigValue{.kind=ConfigKind::string,.string=theme});
            config.mapping.emplace(u"layout",ConfigValue{.kind=ConfigKind::string,.string=layout});
            expect(fatal(session->initialize(config).has_value()));
            auto input=prepare_state(source,*session);
            expect(fatal(input.has_value()))<<(input?"":input.error());
            const auto composite=std::string(input->theme.composite.begin(),input->theme.composite.end());
            const auto alternate=std::string(input->theme.alternate.begin(),input->theme.alternate.end());
            expect(composite!=alternate);
            if(std::u16string_view{theme}==u"neo-dark") {
                expect(composite=="#333");
                expect(alternate=="#f0f0f0");
                expect(bool(input->theme.line==u"#cccccc"));
            }
            // This paint-only regression renders genuine, unlabelled geometry.
            // Never invent font dimensions: reject every measurement request.
            for(auto& node:input->document.nodes)node.label.clear();
            for(bool reverse:{false,true}) {
                if(reverse)std::ranges::reverse(input->document.nodes);
                std::size_t measurements=0;
                const auto svg=render_state_svg(*input,[&](std::u16string_view,double)
                    ->std::expected<StateTextBox,std::string> {
                    ++measurements;
                    return std::unexpected("Unexpected measurement in unlabelled paint test");
                });
                expect(fatal(svg.has_value()))<<(svg?"":svg.error());
                expect(measurements==0_u);
                for(const auto& [id,is_alternate]:expected)
                    expect(inner_fill(svg->svg,id)==(is_alternate?alternate:composite))<<id;
            }
        }
    }
}

suite state_themes=[] {
    "mermaid_state_composite_background_alternates_at_every_depth_and_for_siblings"_test=[] {
        constexpr auto source=uR"(stateDiagram-v2
state A {
    state B {
        state C {
            state D {
                state E {
                    state F { leafF }
                    state FPeer { leafFP }
                }
                state EPeer { leafEP }
            }
            state DPeer { leafDP }
        }
        state CPeer { leafCP }
    }
    state BPeer { leafBP }
}
state APeer { leafAP }
)";
        const std::array<std::pair<std::string_view,bool>,12> expected{{
            {"A",false},{"B",true},{"C",false},{"D",true},{"E",false},{"F",true},
            {"APeer",false},{"BPeer",true},{"CPeer",false},{"DPeer",true},{"EPeer",false},{"FPeer",true}}};
        verify_backgrounds(source,expected);
    };

    "mermaid_state_composite_background_counts_concurrent_region_scopes"_test=[] {
        constexpr auto source=uR"(stateDiagram-v2
state Outer {
    state Middle {
        state Deep {
            state Inner { [*] --> Left }
        }
        --
        state OtherDeep {
            state OtherInner { [*] --> Right }
        }
    }
}
)";
        const std::array<std::pair<std::string_view,bool>,6> expected{{
            {"Outer",false},{"Middle",true},{"Deep",true},{"Inner",false},
            {"OtherDeep",true},{"OtherInner",false}}};
        verify_backgrounds(source,expected);
    };
};
}
