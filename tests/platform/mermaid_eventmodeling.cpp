#include "support/folia_test.hpp"
import folia.platform.mermaid_renderer;

namespace {
using namespace boost::ut;
using namespace folia::platform;
using namespace folia::mermaid;

suite mermaid_eventmodeling = [] {
    "mermaid_eventmodeling_directwrite_svg_default_and_dark"_test = [] {
        const std::vector<std::pair<std::string, std::u16string>> cases = {
            {"basic", u"eventmodeling\ntf 01 ui UI\ntf 02 cmd RunAction\ntf 03 evt ActionExecuted"},
            {"qualified", u"eventmodeling\ntf 01 ui Cart.UI\ntf 02 cmd Inventory.AddItem ->> 01\ntf 03 evt Inventory.ItemAdded ->> 02"},
            {"data", u"eventmodeling\ntf 01 ui UI\ntf 02 cmd AddItem { productId: 7 }\ntf 03 evt ItemAdded [[ItemData]]\ndata ItemData\n{\n productId: 7\n}"},
            {"reset", u"eventmodeling\ntf 01 ui UI\ntf 02 cmd Run\nrf 03 evt Reset"},
            {"relations", u"eventmodeling\ntf 01 ui Cart\ntf 02 cmd Add\ntf 03 cmd Remove\ntf 04 evt Changed ->> 03 ->> 02\nrf 05 rmo CartView ->> 04\ntf 06 processor Update ->> 05"},
            {"namespaces", u"eventmodeling\ntf 01 ui Catalog.Cart\ntf 02 ui Catalog.Checkout\ntf 03 cmd Catalog.PlaceOrder\ntf 04 event Catalog.OrderPlaced\ntf 05 readmodel Catalog.CartView\ntf 06 ui Shop.Cart.Payment"},
            {"formatted-payload", u"eventmodeling\ntf 01 evt Output `html` { <script>ignore()</script><em onclick=\"ignored\">Paid</em> &amp; <strong>Ready</strong><br><sup>2</sup> }"},
        };
        for (const auto& [name, source] : cases) {
            for (const auto theme : {u"default", u"dark"}) {
                auto session = ConfigSession::create();
                expect(fatal(session.has_value()));
                ConfigValue site;
                site.kind = ConfigKind::mapping;
                ConfigValue value;
                value.kind = ConfigKind::string;
                value.string = u"Arial";
                site.mapping.emplace(u"fontFamily", value);
                value.string = theme;
                site.mapping.emplace(u"theme", std::move(value));
                expect(fatal(session->set_site_config(site).has_value()));
                auto result = render_mermaid(source, *session);
                expect(fatal(result.has_value())) << name << (result ? "" : result.error());
                if (!result) continue;
                expect(result->width > 0 && result->height > 0);
                expect(result->svg.find("<foreignObject") == std::string::npos);
                expect(result->svg.find("<g class=\"em-label\"") != std::string::npos);
                if (auto dir = std::getenv("FOLIA_MERMAID_SVG_OUTPUT")) {
                    std::filesystem::create_directories(dir);
                    auto theme_name = std::u16string_view(theme) == u"dark" ? "dark" : "default";
                    const auto filename = "eventmodeling-" + name + "-" + theme_name + ".svg";
                    std::ofstream file(std::filesystem::path(dir) / filename, std::ios::binary);
                    file << result->svg;
                    expect(file.good());
                }
            }
        }
    };
};
}
