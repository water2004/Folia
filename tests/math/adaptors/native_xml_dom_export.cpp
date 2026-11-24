#include "support/folia_test.hpp"
import folia.math.adaptors.lite_adaptor;
import folia.math.core.mml_tree.mml_document;
import folia.math.core.mml_tree.mml_nodes.html_node;
import folia.math.core.mml_tree.xml_dom_adaptor;

using namespace boost::ut;
using namespace folia::math;
using namespace folia::math::adaptors::lite;
using namespace folia::math::mml;

suite native_xml_dom_export = [] {
    "XMLNode exports an independent owned DOM clone after its adaptor is destroyed"_test = [] {
        LiteBase source;
        DOMAttributes spanAttributes{{u"class", u"emphasis"}, {u"data-kind", u"typed"}};
        std::vector<DOMNodeRef> spanChildren{source.text(u"hello<&")};
        auto span = source.node(u"span", spanAttributes, spanChildren);
        auto comment = source.comment(u"retained comment");
        DOMAttributes rootAttributes{{u"data-id", u"source"}};
        std::vector<DOMNodeRef> rootChildren{comment, span};
        auto root = source.node(u"article", rootAttributes, rootChildren);

        DOMNodeRef exported;
        XmlRef frozen;
        std::weak_ptr<XmlAdaptor const> adaptorLifetime;
        std::weak_ptr<XmlObject const> payloadLifetime;
        {
            MmlDocument document;
            auto adaptor = XmlDomAdaptor::create();
            auto &xml = dynamic_cast<XMLNode &>(document.make(NodeKind::xml));
            frozen = adaptor->fromDOM(root);
            payloadLifetime = frozen;
            xml.setXML(frozen, adaptor);
            auto owner = xml.xmlAdaptor();
            adaptorLifetime = owner;
            expect(owner == adaptor);
            auto htmlAdaptor = std::dynamic_pointer_cast<HtmlAdaptor const>(owner);
            expect(htmlAdaptor != nullptr);
            if (!htmlAdaptor)
                return;
            exported = htmlAdaptor->cloneDOM(xml.getXML());
        }

        expect(adaptorLifetime.expired());
        expect(!payloadLifetime.expired());
        expect(exported != nullptr);
        if (!exported)
            return;

        // Mutating the original input after export cannot affect either retained value.
        source.setAttribute(root, u"data-id", u"source-mutated");
        source.setAttribute(span, u"class", u"source-mutated");
        source.append(span, source.text(u" source-tail"));

        LiteBase exportedAdaptor;
        expect(exportedAdaptor.kind(exported) == u"article");
        expect(exportedAdaptor.getAttribute(exported, u"data-id") == u"source");
        auto exportedChildren = exportedAdaptor.childNodes(exported);
        expect(exportedChildren.size() == 2_u);
        if (exportedChildren.size() != 2)
            return;
        expect(exportedAdaptor.kind(exportedChildren[0]) == u"#comment");
        expect(exportedAdaptor.value(exportedChildren[0]) == u"retained comment");
        expect(exportedAdaptor.kind(exportedChildren[1]) == u"span");
        expect(exportedAdaptor.getAttribute(exportedChildren[1], u"class") == u"emphasis");
        expect(exportedAdaptor.getAttribute(exportedChildren[1], u"data-kind") == u"typed");
        expect(exportedAdaptor.textContent(exported) == u"hello<&");

        // Mutating the returned DOM clone cannot change the MML-owned frozen payload.
        exportedAdaptor.setAttribute(exported, u"data-id", u"clone-mutated");
        exportedAdaptor.setAttribute(exportedChildren[1], u"class", u"clone-mutated");
        exportedAdaptor.append(exportedChildren[1], exportedAdaptor.text(u" clone-tail"));
        expect(exportedAdaptor.getAttribute(exported, u"data-id") == u"clone-mutated");
        expect(exportedAdaptor.getAttribute(exportedChildren[1], u"class") == u"clone-mutated");
        expect(exportedAdaptor.textContent(exported) == u"hello<& clone-tail");
        auto payloadReader = XmlDomAdaptor::create();
        expect(payloadReader->getAttribute(frozen, "data-id") == u"source");
        expect(payloadReader->textContent(frozen) == u"hello<&");
        auto frozenChildren = payloadReader->childNodes(frozen);
        expect(frozenChildren.size() == 2_u);
        if (frozenChildren.size() != 2)
            return;
        expect(payloadReader->kind(frozenChildren[0]) == "#comment");
        expect(payloadReader->value(frozenChildren[0]) == u"retained comment");
        expect(payloadReader->kind(frozenChildren[1]) == "span");
        expect(payloadReader->getAttribute(frozenChildren[1], "class") == u"emphasis");
        expect(payloadReader->getAttribute(frozenChildren[1], "data-kind") == u"typed");
    };
};
