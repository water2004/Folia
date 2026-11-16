#include "support/folia_test.hpp"
import folia.mermaid.c4;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;

suite c4_regressions = [] {
    "mermaid_c4_external_queues_and_indexed_relationships"_test = [] {
        const auto document = parse_c4(uR"(C4Dynamic
SystemQueue_Ext(input, "External events", "Incoming transactions")
Container_Boundary(app, "Application") {
  ContainerQueue_Ext(commands, "Command queue", "AMQP", "Pending work")
  ComponentQueue_Ext(results, "Results", "FIFO", "Completed work")
}
RelIndex(7, input, commands, "Dispatch", "AMQP")
BiRel(commands, results, "Acknowledgements")
UpdateRelStyle(input, commands, $offsetX="23", $offsetY="-17")
)");
        expect(fatal(document.has_value())) << (document ? "" : document.error());
        expect(document->shapes.size() == 3_u);
        expect(document->shapes[0].type == u"external_system_queue");
        expect(document->shapes[1].type == u"external_container_queue");
        expect(document->shapes[2].type == u"external_component_queue");
        expect(document->shapes[1].parent == u"app");
        expect(document->shapes[1].technology == std::optional<std::u16string>{u"AMQP"});
        expect(document->relations.size() == 2_u);
        expect(document->relations[0].type == u"rel");
        expect(document->relations[0].from == u"input");
        expect(document->relations[0].to == u"commands");
        expect(document->relations[0].label == u"Dispatch");
        expect(document->relations[0].styles.at(u"offsetX") == u"23");
        expect(document->relations[0].styles.at(u"offsetY") == u"-17");
        expect(document->relations[1].type == u"birel");
    };
};
} // namespace
