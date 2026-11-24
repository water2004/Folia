#include "support/folia_test.hpp"
import folia.math.output.scene.builder;
import folia.math.output.scene.html_snapshot;
import folia.math.adaptors.lite_adaptor;

namespace {
using namespace boost::ut;
using namespace folia::math::output::scene;

suite native_scene_html = [] {
    "foreign HTML is owned by one structured group"_test = [] {
        NodeId group{};
        Scene scene = [&] {
            Builder builder;
            SemanticMetadata metadata;
            metadata.source_tag = "annotation-xml";
            metadata.annotation_encoding = "text/html";
            metadata.annotation = "<b>owned annotation</b>";
            ForeignContent foreign{
                HtmlNodePrimitive{
                    u"span",
                    u"http://www.w3.org/1999/xhtml",
                    {{u"class", u"result"}},
                    {},
                    {HtmlNodePrimitive{
                        u"#text", u"http://www.w3.org/1999/xhtml", {}, u"result", {}}}},
                ForeignObjectGeometry{0, -12, 24, 14, 1.0 / 16.0}, std::move(metadata)};
            group = builder.add_group(0, {}, {}, {}, {}, {}, ElementRole::foreign_content_viewport,
                                      {}, {}, std::move(foreign));
            expect(builder.has_foreign_content(group));
            expect(!builder.has_foreign_content(0));
            bool missing_rejected = false;
            try {
                (void)builder.has_foreign_content(999);
            } catch (std::out_of_range const &) {
                missing_rejected = true;
            }
            expect(missing_rejected);
            expect(builder.entry_count(group) == 0_u);
            bool rejected_entry = false;
            try {
                builder.add_primitive(group, TextRun{"ordinary", {}, {}, {}, {}});
            } catch (std::invalid_argument const &) {
                rejected_entry = true;
            }
            expect(rejected_entry);
            bool rejected_child_group = false;
            try {
                (void)builder.add_group(group);
            } catch (std::invalid_argument const &) {
                rejected_child_group = true;
            }
            expect(rejected_child_group);
            bool rejected_wrap = false;
            try {
                (void)builder.wrap_contents(group);
            } catch (std::invalid_argument const &) {
                rejected_wrap = true;
            }
            expect(rejected_wrap);
            return std::move(builder).freeze();
        }();
        auto const &saved_group =
            *std::get<std::shared_ptr<Group const>>(scene.root().entries.front());
        expect(saved_group.id == group);
        expect(saved_group.foreign_content.has_value());
        auto const &saved = *saved_group.foreign_content;
        expect(saved.content.kind == u"span");
        expect(saved.content.attributes.front().second == u"result");
        expect(saved.content.children.front().text == u"result");
        expect(saved.geometry.x == 0.0_d);
        expect(saved.geometry.y == -12.0_d);
        expect(saved.geometry.width == 24.0_d);
        expect(saved.geometry.height == 14.0_d);
        expect(saved.geometry.em_per_pixel == (1.0 / 16.0));
        expect(saved.metadata.annotation_encoding == "text/html");
    };

    "HTML DOM snapshot owns ordered attributes text comments and namespace hints"_test = [] {
        folia::math::adaptors::lite::LiteBase adaptor;
        auto text = adaptor.text(u"text < &");
        auto comment = adaptor.comment(u"<!--kept-->");
        std::array<folia::math::DOMNodeRef, 2> html_children{text, comment};
        std::array<folia::math::AttributeData, 2> attributes{
            folia::math::AttributeData{u"first", u"<&"},
            folia::math::AttributeData{u"second", u"2"}};
        auto html = adaptor.node(u"span", attributes, html_children);
        auto foreign_object =
            adaptor.node(u"foreignObject", {}, std::span<folia::math::DOMNodeRef const>{&html, 1});
        auto svg =
            adaptor.node(u"svg", {}, std::span<folia::math::DOMNodeRef const>{&foreign_object, 1});

        auto snapshot = snapshotHtmlNode(adaptor, svg, u"source-ns");
        expect(snapshot.kind == u"svg");
        expect(snapshot.namespaceHint == u"http://www.w3.org/2000/svg");
        expect(snapshot.children.size() == 1_u);
        auto const &saved_foreign = snapshot.children.front();
        expect(saved_foreign.namespaceHint == u"http://www.w3.org/1999/xhtml");
        expect(saved_foreign.children.size() == 1_u);
        auto const &saved_html = saved_foreign.children.front();
        expect(saved_html.namespaceHint == u"http://www.w3.org/1999/xhtml");
        expect(saved_html.attributes.size() == 2_u);
        expect(saved_html.attributes[0] ==
               (std::pair<std::u16string, std::u16string>{u"first", u"<&"}));
        expect(saved_html.attributes[1].first == u"second");
        expect(saved_html.children.size() == 2_u);
        expect(saved_html.children[0].kind == u"#text");
        expect(saved_html.children[0].text == u"text < &");
        expect(saved_html.children[1].kind == u"#comment");
        expect(saved_html.children[1].text == u"<!--kept-->");
        html.reset();
        foreign_object.reset();
        svg.reset();
        text.reset();
        comment.reset();
        expect(snapshot.children.front().children.front().children.front().text == u"text < &");
    };

    "foreignObject geometry and exclusive group shape are validated"_test = [] {
        auto rejects = [](ForeignObjectGeometry geometry, std::optional<Link> link = {},
                          std::optional<Tooltip> tooltip = {}, std::optional<Clip> clip = {},
                          std::optional<Viewport> viewport = {}) {
            Builder builder;
            ForeignContent foreign{HtmlNodePrimitive{u"div"}, geometry, {}};
            try {
                (void)builder.add_group(0, {}, std::move(clip), std::move(link), std::move(tooltip),
                                        {}, ElementRole::foreign_content_viewport, {},
                                        std::move(viewport), std::move(foreign));
            } catch (std::invalid_argument const &) {
                return true;
            }
            return false;
        };
        expect(rejects(ForeignObjectGeometry{0, 0, -1, 2, 0.5}));
        expect(rejects(ForeignObjectGeometry{0, 0, 1, 2, 0}));
        expect(
            rejects(ForeignObjectGeometry{0, 0, std::numeric_limits<double>::quiet_NaN(), 2, 0.5}));
        expect(
            rejects(ForeignObjectGeometry{0, 0, 1, 2, 0.5}, Link{"https://example.test", {}, {}}));
        expect(rejects(ForeignObjectGeometry{0, 0, 1, 2, 0.5}, {}, Tooltip{"tip"}));
        expect(rejects(ForeignObjectGeometry{0, 0, 1, 2, 0.5}, {}, {},
                       Clip{PathGeometry{{MoveTo{{0, 0}}}}}));
        expect(rejects(ForeignObjectGeometry{0, 0, 1, 2, 0.5}, {}, {}, {},
                       Viewport{{0, 0, 1, 1}, {0, 0, 1, 1}, {}}));

        Group root;
        auto foreign_group = std::make_shared<Group>();
        foreign_group->id = 1;
        foreign_group->foreign_content =
            ForeignContent{HtmlNodePrimitive{u"div"}, ForeignObjectGeometry{0, 0, 2, 2, 0.5}, {}};
        foreign_group->entries.emplace_back(Primitive{TextRun{"invalid", {}, {}, {}, {}}});
        root.entries.emplace_back(std::shared_ptr<Group const>{foreign_group});
        bool rejected_entries = false;
        try {
            Scene invalid{root, {}, {}};
        } catch (std::invalid_argument const &) {
            rejected_entries = true;
        }
        expect(rejected_entries);

        Builder split_builder;
        auto shell = split_builder.add_group(0);
        ForeignContent child_content{
            HtmlNodePrimitive{u"div"}, ForeignObjectGeometry{0, 0, 1, 1, 0.5}, {}};
        auto foreign_id = split_builder.add_group(shell, {}, {}, {}, {}, {},
                                                  ElementRole::foreign_content_viewport, {}, {},
                                                  std::move(child_content));
        std::array<NodeId, 1> sources{foreign_id};
        bool rejected_foreign_source = false;
        try {
            (void)split_builder.split_group_contents(shell, sources);
        } catch (std::invalid_argument const &) {
            rejected_foreign_source = true;
        }
        expect(rejected_foreign_source);
        expect(split_builder.has_foreign_content(foreign_id));
        expect(split_builder.child_groups(shell) == std::vector<NodeId>{foreign_id});
    };


};
} // namespace
