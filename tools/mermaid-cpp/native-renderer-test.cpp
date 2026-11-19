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
Result render_named(std::string_view source, std::string_view theme) {
    Result result;
    std::uint8_t* data = nullptr; std::size_t size = 0;
    result.status = FoliaNativeMermaidRenderWithTheme(
        reinterpret_cast<const std::uint8_t*>(source.data()), source.size(),
        reinterpret_cast<const std::uint8_t*>(theme.data()), theme.size(),
        &data, &size, &result.width, &result.height);
    if (data) result.text.assign(reinterpret_cast<const char*>(data), size);
    FoliaNativeMermaidFree(data);
    return result;
}
suite host_contract = [] {
    "mermaid_host_named_themes_and_source_overrides"_test = [] {
        for (const auto theme : {"default", "dark", "base", "forest", "neutral",
            "neo", "neo-dark", "redux", "redux-dark", "redux-color", "redux-dark-color"}) {
            const auto named = render_named("info showInfo", theme);
            const auto local = render(std::string{"---\nconfig:\n  theme: "} +
                theme + "\n---\ninfo showInfo");
            expect(named.status == 0_i) << named.text;
            expect(local.status == 0_i) << local.text;
            expect(named.text == local.text);
            expect(named.width == local.width && named.height == local.height);
        }
        const auto forest = render_named("packet\n0-7: \"Data\"", "forest");
        const auto dark = render_named("packet\n0-7: \"Data\"", "dark");
        const auto override = render_named(
            "---\nconfig: {theme: dark}\n---\npacket\n0-7: \"Data\"", "forest");
        expect(forest.status == 0_i && dark.status == 0_i && override.status == 0_i);
        expect(override.text == dark.text);
        expect(render_named("packet\n0-7: \"Data\"", "forest").text == forest.text);
        // Explicit classic retains legacy bool-entry appearance exactly.
        const auto classic = "---\nconfig: {look: classic}\n---\nflowchart LR\nA-->B";
        for (const bool is_dark : {false, true}) {
            const auto named = render_named(classic, is_dark ? "dark" : "default");
            const auto legacy = render(classic, is_dark);
            expect(named.status == 0_i) << named.text;
            expect(legacy.status == 0_i) << legacy.text;
            expect(named.text == legacy.text);
        }
    };
    "mermaid_host_named_theme_errors_are_bounded_and_owned"_test = [] {
        for (const auto theme : std::array<std::string_view, 6>{
            "", "unknown", "Dark", "\xff", std::string_view{"default\0suffix", 14},
            "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"}) {
            const auto result = render_named("info", theme);
            expect(result.status == 1_i);
            expect(result.width == 0 && result.height == 0);
            expect(result.text == "Unsupported native theme");
        }
        std::uint8_t* data = nullptr; std::size_t size = 0;
        float width = 0, height = 0;
        const auto source = reinterpret_cast<const std::uint8_t*>("info");
        expect(FoliaNativeMermaidRenderWithTheme(source, 4, nullptr, 0,
            &data, &size, &width, &height) == -1_i);
        FoliaNativeMermaidFree(data);
        expect(render_named("info", "forest").status == 0_i);
    };
    "mermaid_host_renders_math_as_svg_across_label_consumers"_test = [] {
        for (const auto source : {
            R"(flowchart TD
A["$$\nabla f$$"] -->|$$x_1$$| B["$$\frac{a}{b}$$"])",
            R"(---
config:
  layout: dagre
---
flowchart TD
subgraph G["$$\sum_{k=1}^{n}k$$"]
  A["value $$x_1$$"]
end
style G font-size:24px)",
            R"(flowchart TD
A["`**literal** $$x_1$$`"] --> B["<strong>value</strong> <em>f</em>$$\sqrt{x}$$"])",
            R"(sequenceDiagram
box $$\mathbb{R}$$
participant A as $$x_1$$
participant B as $$x_2$$
end
A->>B: $$\frac{a}{b}$$
Note over A,B: $$\sum_{i=1}^n i$$
loop $$n>0$$
  B-->>A: $$\begin{pmatrix}a&b\\c&d\end{pmatrix}$$
end)",
            R"(mindmap
  root(("$$\nabla f$$"))
    child["$$\frac{a}{b}$$"])",
            R"(classDiagram
class A["$$\mathbb{R}$$"]
A : +$$x_1$$ value
A --> A : $$\frac{a}{b}$$)",
            R"(stateDiagram-v2
state "$$\nabla f$$" as A
A --> A : $$x_1$$
note right of A : $$\frac{a}{b}$$)",
            R"(agentflow-beta TB
flow G["$$\mathbb{R}$$"]
A["$$\nabla f$$"] -->|$$x_1$$| B["$$\frac{a}{b}$$"]
end)",
            R"(block-beta
columns 2
A["$$\frac{a}{b}$$"] B{"$$\nabla f$$"}
A -- "$$x_1$$" --> B)",
            R"(erDiagram
A["$$\mathbb{R}$$"] {
  int value "$$\frac{a}{b}$$"
}
A ||--o{ B : "$$x_1$$")",
            R"(requirementDiagram
