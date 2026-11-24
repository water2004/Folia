#include "math/support/lite_test.hpp"

suite lite_source_queries = [] {
    "Lite query APIs match pinned Source selector and search results"_test = [] {
        const auto &row = fixture(u"queries/simple-selector-contract");
        lite::LiteAdaptor adaptor;
        const auto document = adaptor.parse(row.html);
        const DOMNodeRef root = adaptor.body(document);
        const auto selectors = fixtureSelectors();
        std::string actual = "{\"selectors\":[";
        for (std::size_t i = 0; i < selectors.size(); ++i) {
            if (i)
                actual += ',';
            actual += '[' + quote(selectors[i]) + ',' +
                      pathJson(adaptor, adaptor.getElement(selectors[i], root), root) + ']';
        }
        const auto allP = adaptor.tags(root, u"p");
        const auto noNamespaceP = adaptor.tags(root, u"p", u"urn:no-namespaces");
        const auto idResult = adaptor.elementById(root, u"two");
        const auto classResult = adaptor.elementsByClass(root, u"b");
        const auto attrResult = adaptor.elementsByAttribute(root, u"data-v", u"x");
        const std::vector<DOMNodeRef> classNodes(classResult.begin(), classResult.end());
        const std::vector<DOMNodeRef> attrNodes(attrResult.begin(), attrResult.end());
        actual += "],\"allP\":" + pathsJson(adaptor, allP, root) +
                  ",\"namespacedSearch\":" + pathsJson(adaptor, noNamespaceP, root) +
                  ",\"idSearch\":" + pathJson(adaptor, idResult, root) +
                  ",\"classSearch\":" + pathsJson(adaptor, classNodes, root) +
                  ",\"attributeSearch\":" + pathsJson(adaptor, attrNodes, root) + '}';
        expect(sourceMatch(actual, row.expectedObservation, row.id, row.html));
    };
};

suite lite_source_styles_and_serialization = [] {
    "Lite style edits retain ordered properties and Source serialization"_test = [] {
        const auto &row = fixture(u"style/get-set-and-order");
        lite::LiteAdaptor adaptor;
        const auto document = adaptor.parse(row.html);
        const auto styled = adaptor.getElement(u"#styled", adaptor.body(document));
        const auto before = adaptor.allStyles(styled);
        const auto color = adaptor.getStyle(styled, u"color");
        const auto margin = adaptor.getStyle(styled, u"margin-left");
        adaptor.setStyle(styled, u"color", u"blue");
        adaptor.setStyle(styled, u"font-weight", u"bold");
        const auto after = adaptor.allStyles(styled);
        const auto afterColor = adaptor.getStyle(styled, u"color");
        const auto weight = adaptor.getStyle(styled, u"font-weight");
        std::string attributes = "[";
        const auto all = adaptor.allAttributes(styled);
        for (std::size_t i = 0; i < all.size(); ++i) {
            if (i)
                attributes += ',';
            attributes += '[' + quote(all[i].name) + ',' + quote(all[i].value) + ']';
        }
        attributes += ']';
        const auto actual =
            "{\"before\":{\"raw\":" + quote(*before) + ",\"color\":" + quote(color) +
            ",\"marginLeft\":" + quote(margin) + "},\"after\":{\"raw\":" + quote(*after) +
            ",\"color\":" + quote(afterColor) + ",\"fontWeight\":" + quote(weight) +
            ",\"attributes\":" + attributes + "},\"html\":" + quote(adaptor.outerHTML(styled)) +
            ",\"xml\":" + quote(adaptor.serializeXML(styled)) + '}';
        expect(sourceMatch(actual, row.expectedObservation, row.id, row.html));
    };
    "Lite XML and HTML serializers and protection match Source"_test = [] {
        const auto &row = fixture(u"serialization/protection/xml-namespaces");
        lite::LiteAdaptor adaptor;
        const auto document = adaptor.parse(row.html);
        const auto math = adaptor.getElement(u"math", adaptor.body(document));
        const auto mi = adaptor.getElement(u"mi", adaptor.body(document));
        const auto miElement = std::dynamic_pointer_cast<lite::LiteElement>(mi);
        const auto protect = [&](bool attribute, bool xml) {
            std::string out = "[";
            for (const auto value : {std::u16string_view(u"&<>"), std::u16string_view(u"\"'")}) {
                if (out.size() != 1)
                    out += ',';
                const auto transformed = attribute ? adaptor.parser()->protectAttribute(value, xml)
                                                   : adaptor.parser()->protectHTML(value);
                out += quote(transformed);
            }
            return out + ']';
        };
        const auto actual = "{\"html\":" + quote(adaptor.outerHTML(math)) +
                            ",\"xml\":" + quote(adaptor.serializeXML(math)) +
                            ",\"innerHTML\":" + quote(adaptor.innerHTML(mi)) + ",\"xmlInner\":" +
                            quote(adaptor.parser()->serializeInner(adaptor, miElement, true)) +
                            ",\"protectHTML\":" + protect(false, false) +
                            ",\"protectAttributeHtml\":" + protect(true, false) +
                            ",\"protectAttributeXml\":" + protect(true, true) + '}';
        expect(sourceMatch(actual, row.expectedObservation, row.id, row.html));
    };
};

