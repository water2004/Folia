#include "support/folia_test.hpp"
import folia.math.core.mml_tree.mml_document;
import folia.math.core.mml_tree.mml_nodes.html_node;
import folia.math.input.tex.node_util;
using namespace boost::ut;
using namespace folia::math::mml;

namespace {
struct Xml final : XmlObject {
    std::string payload;
};

class Adaptor final : public HtmlAdaptor {
  public:
    folia::math::DOMNodeRef cloneDOM(XmlRef) const override {
        throw std::logic_error("DOM export unavailable in opaque test adaptor");
    }

    XmlRef clone(XmlRef value) const override {
        auto x = std::make_shared<Xml>();
        x->payload = as(value).payload;
        return x;
    }

    std::string serializeXML(XmlRef value) const override {
        return as(value).payload;
    }

    std::optional<std::u16string> getAttribute(XmlRef, std::string_view key) const override {
        if (key == "data-mjx-hdw")
            throw std::runtime_error("missing");
        return std::nullopt;
    }

    XmlRef node(std::string_view tag, std::vector<std::pair<std::string, std::u16string>>,
                std::vector<XmlRef> children) const override {
        auto x = std::make_shared<Xml>();
        x->payload = "<" + std::string(tag) + ">" + as(children.at(0)).payload + "</" +
                     std::string(tag) + ">";
        return x;
    }

    std::string outerHTML(XmlRef value) const override {
        return as(value).payload;
    }

    std::u16string textContent(XmlRef) const override {
        return u"html text";
    }

    std::string kind(XmlRef) const override {
        return "span";
    }

  private:
    static Xml const &as(XmlRef const &value) {
        return dynamic_cast<Xml const &>(*value);
    }
};
} // namespace

suite typed_mml_branches = [] {
    "token text uses typed text and HTML children, while walk skips contents"_test = [] {
        MmlDocument doc;
        auto &token = dynamic_cast<AbstractMmlTokenNode &>(doc.make(NodeKind::mi));
        auto &text = dynamic_cast<TextNode &>(doc.make(NodeKind::text));
        text.setText(u"A\U0001F600");
        token.appendChild(text);
        auto &html = dynamic_cast<HtmlNode &>(doc.make(NodeKind::html));
        auto value = std::make_shared<Xml>();
        value->payload = "<i/>";
        html.setHTML(value, std::make_shared<Adaptor>());
        token.appendChild(html);
        auto defaults = AbstractMmlTokenNode::defaults();
        expect(std::get<std::u16string>(*defaults.defaultValue(u"mathvariant")) == u"normal");
        auto layoutDefaults = AbstractMmlLayoutNode::defaults();
        expect(layoutDefaults.defaultValue(u"mathvariant") == nullptr);
        auto baseDefaults = AbstractMmlBaseNode::defaults();
        expect(baseDefaults.defaultValue(u"mathvariant") == nullptr);
        std::vector<MmlNode *> walked;
        token.walkTree([&](MmlNode &n) { walked.push_back(&n); });
        expect(token.getText() == u"A\U0001F600html text");
        expect(folia::math::tex::NodeUtil::getText(token) == u"A\U0001F600html text");
        expect(folia::math::tex::NodeUtil::getText(text) == u"A\U0001F600");
        expect(!token.isEmpty());
        expect(walked.size() == 1_u);
        auto &emptyToken = dynamic_cast<AbstractMmlTokenNode &>(doc.make(NodeKind::mi));
        auto &emptyText = dynamic_cast<TextNode &>(doc.make(NodeKind::text));
        emptyToken.appendChild(emptyText);
        expect(emptyToken.isEmpty());
        auto &textCopy = dynamic_cast<TextNode &>(text.copy());
        expect(textCopy.getText() == u"A\U0001F600");
        expect(text.toString() == std::string("A\xF0\x9F\x98\x80"));
    };
    "XML Source copy clones immutable data without copying adaptor"_test = [] {
        MmlDocument doc;
        auto adaptor = std::make_shared<Adaptor>();
        auto &node = dynamic_cast<XMLNode &>(doc.make(NodeKind::xml));
        auto value = std::make_shared<Xml>();
        value->payload = "<p/>";
        node.setXML(value, adaptor);
        auto &copy = dynamic_cast<XMLNode &>(node.copy());
        expect(copy.getXML() != node.getXML());
        expect(throws<std::logic_error>([&] { (void)copy.getSerializedXML(); }));
    };
    "HtmlNode wraps source HTML when the marker lookup throws"_test = [] {
        MmlDocument doc;
        auto &node = dynamic_cast<HtmlNode &>(doc.make(NodeKind::html));
        auto value = std::make_shared<Xml>();
        value->payload = "<b/>";
        auto adaptor = std::make_shared<Adaptor>();
        expect(throws<std::logic_error>([&] { (void)adaptor->cloneDOM(value); }));
        node.setHTML(value, adaptor);
        expect(node.getSerializedHTML() == "<span><b/></span>");
        expect(node.textContent() == u"html text");
        expect(node.toString() == "HTML=<span>...</span>");
        auto &token = dynamic_cast<AbstractMmlTokenNode &>(doc.make(NodeKind::mi));
        token.appendChild(node);
        node.verifyTree(VerifyOptions{});
        expect(node.parent() == &token);
    };
};
