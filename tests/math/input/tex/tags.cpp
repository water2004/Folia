#include "math/support/tex_fixture.hpp"

suite tags = [] {
    "tags keep nested environment labels and explicit no numbering on the request owner"_test = [] {
        Fixture fixture;
        AbstractTags tags(*fixture.options);
        auto &equation = fixture.options->request().equation;
        tags.setLabel(u"outer");
        tags.start("align", true, true);
        expect(tags.label() == u"outer");
        tags.autoTag();
        expect(equation.counter.equation == 1);
        expect(equation.current_tag.tag.value() == u"1");
        tags.autoTag();
        expect(equation.counter.equation == 1);
        tags.start("split", false, false);
        tags.setLabel(u"inner");
        tags.notag();
        expect(equation.current_tag.no_tag);
        tags.end();
        expect(tags.env() == "align");
        expect(tags.label() == u"outer");
        expect(equation.history.size() == 1_u);
        expect(equation.history[0].label_id == u"inner");
        tags.clearTag();
        expect(!equation.current_tag.tag);
        expect(!equation.current_tag.no_tag);
        expect(tags.label() == u"outer");
        tags.end();
        expect(equation.history.size() == 2_u);
        expect(tags.label() == u"outer");
        equation.redo = true;
        equation.ref_update = true;
        tags.resetTag();
        expect(equation.history.empty());
        expect(!equation.redo);
        expect(!equation.ref_update);
    };
    "tags commit equation counters labels and recompile state using real MathItem"_test = [] {
        Fixture fixture;
        AbstractTags tags(*fixture.options);
        auto &equation = fixture.options->request().equation;
        folia::math::core::AbstractMathItem math(u"x", nullptr);
        tags.reset(7);
        tags.startEquation(math);
        tags.autoTag();
        equation.labels.emplace(u"label", LabelRecord{u"8", u"mjx-eqn:label"});
        equation.ids.emplace(u"mjx-eqn:label");
        equation.redo = true;
        tags.finishEquation(math);
        expect(fixture.session.counters().equation == 8);
        expect(fixture.session.containsId(u"mjx-eqn:label"));
        expect(fixture.session.findLabel(u"label")->tag == u"8");
        expect(math.inputData.recompile.has_value());
        expect(math.inputData.recompile->counter == 7);
        tags.startEquation(math);
        expect(equation.ref_update);
        expect(equation.counter.equation == 7);
        tags.autoTag();
        tags.finishEquation(math);
        expect(fixture.session.counters().equation == 8);
        tags.reset(2);
        expect(fixture.session.labels().empty());
        expect(fixture.session.ids().empty());
        expect(equation.counter.equation == 2);
    };
    "tags format references and URLs and construct the original labeled table shape"_test = [] {
        Fixture fixture;
        AbstractTags tags(*fixture.options);
        auto format = std::get<std::vector<std::u16string>>(tags.formatRef(u"A"));
        expect(std::ranges::equal(format, std::array{u"(", u"A", u")"}));
        expect(tags.formatUrl(u"α #😀", u"page") == u"page#%CE%B1%20%23%F0%9F%98%80");
        expect(throws<std::invalid_argument>(
            [&] { (void)tags.formatUrl(std::u16string(1, 0xD800), u""); }));
        auto &math = fixture.token();
        auto &tag = fixture.node(NodeKind::mtd);
        auto &table = tags.enTag(math, tag);
        expect(table.kind() == NodeKind::mtable);
        expect(table.children()[0]->kind() == NodeKind::mlabeledtr);
        auto row = table.children()[0];
        expect(row->children()[0] == &tag);
        expect(row->children()[1]->children()[0]->children()[0] == &math);
        expect(attr(table, u"side") == u"right");
        expect(attr(table, u"minlabelspacing") == u"0.8em");
        expect(fixture.options->request().nodeLists[u"mtable"].back() == &table);
    };
    "tags factory resolves configured creators builtins and explicit unknown defaults"_test = [] {
        Fixture fixture;
        auto &configuration = fixture.configuration;
        TagsFactory::add(configuration,
                         {u"local", [](ParseOptions &options) -> std::unique_ptr<Tags> {
                              return std::make_unique<NoTags>(options);
                          }});
        TagsFactory::setDefault(configuration, u"local");
        auto tags = TagsFactory::create(*fixture.options, u"missing", configuration.defaultTags,
                                        configuration.tags);
        expect(dynamic_cast<NoTags *>(tags.get()) != nullptr);
        auto all = TagsFactory::create(*fixture.options, u"all", configuration.defaultTags,
                                       configuration.tags);
        expect(dynamic_cast<AllTags *>(all.get()) != nullptr);
        expect(throws<std::runtime_error>(
            [&] { (void)TagsFactory::create(*fixture.options, u"missing", u"also-missing", {}); }));
        tags->start("align", true, true);
        tags->autoTag();
        expect(!fixture.options->request().equation.current_tag.tag);
        expect(tags->getTag(true) == nullptr);
    };
};
