#include "support/folia_test.hpp"
import folia.mermaid.cynefin;
namespace {
using namespace boost::ut;
using namespace folia::mermaid;
suite cynefin=[] {
    "mermaid_cynefin_parser_matches_beta_grammar"_test=[] {
        auto parsed=parse_cynefin(u"cynefin-beta:\ntitle Framework\naccTitle: Cynefin\naccDescr: Five domains\ncomplex\n  \"Emergent practice\"\n  \"Probe-sense-respond\"\nclear\n  'Best practice'\ncomplex --> clear : \"Codified\"\n");
        expect(parsed.has_value());if(!parsed)return;
        expect(parsed->title==u"Framework");expect(parsed->acc_title==u"Cynefin");expect(parsed->acc_description==u"Five domains");
        expect(parsed->domains.size()==2_u);expect(parsed->domains[0].name==CynefinDomainName::complex);
        expect(parsed->domains[0].items.size()==2_u);expect(parsed->domains[0].items[1].label==u"Probe-sense-respond");
        expect(parsed->domains[1].items[0].label==u"Best practice");expect(parsed->transitions.size()==1_u);
        expect(parsed->transitions[0].from==CynefinDomainName::complex);expect(parsed->transitions[0].to==CynefinDomainName::clear);
        expect(parsed->transitions[0].label==u"Codified");
    };
    "mermaid_cynefin_rejects_unknown_domain_and_dangling_label"_test=[] {
        expect(!parse_cynefin(u"cynefin-beta\nunknown\n"));
        expect(!parse_cynefin(u"cynefin-beta\n\"orphan\"\n"));
        expect(!parse_cynefin(u"cynefin-beta\ncomplex --> unclear\n"));
    };
};
}
