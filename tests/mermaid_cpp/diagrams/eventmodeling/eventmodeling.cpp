#include "support/folia_test.hpp"
import folia.mermaid.eventmodeling;
namespace {
using namespace boost::ut;
using namespace folia::mermaid;
suite eventmodeling = [] {
    "eventmodeling_parses_frames_and_relations"_test = [] {
        auto d=parse_eventmodeling(u"eventmodeling\ntf 01 ui Cart.UI\ntf 02 cmd AddItem ->> 01 { id: 1 }\nrf 03 evt ItemAdded ->> 02\n");
        expect(fatal(d.has_value()));
        expect(d->frames.size()==3_u);
        expect(d->frames[1].sources.size()==1_u);
        expect(d->frames[1].data==u" id: 1");
        expect(d->frames[2].reset);
        expect(d->frames[0].entity==u"Cart.UI");
    };
    "eventmodeling_resolves_data_block_references"_test = [] {
        auto d=parse_eventmodeling(u"eventmodeling\ntf 01 cmd Add [[payload]]\ndata payload\n{\nproductId: 7\n}\n");
        expect(fatal(d.has_value()));
        expect(d->frames[0].data==u"productId: 7");
    };
    "eventmodeling_rejects_bad_and_duplicate_frame_ids"_test = [] {
        expect(!parse_eventmodeling(u"eventmodeling\ntf 01 ui A\nrf 01 evt B\n"));
        expect(!parse_eventmodeling(u"eventmodeling\ntf 1000 ui A\n"));
        expect(parse_eventmodeling(u"eventmodeling\ntf 01 ui A ->> 09\n").has_value());
        expect(!parse_eventmodeling(u"eventmodeling\ntf 01 ui A junk\n"));
    };
    "mermaid_eventmodeling_accepts_typed_payloads_and_unresolved_references"_test = [] {
        const auto parsed = parse_eventmodeling(
            u"eventmodeling\n"
            u"/* Notes occupy source lines. */\n"
            u"entity Catalog.Cart.Payment\n"
            u"note 03 `text` {\nDesign notes.\n}\n"
            u"gwt 03 given ui Payment when cmd Pay then evt Paid\n"
            u"timeframe 01 ui Catalog.Cart.Payment\n"
            u"tf 02 cmd Pay [[Unknown]] `json` { id: 1 }\n"
            u"resetframe 03 evt Paid ->> 02 [[Payload]] `jsobj` { id: 2 }\n"
            u"data Payload `json` {\n  id: 3\n}\n"
            u"data Payload {\n  id: 4\n}\n");
        expect(fatal(parsed.has_value())) << (parsed ? "" : parsed.error());
        if (!parsed) return;
        expect(fatal(parsed->frames.size() == 3_u));
        expect(parsed->frames[0].entity == u"Catalog.Cart.Payment");
        expect(parsed->frames[0].line == 8_u);
        expect(parsed->frames[1].has_data && !parsed->frames[1].data_from_reference);
        expect(parsed->frames[1].data == u" id: 1");
        expect(parsed->frames[2].reset && parsed->frames[2].data_from_reference);
        expect(parsed->frames[2].data == u"  id: 3");
        expect(!parse_eventmodeling(u"eventmodeling\nnote 03"));
        expect(!parse_eventmodeling(u"eventmodeling\ntf 01 evt A `unknown` { x }"));
    };
};
}
