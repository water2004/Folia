#include "support/folia_test.hpp"
import folia.mermaid.wardley;
namespace { using namespace boost::ut; using namespace folia::mermaid;
suite mermaid_wardley = [] {
 "mermaid_wardley_parses_components_links_and_evolution"_test = [] {
   auto d=parse_wardley(u"wardley-beta\ntitle Example\nanchor User [1.0, 0.1]\ncomponent API [0.7, 0.65] (buy) inertia\ncomponent DB [0.4, 0.9]\nAPI --> DB\nevolve API 0.9\nnote \"Important\" [0.5, 0.4]\n");
   expect(fatal(d.has_value())); expect(d->title==u"Example");expect(d->nodes.size()==3_u);expect(d->nodes[0].x==10.0);expect(d->nodes[1].y==70.0);expect(d->nodes[1].inertia);expect(d->nodes[1].strategy==u"buy");expect(d->links.size()==1_u);expect(d->trends.size()==1_u);expect(d->notes.size()==1_u);
 };
 "mermaid_wardley_rejects_invalid_coordinate_and_header"_test = [] {expect(!parse_wardley(u"not-a-map"));expect(!parse_wardley(u"wardley-beta\ncomponent A [101.0, 0.5]"));expect(!parse_wardley(u"wardley-beta\ncomponent A [1, 0.5]"));expect(!parse_wardley(u"wardley-beta\ncomponent A [0.8 0.5]"));expect(!parse_wardley(u"wardley-beta\npipeline Parent {\ncomponent Item [0.5]"));};
}; }