requirement R {
id: R1
text: "$$\frac{a}{b}$$"
risk: low
verifymethod: test
})",
            R"(usecase-beta
actor A("$$x^2$$")
U("$$\frac{a}{b}$$")
A -- "$$n$$" --> U)",
            R"(---
config:
  flowchart:
    htmlLabels: true
---
swimlane-beta TB
subgraph L["$$\frac{a}{b}$$"]
A["$$x^2$$"]
B["$$y$$"]
end
A -->|"$$\sqrt{x}$$"| B)"}) {
            for (const bool dark : {false,true}) {
                const auto result = render(source,dark);
                expect(result.status == 0_i) << result.text;
                expect(std::isfinite(result.width) && result.width > 0);
                expect(std::isfinite(result.height) && result.height > 0);
                expect(result.text.find("<path") != std::string::npos);
                expect(result.text.find("$$") == std::string::npos);
                expect(result.text.find("<foreignObject") == std::string::npos);
                expect(result.text.find("<mjx-container") == std::string::npos);
            }
        }
        // The pinned Mermaid SVG-text mode keeps dollar delimiters literal.
        const auto literal = render(R"(---
config:
  htmlLabels: false
---
flowchart TD
A["$$x_1$$"])");
        expect(literal.status == 0_i) << literal.text;
        expect(literal.text.find("$$x_1$$") != std::string::npos);
    };

    "mermaid_host_requires_mermaid_source_for_compound_feedback_edges"_test = [] {
        constexpr auto escaped = R"(flowchart TB\
&#x20; subgraph client[客户端]\
&#x20;   A["请求\<br/>数据"] --> B[显示]\
&#x20; end\
&#x20; subgraph service[服务端]\
&#x20;   C --> D\
&#x20; end\
&#x20; A -->|发送| C\
&#x20; D -. "返回" .-> B)";
        constexpr auto clean = R"(flowchart TB
  subgraph client[客户端]
    A["请求<br/>数据"] --> B[显示]
  end
  subgraph service[服务端]
    C --> D
  end
  A -->|发送| C
  D -. "返回" .-> B)";
        for (bool dark : {false, true}) {
            const auto diagram = render(clean, dark);
            const auto invalid = render(escaped, dark);
            expect(diagram.status == 0_i) << diagram.text;
            expect(diagram.width > 0 && diagram.height > 0);
            expect(diagram.text.find("返回") != std::string::npos);
            expect(invalid.status == 1_i && invalid.width == 0 && invalid.height == 0);
        }
        const auto unoriented = render(std::string{
            "---\nconfig:\n  elk:\n    orientFeedbackEdges: false\n---\n"} + clean);
        expect(unoriented.status == 0_i) << unoriented.text;
        expect(unoriented.width > 0 && unoriented.height > 0);
        expect(unoriented.text != render(clean).text);
        expect(render("flowchart TB\\\nA --> B").status == 1_i);
        expect(render("flowchart TB\\\n&#x20; subgraph unfinished").status == 1_i);
        const auto literal = render(R"(flowchart TB
A["&#x20; C:\work"] --> B)");
        expect(literal.status == 0_i) << literal.text;
        expect(literal.text.find(R"(C:\work)") != std::string::npos);
    };
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
            "quadrantChart\nEditor: [0.2, 0.7]", "radar-beta\naxis A,B,C\ncurve current{1,2,3}",
            "erDiagram\nCUSTOMER ||--o{ ORDER : places",
            "requirementDiagram\nrequirement req1 {\nid: R1\ntext: Save\nrisk: high\nverifyMethod: test\n}",
            "info showInfo", "treeView-beta\nsrc/\n  main.cpp",
            "ishikawa-beta\nProblem\nCause A", "cynefin-beta\nclear\n  \"Best practice\"",
            "venn-beta\nset A\nset B\nunion A,B[Both]",
            "railroad-beta\ndigit = terminal('0');",
            "agentflow-beta TB\nRead[\"Read\"] --> Write[\"Write\"]",
            "swimlane-beta LR\nsubgraph Work\nRead --> Write\nend",
            "eventmodeling\ntf 01 ui Cart.UI\ntf 02 cmd AddItem ->> 01",
            "wardley-beta\ncomponent Service [0.8, 0.55]",
            "usecase-beta\nactor User\nUser --> Login"}) {
            auto result = render(source);
            expect(result.status == 0_i) << result.text;
            expect(result.width > 0 && result.height > 0);
        }
        auto default_class = render("classDiagram\nclass Document");
        expect(default_class.status == 0_i) << default_class.text;
        expect(default_class.width > 0 && default_class.height > 0);
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
    "mermaid_host_renders_zenuml_through_the_public_entry"_test = [] {
        const auto source = "zenuml\n@Actor A as Alice\n@Database B as Store\nA->B: Save";
        const auto first = render(source);
        expect(first.status == 0_i) << first.text;
        expect(first.width > 0 && first.height > 0);
        expect(first.text.find("Alice") != std::string::npos);
        expect(first.text.find("Store") != std::string::npos);
        expect(render("zenuml\nA.method() {\n").status == 1_i);
        expect(render(source).text == first.text);
    };
};
}
