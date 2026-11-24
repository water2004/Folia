#include "support/folia_test.hpp"
import folia.math.output.scene.builder;
import folia.math.output.svg.serializer;
import folia.math.output.svg.writer;

namespace {
using namespace boost::ut;
namespace scene = folia::math::output::scene;
namespace svg = folia::math::output::svg;

scene::Scene make_scene() {
    scene::Builder builder;
    builder.set_metrics({2.0, 0.8, 0.2});
    builder.set_font_metrics({1.0, 0.5, 0.45, 0.7});
    return std::move(builder).freeze();
}
} // namespace

suite native_svg_writer_text = [] {
    "XML text and attributes escape markup characters"_test = [] {
        scene::Builder builder;
        builder.set_metrics({1, 0.5, 0.1});
        builder.set_font_metrics({1, 0.5, 0.5, 0.7});
        scene::SemanticMetadata metadata;
        metadata.source_tag = "mtext";
        metadata.ordered_attributes.push_back({"aria-label", "a&\"<b>"});
        metadata.ordered_attributes.push_back({"data-origin", "source"});
        auto group = builder.add_group(0, {}, {}, {}, {}, metadata);
        builder.add_primitive(group, scene::TextRun{"<&>"});
        auto output =
            svg::write(std::move(builder).freeze(), {.font_cache = svg::FontCacheMode::none});
        expect(output.find("aria-label=\"a&amp;&quot;&lt;b&gt;\"") != output.npos);
        expect(output.find("aria-label=") < output.find("data-origin=\"source\""));
        expect(output.find("><text transform=\"scale(1,-1)\">&lt;&amp;&gt;</text>") != output.npos);
    };

    "SVG attributes preserve long spans adjacent escapes empty and UTF8 values"_test = [] {
        const std::string plain(4096, 'x');
        const std::string utf8 = "\xCE\xB1\xF0\x9F\x98\x80";
        svg::SvgWriteOptions options;
        options.font_cache = svg::FontCacheMode::none;
        options.container_attributes = {{"data-long", "&\"<>" + plain + utf8 + "'&"},
                                        {"data-empty", ""}};
        const auto output = svg::write(make_scene(), options);
        expect(output.starts_with("<mjx-container data-long=\"&amp;&quot;&lt;&gt;" + plain +
                                  utf8 + "'&amp;\" data-empty=\"\">"));
        expect(options.container_attributes[0].value == "&\"<>" + plain + utf8 + "'&");
    };

    "owned HTML materialization follows Lite XML serialization rules"_test = [] {
        scene::HtmlNodePrimitive root;
        root.kind = u"div";
        root.namespaceHint = u"http://www.w3.org/1999/xhtml";
        root.attributes.push_back({u"title", u"a\"<&"});
        root.children.push_back({u"#text", {}, {}, u"before < &", {}});
        root.children.push_back({u"#comment", {}, {}, u"<!--kept-->", {}});
        root.children.push_back({u"br", u"http://www.w3.org/1999/xhtml", {}, {}, {}});
        root.children.push_back({u"span", {}, {}, {}, {}});
        root.children.push_back({u"script", {}, {}, {}, {{u"#text", {}, {}, u"x < y && z", {}}}});
        root.children.push_back({u"svg:svg",
                                 u"http://www.w3.org/2000/svg",
                                 {{u"xmlns:svg", u"http://www.w3.org/2000/svg"}},
                                 {},
                                 {{u"svg:rect",
                                   u"http://www.w3.org/2000/svg",
                                   {{u"xmlns:svg", u"http://www.w3.org/2000/svg"}},
                                   {},
                                   {}}}});

        auto serialized = svg::serializeHtml(root);
        expect(serialized ==
               "<div title=\"a&quot;&lt;&amp;\">before &lt; &amp;<!--kept--><br/><span/>"
               "<script>x < y && z</script><svg:svg xmlns:svg=\"http://www.w3.org/2000/svg\">"
               "<svg:rect xmlns:svg=\"http://www.w3.org/2000/svg\"/></svg:svg></div>");
    };

    "foreignObject uses CSS pixel projection and ordered owned HTML"_test = [] {
        scene::Builder builder;
        builder.set_metrics({2, 0.8, 0.2});
        builder.set_font_metrics({1, 0.5, 0.5, 0.7});
        scene::HtmlNodePrimitive content{u"div",
                                         u"http://www.w3.org/1999/xhtml",
                                         {},
                                         {},
                                         {{u"#text", {}, {}, u"foreign<&>", {}}}};
        scene::ForeignContent foreign{
            std::move(content), {8.1234, 4.5678, 24.9876, 12.3456, 0.062345}, {}};
        auto const group =
            builder.add_group(0,
                              scene::Transform{std::vector<scene::TransformOperation>{
                                  scene::Translation{0.25, -0.5}}},
                              {}, {}, {}, {}, scene::ElementRole::foreign_content_viewport, {}, {},
                              std::move(foreign));
        (void)group;
        auto output =
            svg::write(std::move(builder).freeze(), {.font_cache = svg::FontCacheMode::none});
        expect(output.find("<foreignObject data-mjx-xml=\"true\" x=\"8.123px\" y=\"4.568px\" "
                           "width=\"24.988px\" height=\"12.346px\" transform=\"translate(250 -500) "
                           "scale(62.345) matrix(1 0 0 -1 0 0)\"><div>foreign&lt;&amp;&gt;</div>"
                           "</foreignObject>") != output.npos);

        scene::Builder boundary_builder;
        boundary_builder.set_metrics({1, 0.5, 0.1});
        boundary_builder.set_font_metrics({1, 0.5, 0.5, 0.7});
        scene::HtmlNodePrimitive empty{u"div", u"http://www.w3.org/1999/xhtml", {}, {}, {}};
        scene::ForeignContent boundary{
            std::move(empty), {-0.00055, 0.00055, 0.00055, 0.00155, 0.00000055}, {}};
        (void)boundary_builder.add_group(0, {}, {}, {}, {}, {},
                                         scene::ElementRole::foreign_content_viewport, {}, {},
                                         std::move(boundary));
        auto boundary_output = svg::write(std::move(boundary_builder).freeze(),
                                          {.font_cache = svg::FontCacheMode::none});
        expect(boundary_output.find("<foreignObject data-mjx-xml=\"true\" x=\"0px\" y=\"0px\" "
                                    "width=\"0px\" height=\"0.002px\" transform=\"scale(0) "
                                    "matrix(1 0 0 -1 0 0)\"><div/></foreignObject>") !=
               boundary_output.npos);
    };

    "SVG fixed numbers use ECMAScript tie rounding"_test = [] {
        scene::Builder builder;
        builder.set_metrics({0.00125, 0.00125, 0});
        builder.set_font_metrics({1, 0.5, 0.5, 0.7});
        auto output =
            svg::write(std::move(builder).freeze(), {.font_cache = svg::FontCacheMode::none});
        expect(output.find("viewBox=\"0 -1.3 1.3 1.3\"") != output.npos);
    };


};
