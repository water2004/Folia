#include "native-renderer.h"
#include "support/folia_test.hpp"

namespace {
using namespace boost::ut;
struct Result { int status; std::string text; float width{}, height{}; };
Result render(std::string_view source, bool dark = false) {
    Result result;
    std::uint8_t* data = nullptr; std::size_t size = 0;
    result.status = FoliaNativeMermaidRender(
        reinterpret_cast<const std::uint8_t*>(source.data()), source.size(), dark,
        &data, &size, &result.width, &result.height);
    if (data) result.text.assign(reinterpret_cast<const char*>(data), size);
    FoliaNativeMermaidFree(data);
    return result;
}
suite host_contract = [] {
    "mermaid_host_utf8_theme_and_request_isolation"_test = [] {
        const auto source = "pie\n\"文档 😀\": 3\n\"Other\": 1";
        auto light = render(source), dark = render(source, true);
        expect(light.status == 0_i) << light.text;
        expect(dark.status == 0_i) << dark.text;
        expect(light.width > 0 && light.height > 0);
        expect(light.text.find("文档 😀") != std::string::npos);
        expect(light.text != dark.text);
        auto override = render("%%{init: {\"theme\": \"dark\"}}%%\npie\n\"文档 😀\": 3\n\"Other\": 1");
        expect(override.status == 0_i) << override.text;
        expect(override.text == dark.text);
        expect(render(source).text == light.text);
    };
    "mermaid_host_family_defaults_and_errors"_test = [] {
        for (auto source : {"packet\n0-7: \"Header\"", "sequenceDiagram\nA->>B: Save",
            "quadrantChart\nEditor: [0.2, 0.7]", "radar-beta\naxis A,B,C\ncurve current{1,2,3}"}) {
            auto result = render(source);
            expect(result.status == 0_i) << result.text;
        }
        auto default_class = render("classDiagram\nclass Document");
        expect(default_class.status == 1_i);
        expect(default_class.text.find("ELK") != std::string::npos);
        auto default_flow = render("flowchart TD\nA-->B");
        expect(default_flow.status == 0_i) << default_flow.text;
        expect(default_flow.width > 0 && default_flow.height > 0);
        auto explicit_flow = render("---\nconfig:\n  layout: dagre\n---\nflowchart TD\nA-->B");
        expect(explicit_flow.status == 0_i) << explicit_flow.text;
        auto explicit_class = render("---\nconfig:\n  layout: dagre\n---\nclassDiagram\nclass Document");
        expect(explicit_class.status == 0_i) << explicit_class.text;
        auto invalid = render("\xff");
        expect(invalid.status == 1_i && invalid.width == 0 && invalid.height == 0);
        expect(render("pie\n\"Valid\": 1").status == 0_i);
    };
};
}
