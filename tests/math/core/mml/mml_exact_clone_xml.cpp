#include "support/folia_test.hpp"
import folia.math.core.mml_tree.mml_document;
import folia.math.core.mml_tree.mml_nodes.html_node;

using namespace boost::ut;
using namespace folia::math::mml;

namespace {
struct Markup final : XmlObject {
    explicit Markup(std::string value) : source(std::move(value)) {
    }

    std::string source;
};

class TestAdaptor final : public HtmlAdaptor {
  public:
    mutable std::size_t cloneCalls{};

    folia::math::DOMNodeRef cloneDOM(XmlRef) const override {
        throw std::logic_error("DOM export unavailable in opaque test adaptor");
    }

    XmlRef clone(XmlRef value) const override {
        ++cloneCalls;
        return std::make_shared<Markup>(read(value));
    }

    std::string serializeXML(XmlRef value) const override {
        return value ? read(value) : "<null/>";
    }

    std::optional<std::u16string> getAttribute(XmlRef, std::string_view) const override {
        return u"present";
    }

    XmlRef node(std::string_view tag, std::vector<std::pair<std::string, std::u16string>>,
                std::vector<XmlRef>) const override {
        return std::make_shared<Markup>("<" + std::string(tag) + "/>");
    }

    std::string outerHTML(XmlRef value) const override {
        return read(value);
    }

    std::u16string textContent(XmlRef value) const override {
        auto text = read(value);
        return std::u16string(text.begin(), text.end());
    }

    std::string kind(XmlRef) const override {
        return "svg";
    }

  private:
    static std::string read(XmlRef const &value) {
        return dynamic_cast<Markup const &>(*value).source;
    }
};
} // namespace

