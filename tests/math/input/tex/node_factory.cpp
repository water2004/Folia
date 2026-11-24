#include "support/folia_test.hpp"
import folia.math.input.tex.node_factory;

using namespace boost::ut;
using namespace folia::math::tex;
using namespace folia::math::mml;

namespace {
struct Host final : NodeFactoryConfiguration {
    TexEnvironment env;
    std::vector<std::string> events;
    TexEnvironment const &environment() const override {
        return env;
    }
    void addNode(NodeKind kind, MmlNode &) override {
        events.push_back("save:" + std::string(nodeKindName(kind)));
    }
    void addNode(std::u16string_view, MmlNode &) override {}
};
Host &host(NodeFactory &factory) {
    return static_cast<Host &>(factory.configuration());
}
MmlNode &node(NodeFactory &f, NodeKind kind, std::span<MmlNode *const> children,
              NodeDefinition definition, MmlNode *text) {
    host(f).events.push_back("node:" + std::string(nodeKindName(kind)));
    return NodeFactory::createNode(f, kind, children, std::move(definition), text);
}
MmlNode &token(NodeFactory &f, NodeKind kind, NodeDefinition definition,
               std::optional<std::u16string_view> text) {
    host(f).events.push_back("token");
    return NodeFactory::createToken(f, kind, std::move(definition), text);
}
MmlNode *text(NodeFactory &f, std::optional<std::u16string_view> value) {
    host(f).events.push_back("text");
    return NodeFactory::createText(f, value);
}
MmlNode &error(NodeFactory &f, std::u16string_view message) {
    host(f).events.push_back("error");
    return NodeFactory::createError(f, message);
}
MmlNode &throwAfterNode(NodeFactory &f, NodeKind kind, std::span<MmlNode *const> children,
                        NodeDefinition definition, MmlNode *text) {
    (void)node(f, kind, children, std::move(definition), text);
    throw std::runtime_error("creator failure");
}
MmlNode &changedNode(NodeFactory &f, NodeKind kind, std::span<MmlNode *const> children,
                     NodeDefinition definition, MmlNode *text) {
    host(f).events.push_back("changed-node");
    return NodeFactory::createNode(f, kind, children, std::move(definition), text);
}
MmlNode *changeNestedCreator(NodeFactory &f, std::optional<std::u16string_view> value) {
    host(f).events.push_back("change-text");
    f.setCreators(NodeCreators{.node = changedNode});
    return NodeFactory::createText(f, value);
}
} // namespace

suite node_factory = [] {
    "node factory token preserves absent text separately from empty text"_test = [] {
        MmlDocument document;
        NodeFactory factory(document.factory());
        auto &absent = factory.makeToken(NodeKind::mo, std::nullopt);
        auto &empty = factory.makeToken(NodeKind::mo, std::u16string_view{});
        expect(absent.children().empty());
        expect(empty.children().size() == 1_u);
        expect(empty.children().front()->kind() == NodeKind::text);
        expect(empty.children().front()->text().empty());
    };
    "node factory token and error defaults consume configured nested creators"_test = [] {
        MmlDocument document;
        Host context;
        NodeFactory factory(document.factory());
        factory.setConfiguration(context);
        factory.setCreators(
            NodeCreators{.node = node, .token = token, .text = text, .error = error});
        auto &result = factory.makeToken(NodeKind::mi, u"x");
        expect(context.events == std::vector<std::string>{"token", "text", "node:mi", "save:mi"})
            << "token creator order";
        expect(result.children().front()->text() == u"x");
        expect(result.children().front()->parent() == &result);
        context.events.clear();
        auto &failure = factory.makeError(u"bad");
        expect(context.events == std::vector<std::string>{"error", "text", "node:mtext",
                                                          "save:mtext", "node:merror",
                                                          "save:merror"})
            << "error creator order";
        auto *inferred = failure.children().front();
        expect(inferred->kind() == NodeKind::inferred_mrow);
        expect(inferred->children().front()->kind() == NodeKind::mtext);
        expect(inferred->children().front()->children().front()->text() == u"bad");
        expect(std::get<std::u16string>(*failure.attributes().own(u"data-mjx-error")) == u"bad");
    };

    "node factory changes to a creator are visible to subsequent nested calls"_test = [] {
        MmlDocument document;
        Host context;
        NodeFactory factory(document.factory());
        factory.setConfiguration(context);
        factory.setCreators(
            NodeCreators{.node = node, .token = token, .text = changeNestedCreator});
        auto &result = factory.makeToken(NodeKind::mi, u"x");
        expect(context.events ==
               std::vector<std::string>{"token", "change-text", "changed-node", "save:mi"});
        expect(result.kind() == NodeKind::mi);
    };

    "node factory does not register a node when its configured creator throws"_test = [] {
        MmlDocument document;
        Host context;
        NodeFactory factory(document.factory());
        factory.setConfiguration(context);
        factory.setCreators(NodeCreators{.node = throwAfterNode, .text = text});
        expect(throws<std::runtime_error>([&] { factory.makeToken(NodeKind::mi, u"x"); }));
        expect(context.events == std::vector<std::string>{"text", "node:mi"});
        // The completed node belongs to the document even if registration did not run.
    };

    "node factory definition separates native properties from XML attributes"_test = [] {
        MmlDocument document;
        NodeFactory factory(document.factory());
        NodeDefinition definition;
        definition.attributes.set(u"mathcolor", u"red");
        definition.properties.tex.explicit_tex_class = TexClass::op;
        definition.properties.tex.movable_limits = false;
        definition.properties.tex.function_operator = true;
        auto &result = factory.makeNode(NodeKind::mo, {}, definition);
        expect(result.texClass().value() == TexClass::op);
        expect(result.properties().tex.explicit_tex_class.value() == TexClass::op);
        expect(!result.properties().tex.movable_limits.value());
        expect(!std::get<bool>(*result.attributes().own(u"movablelimits")));
        expect(result.properties().tex.function_operator.value());
        expect(!result.attributes().own(u"fnOP"));
        MmlDocument destination;
        auto &copy = destination.cloneSubtree(result);
        expect(copy.properties().tex.function_operator.value());
        expect(copy.texClass().value() == TexClass::op);
        auto &identifier = factory.makeNode(NodeKind::mi, {}, definition);
        expect(!identifier.attributes().own(u"movablelimits"));
        expect(!identifier.properties().tex.movable_limits.value());
        auto *content = factory.makeText(u"x");
        auto &token = factory.makeNode(NodeKind::mi, {}, {}, content);
        expect(content->parent() == &token);
        expect(token.children().size() == 1_u);
    };

    "node factory text distinguishes null from an empty text value and supports factory rebinding"_test =
        [] {
            MmlDocument first, second;
            NodeFactory factory(first.factory());
            expect(factory.makeText(std::nullopt) == nullptr);
            auto *empty = factory.makeText(u"");
            expect(empty != nullptr && empty->text().empty());
            expect(&empty->document() == &first);
            factory.setMmlFactory(second.factory());
            auto &result = factory.makeToken(NodeKind::mi);
            expect(&result.document() == &second);
            expect(result.children().front()->text().empty());
            expect(&empty->document() == &first);
            expect(throws<std::invalid_argument>([&] {
                factory.setCreators(NodeCreators{.token = static_cast<TokenCreator>(nullptr)});
            }));
        };
};
