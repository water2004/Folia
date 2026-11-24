#include "support/folia_test.hpp"
import folia.mermaid.sequence;
import folia.mermaid.sequence_svg;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;
suite sequence_regressions=[] {
    "mermaid_sequence_actor_menus_match_original_database_order_and_updates"_test=[] {
        auto session=ConfigSession::create();
        expect(fatal(session.has_value()));
        // SequenceDB.insertLinks enumerates numeric keys first, replaces an
        // existing URL in place, and preserves the order of other keys.
        auto result=prepare_sequence(uR"(sequenceDiagram
participant A
links A: {"Wiki":"https://example.org/old","10":"https://example.org/ten","Dashboard":"https://example.org/dash?a=1&b=2"}
link A: 2 @ https://example.org/two
links A: {"Wiki":"https://example.org/new","Source<br/>code":"https://example.org/code"}
A->>B: Go
)",*session);
        expect(fatal(result.has_value())) << (result?"":result.error());
        const auto& links=result->actors[0].links;
        const std::vector<std::u16string> labels{u"2",u"10",u"Wiki",u"Dashboard",u"Source<br/>code"};
        expect(fatal(links.size()==labels.size()));
        for(std::size_t i=0;i<labels.size();++i) expect(bool(links[i].label==labels[i]));
        expect(bool(links[2].url==u"https://example.org/new"));
        expect(bool(links[3].url==u"https://example.org/dash?a=1&b=2"));
        // A later explicit participant declaration constructs a new actor in
        // upstream SequenceDB; incidental message references retain its links.
        auto reset=prepare_sequence(u"sequenceDiagram\nlink A: Old @ https://example.org/old\nparticipant A as New\nlinks A: {\"Fresh\":\"https://example.org/new\"}\nA->>B: Go",*session);
        expect(fatal(reset.has_value()));
        expect(fatal(reset->actors[0].links.size()==1_u));
        expect(bool(reset->actors[0].links[0].label==u"Fresh"));
        auto invalid=prepare_sequence(u"sequenceDiagram\nlinks A: {\"broken\":}\nA->>B: Go",*session);
        expect(fatal(invalid.has_value()));
        expect(invalid->actors[0].links.empty());
        SequenceLimits limits;limits.links=1;
        expect(!prepare_sequence(u"sequenceDiagram\nlinks A: {\"First\":\"one\",\"Second\":\"two\"}",*session,limits));
    };
    "mermaid_sequence_menu_geometry_uses_literal_measurement_when_hidden_or_visible"_test=[] {
        auto session=ConfigSession::create();
        expect(fatal(session.has_value()));
        auto doc=prepare_sequence(u"sequenceDiagram\nparticipant A\nlinks A: {\"$$x$$\":\"https://example.org/math\",\"Source\":\"https://example.org/code\"}",*session);
        expect(fatal(doc.has_value()));
        SequenceRenderInput input;
        input.document=std::move(*doc);input.config.mirror_actors=false;
        SequenceMeasurements measured;
        measured.actors.push_back({12,16,u"A",{u"A"},16});
        expect(!render_sequence_svg(input,measured));
        std::vector<std::u16string> requests;
        // Supplied boxes test the measurement contract; they are not font or
        // browser geometry expectations. Menu labels never enter MathJax.
        measured.measure_menu=[&](std::u16string_view text)->std::expected<SequenceTextBox,std::string> {
            requests.emplace_back(text);
            return SequenceTextBox{600,16,std::u16string{text},{std::u16string{text}},16};
        };
        const auto hidden=render_sequence_svg(input,measured);
        expect(fatal(hidden.has_value())) << (hidden?"":hidden.error());
        expect(hidden->width==740.0);expect(hidden->height==165.0);
        expect(hidden->svg.find("class=\"actorPopupMenu\" display=\"none\"")!=hidden->svg.npos);
        expect(hidden->svg.find("$$x$$")!=hidden->svg.npos);
        expect(fatal(requests.size()==2_u));expect(bool(requests[0]==u"$$x$$"));
        input.config.force_menus=true;
        const auto visible=render_sequence_svg(input,measured);
        expect(fatal(visible.has_value()));
        expect(visible->width==hidden->width);expect(visible->height==hidden->height);
        expect(visible->svg.find("class=\"actorPopupMenu\" display=\"block\"")!=visible->svg.npos);
        measured.measure_menu=[](std::u16string_view)->std::expected<SequenceTextBox,std::string> {
            return std::unexpected("Menu measurement failed");
        };
        expect(!render_sequence_svg(input,measured));
    };
};
}