suite mml_exact_clone_xml = [] {
    "XML exact clone retains immutable payload and adaptor after source destruction"_test = [] {
        MmlDocument destination;
        XMLNode *copy = nullptr;
        std::weak_ptr<XmlObject const> payloadLifetime;
        std::weak_ptr<XmlAdaptor const> adaptorLifetime;
        {
            MmlDocument source;
            auto &node = dynamic_cast<XMLNode &>(source.factory().make(NodeKind::xml));
            auto payload = std::make_shared<Markup>("<math><mi>x</mi></math>");
            auto adaptor = std::make_shared<TestAdaptor>();
            payloadLifetime = payload;
            adaptorLifetime = adaptor;
            node.setXML(payload, adaptor);
            expect(throws<std::logic_error>([&] { (void)adaptor->cloneDOM(payload); }));
            copy = &dynamic_cast<XMLNode &>(destination.cloneSubtree(node));
            expect(copy->getXML() == node.getXML());
            expect(copy->xmlAdaptor() == adaptor);
            expect(copy->getSerializedXML() == "<math><mi>x</mi></math>");
            expect(adaptor->cloneCalls == 0_u);
        }

        expect(!payloadLifetime.expired());
        expect(!adaptorLifetime.expired());
        expect(copy != nullptr);
        expect(copy->getSerializedXML() == "<math><mi>x</mi></math>");
        expect(copy->xmlAdaptor() != nullptr);
        auto retainedAdaptor = std::dynamic_pointer_cast<TestAdaptor const>(adaptorLifetime.lock());
        expect(retainedAdaptor != nullptr);
        if (retainedAdaptor)
            expect(retainedAdaptor->cloneCalls == 0_u);
    };

    "HTML exact clone preserves its concrete subtype and shared HTML view"_test = [] {
        MmlDocument source;
        MmlDocument destination;
        auto adaptor = std::make_shared<TestAdaptor>();
        auto &node = dynamic_cast<HtmlNode &>(source.factory().make(NodeKind::html));
        auto payload = std::make_shared<Markup>("<span>html</span>");
        node.setHTML(payload, adaptor);

        auto &copy = dynamic_cast<HtmlNode &>(destination.cloneSubtree(node));
        expect(copy.kind() == NodeKind::html);
        expect(copy.getHTML() == node.getHTML());
        expect(copy.getSerializedHTML() == "<span>html</span>");
        expect(copy.textContent() == u"<span>html</span>");
        expect(copy.toString() == "HTML=<svg>...</svg>");
        expect(adaptor->cloneCalls == 0_u);
    };

    "XML exact clone preserves null state and replacement remains independent"_test = [] {
        MmlDocument source;
        MmlDocument destination;
        auto &empty = dynamic_cast<XMLNode &>(source.make(NodeKind::xml));
        auto &emptyCopy = dynamic_cast<XMLNode &>(destination.cloneSubtree(empty));
        expect(emptyCopy.getXML() == nullptr);
        expect(throws<std::logic_error>([&] { (void)emptyCopy.getSerializedXML(); }));

        auto adaptor = std::make_shared<TestAdaptor>();
        auto &nullPayload = dynamic_cast<XMLNode &>(source.make(NodeKind::xml));
        nullPayload.setXML({}, adaptor);
        auto &nullPayloadCopy = dynamic_cast<XMLNode &>(destination.cloneSubtree(nullPayload));
        expect(nullPayloadCopy.getXML() == nullptr);
        expect(nullPayloadCopy.getSerializedXML() == "<null/>");

        auto &nullAdaptor = dynamic_cast<XMLNode &>(source.make(NodeKind::xml));
        nullAdaptor.setXML(std::make_shared<Markup>("<no-adaptor/>"));
        auto &nullAdaptorCopy = dynamic_cast<XMLNode &>(destination.cloneSubtree(nullAdaptor));
        expect(nullAdaptorCopy.getXML() != nullptr);
        expect(throws<std::logic_error>([&] { (void)nullAdaptorCopy.getSerializedXML(); }));

        auto first = std::make_shared<Markup>("<one/>");
        auto second = std::make_shared<Markup>("<two/>");
        auto third = std::make_shared<Markup>("<three/>");
        auto &node = dynamic_cast<XMLNode &>(source.make(NodeKind::xml));
        node.setXML(first, adaptor);
        auto &copy = dynamic_cast<XMLNode &>(destination.cloneSubtree(node));
        node.setXML(second, adaptor);
        expect(copy.getXML() == first);
        expect(copy.getSerializedXML() == "<one/>");
        copy.setXML(third, adaptor);
        expect(node.getXML() == second);
        expect(node.getSerializedXML() == "<two/>");
        expect(copy.getSerializedXML() == "<three/>");
    };

    "MathJax XML copy continues cloning payload without carrying the adaptor"_test = [] {
        MmlDocument source;
        auto &node = dynamic_cast<XMLNode &>(source.make(NodeKind::xml));
        auto adaptor = std::make_shared<TestAdaptor>();
        auto payload = std::make_shared<Markup>("<copy/>");
        node.setXML(payload, adaptor);

        auto &copy = dynamic_cast<XMLNode &>(node.copy());
        expect(copy.getXML() != node.getXML());
        expect(adaptor->cloneCalls == 1_u);
        expect(throws<std::logic_error>([&] { (void)copy.getSerializedXML(); }));
    };
    "html_verify_tree_null_and_explicit_options_preserve_token_and_non_token_placements"_test = [] {
        MmlDocument document;
        auto &factory = document.factory();
        MmlNode &token = factory.make(NodeKind::mi);
        MmlNode &tokenHtml = factory.make(NodeKind::html);
        document.append(token, tokenHtml);

        MmlNode &row = factory.make(NodeKind::mrow);
        MmlNode &rowHtml = factory.make(NodeKind::html);
        document.append(row, rowHtml);
        const auto nodeCountBefore = document.size();

        tokenHtml.verifyTree(std::nullopt);
        rowHtml.verifyTree(std::nullopt);
        tokenHtml.verifyTree(VerifyOptions{});
        rowHtml.verifyTree(VerifyOptions{});

        expect(token.isToken());
        expect(tokenHtml.parent() == &token);
        expect(token.children().size() == 1u);
        if (!token.children().empty())
            expect(token.children().front() == &tokenHtml);
        expect(!row.isToken());
        expect(rowHtml.parent() == &row);
        expect(row.children().size() == 1u);
        if (!row.children().empty())
            expect(row.children().front() == &rowHtml);
        expect(document.size() == nodeCountBefore)
            << "HtmlNode's inherited empty-node mError leaves invalid placement unchanged";
    };

};
