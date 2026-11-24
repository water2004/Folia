#include "support/folia_test.hpp"
import folia.math.adaptors.lite_adaptor;
import folia.math.core.mml_tree.mml_document;
import folia.math.core.mml_tree.mml_nodes.html_node;
import folia.math.core.mml_tree.xml_dom_adaptor;

using namespace boost::ut;
using namespace folia::math;
using namespace folia::math::adaptors::lite;
using namespace folia::math::mml;

namespace {
struct ForeignXml final : XmlObject {};
} // namespace

suite xml_dom_adaptor = [] {
    "XML adaptor freezes HTML independently of source mutation and owner lifetime"_test = [] {
        XmlRef frozen;
        std::weak_ptr<DOMNode> originalLifetime;
        std::string initialMarkup;
        DOMAttributes initialAttributes;
        {
            LiteAdaptor source;
            auto document = source.parse(u"<b>old</b>", u"text/html");
            auto original = source.root(document);
            source.setAttribute(original, u"data-k", u"v");
            originalLifetime = original;
            auto adaptor = XmlDomAdaptor::create();
            frozen = adaptor->fromDOM(original);
            initialMarkup = adaptor->outerHTML(frozen);
            initialAttributes = adaptor->allAttributes(frozen);

            source.setAttribute(original, u"data-k", u"changed");
            std::array<DOMNodeRef, 1> addedText{source.text(u"new")};
            source.append(original, source.node(u"i", {}, addedText));
            expect(adaptor->outerHTML(frozen) == initialMarkup)
                << "freeze retains parsed document serialization after source mutation";
            expect(adaptor->allAttributes(frozen).size() == initialAttributes.size())
                << "freeze retains original ordered attribute count";
            expect(adaptor->getAttribute(frozen, "data-k") == u"v")
                << "freeze retains source attribute value";
        }

        expect(originalLifetime.expired()) << "frozen payload does not retain source root";
        auto observer = XmlDomAdaptor::create();
        expect(observer->outerHTML(frozen) == initialMarkup)
            << "frozen payload remains serializable after source adaptor destruction";
        expect(observer->getAttribute(frozen, "data-k") == u"v")
            << "frozen attributes remain queryable after source adaptor destruction";
        expect(observer->textContent(frozen) == u"old")
            << "frozen text remains unchanged after source adaptor destruction";
    };

    "XML adaptor and HtmlNode wrapper clone without reparenting frozen inputs"_test = [] {
        LiteAdaptor source;
        auto sourceDoc = source.parse(u"<i>immutable</i>", u"text/html");
        auto matches = source.tags(source.root(sourceDoc), u"i");
        expect(matches.size() == 1_u) << "parsed document contains the one requested i element";
        if (matches.empty()) return;
        auto sourceNode = matches.front();
        auto adaptor = XmlDomAdaptor::create();
        auto original = adaptor->fromDOM(sourceNode);
        auto independent = adaptor->clone(original);
        expect(adaptor->outerHTML(independent) == adaptor->outerHTML(original))
            << "clone preserves selected parsed element serialization";
        auto rebuilt = adaptor->node("article", {}, {original});
        expect(adaptor->outerHTML(rebuilt) == "<article><i>immutable</i></article>")
            << "node(children) clones a child into the new element";
        expect(adaptor->outerHTML(original) == "<i>immutable</i>")
            << "node(children) leaves the frozen input serialization unchanged";

        MmlDocument mml;
        auto &html = dynamic_cast<HtmlNode &>(mml.make(NodeKind::html));
        html.setHTML(original, adaptor);
        auto wrapped = html.getHTML();
        expect(wrapped == original)
            << "HtmlNode keeps a valid LiteElement when getAttribute returns no marker";
        expect(adaptor->kind(wrapped) == "i")
            << "absent data-mjx-hdw on LiteElement does not trigger Source wrapper path";
        expect(!adaptor->getAttribute(wrapped, "data-mjx-hdw"))
            << "valid LiteElement marker lookup returns absence without throwing";
        expect(adaptor->textContent(wrapped) == u"immutable")
            << "unwrapped parsed element exposes descendant text";
        expect(adaptor->outerHTML(wrapped) == "<i>immutable</i>")
            << "HtmlNode serializes the valid LiteElement without a span";
        expect(adaptor->outerHTML(original) == "<i>immutable</i>")
            << "HtmlNode wrapping does not change original child payload";
        expect(adaptor->outerHTML(independent) == "<i>immutable</i>")
            << "independent clone remains unchanged after wrapping";

        auto wrapperChildren = adaptor->childNodes(wrapped);
        expect(wrapperChildren.size() == 1_u) << "parsed i element has exactly its text child";
        if (!wrapperChildren.empty())
            expect(adaptor->value(wrapperChildren.front()) == u"immutable")
                << "parsed i child wrapper preserves text value";
    };

    "XML adaptor wraps Text and Comment preserving raw comment spelling"_test = [] {
        LiteAdaptor source;
        auto adaptor = XmlDomAdaptor::create();
        auto text = adaptor->fromDOM(source.text(u"raw < text"));
        auto comment = adaptor->fromDOM(source.comment(u"<!--raw comment-->"));
        expect(adaptor->kind(text) == "#text") << "root Text payload keeps LiteText kind";
        expect(adaptor->value(text) == u"raw < text") << "root Text preserves raw value";
        expect(adaptor->kind(comment) == "#comment") << "root Comment keeps LiteComment kind";
        expect(adaptor->value(comment) == u"<!--raw comment-->")
            << "root Comment preserves exact raw spelling";

        MmlDocument mml;
        auto &textHtml = dynamic_cast<HtmlNode &>(mml.make(NodeKind::html));
        textHtml.setHTML(text, adaptor);
        expect(adaptor->outerHTML(textHtml.getHTML()) == "<span>raw &lt; text</span>")
            << "HtmlNode span serializes root Text through Lite HTML escaping";
        expect(textHtml.textContent() == u"raw < text")
            << "HtmlNode textContent returns root Text content";

        auto &commentHtml = dynamic_cast<HtmlNode &>(mml.make(NodeKind::html));
        commentHtml.setHTML(comment, adaptor);
        expect(adaptor->outerHTML(commentHtml.getHTML()) == "<span><!--raw comment--></span>")
            << "HtmlNode span serialization preserves raw Comment markup";
        expect(commentHtml.textContent().empty())
            << "Lite textContent excludes Comment children";
        auto commentChildren = adaptor->childNodes(commentHtml.getHTML());
        expect(commentChildren.size() == 1_u) << "comment span retains one typed child";
        if (!commentChildren.empty()) {
            expect(adaptor->kind(commentChildren.front()) == "#comment")
                << "wrapped child retains Comment kind";
            expect(adaptor->value(commentChildren.front()) == u"<!--raw comment-->")
                << "wrapped child retains raw Comment value";
        }
    };

    "XML adaptor payloads retain only their own frozen subtree lifetime"_test = [] {
        std::weak_ptr<XmlObject const> payloadLifetime;
        std::weak_ptr<XmlObject const> childLifetime;
        std::weak_ptr<XmlAdaptor const> adaptorLifetime;
        std::weak_ptr<DOMNode> sourceLifetime;
        XmlRef payload;
        std::vector<XmlRef> children;
        {
            MmlDocument mml;
            auto adaptor = XmlDomAdaptor::create();
            adaptorLifetime = adaptor;
            LiteAdaptor source;
            auto document = source.parse(u"<div><em>x</em></div>", u"text/html");
            auto roots = source.tags(source.root(document), u"div");
            expect(roots.size() == 1_u) << "parsed html root contains one div element";
            auto root = roots.empty() ? source.root(document) : roots.front();
            sourceLifetime = root;
            payload = adaptor->fromDOM(root);
            payloadLifetime = payload;
            auto &html = dynamic_cast<HtmlNode &>(mml.make(NodeKind::html));
            html.setHTML(payload, adaptor);
            children = adaptor->childNodes(payload);
            if (!children.empty())
                childLifetime = children.front();
            expect(!adaptorLifetime.expired()) << "HtmlNode owns the assigned adaptor while MmlDocument lives";
            adaptor.reset();
            expect(!adaptorLifetime.expired()) << "MmlDocument retains adaptor after local owner reset";
        }
        expect(sourceLifetime.expired()) << "frozen payload and child wrappers release source Lite node";
        expect(adaptorLifetime.expired()) << "adapter owner releases after HtmlNode document destruction";
        expect(!payloadLifetime.expired()) << "caller XmlRef retains parent payload after document destruction";
        expect(!children.empty()) << "parent payload exposes its parsed em child";
        payload.reset();
        expect(payloadLifetime.expired()) << "parent XmlRef payload releases independently of child wrapper";
        expect(!childLifetime.expired()) << "child XmlRef retains its Lite subtree after parent release";
        auto observer = XmlDomAdaptor::create();
        if (!children.empty())
            expect(observer->outerHTML(children.front()) == "<em>x</em>")
                << "surviving child wrapper remains serializable";
        children.clear();
        expect(childLifetime.expired()) << "last child XmlRef release destroys child payload";
    };

    "XML adaptor rejects foreign and wrong-kind queries with standard exceptions"_test = [] {
        auto adaptor = XmlDomAdaptor::create();
        auto foreign = std::make_shared<ForeignXml>();
        auto text = adaptor->fromDOM(LiteAdaptor{}.text(u"x"));
        expect(throws<std::invalid_argument>([&] { (void)adaptor->kind({}); }))
            << "null XmlRef is rejected with invalid_argument";
        expect(throws<std::invalid_argument>([&] { (void)adaptor->outerHTML(text); }))
            << "outerHTML rejects non-element LiteText";
        expect(throws<std::invalid_argument>([&] { (void)adaptor->allAttributes(text); }))
            << "allAttributes rejects non-element LiteText";
        expect(throws<std::invalid_argument>([&] { (void)adaptor->childNodes(text); }))
            << "childNodes rejects non-element LiteText";
        expect(throws<std::invalid_argument>([&] { (void)adaptor->clone(foreign); }))
            << "clone rejects unsupported XmlObject payload type";
        expect(throws<std::invalid_argument>([&] { (void)adaptor->fromDOM({}); }))
            << "fromDOM rejects a null Lite node";
    };

    "XMLNode serializes a frozen Lite payload after retaining its typed adaptor"_test = [] {
        LiteBase source;
        DOMAttributes attributes{{u"data-x", u"a&b"}};
        std::array<DOMNodeRef, 1> children{source.text(u"raw < text")};
        const auto element = source.node(u"span", attributes, children);

        MmlDocument document;
        auto &xml = dynamic_cast<XMLNode &>(document.make(NodeKind::xml));
        std::weak_ptr<XmlAdaptor const> ownerLease;
        {
            auto owner = XmlDomAdaptor::create();
            ownerLease = owner;
            xml.setXML(owner->fromDOM(element), owner);
        }

        expect(!ownerLease.expired()) << "XMLNode retains its real XmlDomAdaptor";
        expect(xml.getSerializedXML() == "<span data-x=\"a&amp;b\">raw &lt; text</span>")
            << "XMLNode serializes the immutable typed Lite payload through its retained owner";
        xml.setXML({}, {});
        expect(ownerLease.expired()) << "clearing XML state releases the retained adaptor";
    };
};
