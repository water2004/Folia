#include "support/folia_test.hpp"
import folia.math.adaptors.lite_adaptor;
import folia.math.util.utf16;

namespace {
using namespace boost::ut;
using namespace folia::math;
namespace lite = folia::math::adaptors::lite;
#include "math/fixtures/lite.inc"

std::string quote(std::u16string_view value) {
    std::string result = "\"";
    constexpr char hex[] = "0123456789abcdef";
    for (std::size_t i = 0; i < value.size(); ++i) {
        const auto unit = static_cast<std::uint16_t>(value[i]);
        if (unit == '"')
            result += "\\\"";
        else if (unit == '\\')
            result += "\\\\";
        else if (unit == '\b')
            result += "\\b";
        else if (unit == '\f')
            result += "\\f";
        else if (unit == '\n')
            result += "\\n";
        else if (unit == '\r')
            result += "\\r";
        else if (unit == '\t')
            result += "\\t";
        else if (unit < 0x20 || (unit >= 0xD800 && unit <= 0xDFFF)) {
            if (unit >= 0xD800 && unit <= 0xDBFF && i + 1 < value.size()) {
                const auto low = static_cast<std::uint16_t>(value[i + 1]);
                if (low >= 0xDC00 && low <= 0xDFFF) {
                    result += utf16::encode(value.substr(i, 2));
                    ++i;
                    continue;
                }
            }
            result += "\\u";
            result.push_back(hex[(unit >> 12) & 0x0F]);
            result.push_back(hex[(unit >> 8) & 0x0F]);
            result.push_back(hex[(unit >> 4) & 0x0F]);
            result.push_back(hex[unit & 0x0F]);
        } else if (unit < 0x80) {
            result.push_back(static_cast<char>(unit));
        } else {
            result += utf16::encode(value.substr(i, 1));
        }
    }
    result.push_back('"');
    return result;
}

std::string nodeJson(lite::LiteBase const &adaptor, DOMNodeRef const &node) {
    const auto kind = adaptor.kind(node);
    if (kind == u"#text" || kind == u"#comment")
        return "{\"kind\":" + quote(kind) + ",\"value\":" + quote(adaptor.value(node)) + "}";
    std::string result = "{\"kind\":" + quote(kind) + ",\"attributes\":[";
    const auto attributes = adaptor.allAttributes(node);
    for (std::size_t i = 0; i < attributes.size(); ++i) {
        if (i)
            result += ',';
        result += '[' + quote(attributes[i].name) + ',' + quote(attributes[i].value) + ']';
    }
    result += "],\"children\":[";
    const auto children = adaptor.childNodes(node);
    for (std::size_t i = 0; i < children.size(); ++i) {
        if (i)
            result += ',';
        result += nodeJson(adaptor, children[i]);
    }
    result += "]}";
    return result;
}

std::string documentJson(lite::LiteBase const &adaptor, DOMDocumentRef const &document) {
    return "{\"kind\":\"#document\",\"doctype\":" + quote(adaptor.doctype(document)) +
           ",\"root\":" + nodeJson(adaptor, adaptor.root(document)) +
           ",\"head\":" + nodeJson(adaptor, adaptor.head(document)) +
           ",\"body\":" + nodeJson(adaptor, adaptor.body(document)) + "}";
}

std::string parseJson(lite::LiteBase const &adaptor, DOMDocumentRef const &document) {
    return "{\"document\":" + documentJson(adaptor, document) +
           ",\"bodyText\":" + quote(adaptor.textContent(adaptor.body(document))) +
           ",\"bodyInnerHTML\":" + quote(adaptor.innerHTML(adaptor.body(document))) +
           ",\"rootHTML\":" + quote(adaptor.outerHTML(adaptor.root(document))) +
           ",\"rootXML\":" + quote(adaptor.serializeXML(adaptor.root(document))) + "}";
}

LiteSourceFixture const &fixture(std::u16string_view id) {
    for (const auto &row : liteSourceFixtures)
        if (row.id == id)
            return row;
    throw std::runtime_error("missing Lite Source fixture");
}

LiteSourceMutationStep const &mutationStep(std::u16string_view name) {
    for (const auto &row : liteSourceMutationSteps)
        if (row.name == name)
            return row;
    throw std::runtime_error("missing Lite Source mutation step");
}

std::vector<LiteSourceLoadStep> sourceLoads(std::u16string_view recordId) {
    std::vector<LiteSourceLoadStep> result;
    for (const auto &load : liteSourceEntityLoadSteps)
        if (load.recordId == recordId)
            result.push_back(load);
    return result;
}

std::u16string_view shardKey(std::u16string_view sourceFile) {
    const auto slash = sourceFile.find_last_of(u"/\\");
    auto name = sourceFile.substr(slash == sourceFile.npos ? 0 : slash + 1);
    if (name.ends_with(u".js")) name.remove_suffix(3);
    return name;
}

std::vector<std::u16string> nativeShardKeys(std::vector<LiteSourceLoadStep> const& source) {
    std::vector<std::u16string> result;
    for (const auto& load : source) result.emplace_back(shardKey(load.file));
    std::ranges::sort(result);
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

std::vector<std::u16string> nativeShardKeys(folia::math::entities::EntityModule const& entities) {
    return std::vector<std::u16string>(entities.loadedFiles().begin(),
                                       entities.loadedFiles().end());
}

std::string shardKeysJson(std::vector<std::u16string> const& keys) {
    std::string result = "[";
    for (std::size_t i = 0; i < keys.size(); ++i) {
        if (i) result += ',';
        result += quote(keys[i]);
    }
    return result + ']';
}

std::string expected(std::u16string_view value) {
    return utf16::encode(value);
}
bool sourceMatch(std::string const &actual, std::u16string_view expectedValue,
                 std::u16string_view id, std::u16string_view input = {}) {
    const auto expectedBytes = expected(expectedValue);
    if (actual == expectedBytes)
        return true;
    std::size_t mismatch = 0;
    while (mismatch < actual.size() && mismatch < expectedBytes.size() &&
           actual[mismatch] == expectedBytes[mismatch])
        ++mismatch;
    const auto start = mismatch > 64 ? mismatch - 64 : 0;
    std::cerr << "[Lite Source mismatch] id=" << utf16::encode(id)
              << " input_length_utf16=" << input.size() << " first_byte=" << mismatch
              << " actual_bytes=" << actual.size() << " expected_bytes=" << expectedBytes.size()
              << " actual_near=" << actual.substr(start, 160)
              << " expected_near=" << expectedBytes.substr(start, 160) << '\n';
    return false;
}
std::string step(std::u16string_view name, std::string fields) {
    return "{\"step\":" + quote(name) + fields + "}";
}

std::string pathJson(lite::LiteBase const &adaptor, DOMNodeRef node, DOMNodeRef const &root) {
    if (!node)
        return "null";
    std::vector<std::size_t> parts;
    while (node && node != root) {
        const auto parent = adaptor.parent(node);
        if (!parent)
            return "null";
        const auto siblings = adaptor.childNodes(parent);
        const auto found = std::ranges::find(siblings, node);
        if (found == siblings.end())
            return "null";
        parts.push_back(static_cast<std::size_t>(found - siblings.begin()));
        node = parent;
    }
    if (node != root)
        return "null";
    std::string result = "[";
    for (auto it = parts.rbegin(); it != parts.rend(); ++it) {
        if (it != parts.rbegin())
            result += ',';
        result += std::to_string(*it);
    }
    return result + ']';
}

std::string pathsJson(lite::LiteBase const &adaptor, std::span<DOMNodeRef const> nodes,
                      DOMNodeRef const &root) {
    std::string result = "[";
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        if (i)
            result += ',';
        result += pathJson(adaptor, nodes[i], root);
    }
    return result + ']';
}

std::vector<std::u16string_view> const fixtureSelectors() {
    std::vector<std::u16string_view> result;
    for (std::size_t i = 0; i < liteSourceFixtureCount; ++i) {
        const auto &row = liteSourceFixtures[i];
        if (row.kind == LiteSourceFixtureKind::query) {
            for (std::size_t j = row.selectorOffset; j < row.selectorOffset + row.selectorCount;
                 ++j)
                result.push_back(liteSourceSelectors[j].value);
            break;
        }
    }
    return result;
}
} // namespace

suite lite_source_json_surrogate_witness = [] {
    "Lite test observation JSON preserves lone UTF16 surrogates"_test = [] {
        const std::array<char16_t, 1> lone{static_cast<char16_t>(0xD800)};
        expect(quote(std::u16string_view(lone.data(), lone.size())) == "\"\\ud800\"");
    };
};

suite lite_source_parser_records = [] {
    "Lite parser matches Source trees and serialization"_test = [] {
        lite::LiteAdaptor adaptor;
        std::size_t compared = 0;
        for (const auto &row : liteSourceFixtures) {
            if (row.kind != LiteSourceFixtureKind::parse)
                continue;
            try {
                const auto document = adaptor.parse(row.html, row.format);
                expect(sourceMatch(parseJson(adaptor, document), row.expectedObservation, row.id,
                                   row.html));
            } catch (const std::range_error &) {
                expect(row.id == u"parse/unknown-numeric-entity-edge")
                    << "Native range error is separate from Source RangeError naming: "
                    << utf16::encode(row.id);
            }
            ++compared;
        }
        expect(compared == 30_u);
    };
};

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

suite lite_source_mutation_operations = [] {
    "Lite clone insert move remove replace and split match Source operation traces"_test = [] {
        const auto &mutationFixture = fixture(u"mutations/clone-insert-move-remove-replace-split");
        lite::LiteAdaptor adaptor;
        const auto document = adaptor.parse(mutationFixture.html);
        const DOMNodeRef body = adaptor.body(document);
        const DOMNodeRef parent = adaptor.getElement(u"#parent", body);
        const DOMNodeRef other = adaptor.getElement(u"#other", body);
        const DOMNodeRef old = adaptor.getElement(u"#old", body);
        auto actual = step(u"initial", ",\"tree\":" + documentJson(adaptor, document));
        expect(sourceMatch(actual, mutationStep(u"initial").expected, u"mutation/initial",
                           mutationFixture.html));

        const auto shallow = adaptor.clone(parent, false);
        const auto deep = adaptor.clone(parent, true);
        actual = step(u"clone-shallow-and-deep", ",\"shallow\":" + nodeJson(adaptor, shallow) +
                                                     ",\"deep\":" + nodeJson(adaptor, deep));
        expect(sourceMatch(actual, mutationStep(u"clone-shallow-and-deep").expected,
                           u"mutation/clone-shallow-and-deep"));
        const auto clonedChildren = adaptor.childNodes(deep);
        expect(adaptor.parent(clonedChildren[0]) == deep);
        const auto clonedElementChildren = adaptor.childNodes(clonedChildren[0]);
        expect(!adaptor.parent(clonedElementChildren[0]))
            << "Source deep clones text children without assigning a parent";
        expect(!adaptor.parent(clonedChildren[1]))
            << "Source deep clones comment children without assigning a parent";

        const std::array<AttributeData, 2> attrs{{{u"id", u"inserted"}, {u"title", u"new"}}};
        const auto text = adaptor.text(u"I");
        const std::array<DOMNodeRef, 1> child{text};
        const auto inserted = adaptor.node(u"em", attrs, child);
        adaptor.insert(inserted, old);
        actual = step(u"insert-before-old", ",\"tree\":" + documentJson(adaptor, document));
        expect(sourceMatch(actual, mutationStep(u"insert-before-old").expected,
                           u"mutation/insert-before-old"));

        adaptor.append(other, inserted);
        actual = step(u"move-by-append", ",\"tree\":" + documentJson(adaptor, document) +
                                             ",\"oldParent\":" + nodeJson(adaptor, parent) +
                                             ",\"newParent\":" + nodeJson(adaptor, other));
        expect(sourceMatch(actual, mutationStep(u"move-by-append").expected,
                           u"mutation/move-by-append"));

        const auto removed = adaptor.remove(inserted);
        actual = step(u"remove", ",\"detached\":" + nodeJson(adaptor, removed) +
                                     ",\"tree\":" + documentJson(adaptor, document));
        expect(sourceMatch(actual, mutationStep(u"remove").expected, u"mutation/remove"));

        const std::array<AttributeData, 1> replacementAttrs{{{u"id", u"replacement"}}};
        const std::array<DOMNodeRef, 1> replacementChildren{adaptor.text(u"R")};
        const auto replacement = adaptor.node(u"strong", replacementAttrs, replacementChildren);
        const auto replaced = adaptor.replace(replacement, old);
        actual = step(u"replace", ",\"detached\":" + nodeJson(adaptor, replaced) +
                                      ",\"tree\":" + documentJson(adaptor, document));
        expect(sourceMatch(actual, mutationStep(u"replace").expected, u"mutation/replace"));

        const auto splitSource = adaptor.text(u"abcd");
        adaptor.append(parent, splitSource);
        const auto splitTail = adaptor.split(splitSource, 2);
        actual = step(u"split-text-at-2", ",\"tree\":" + documentJson(adaptor, document) +
                                              ",\"left\":" + quote(adaptor.value(splitSource)) +
                                              ",\"right\":" + quote(adaptor.value(splitTail)));
        expect(sourceMatch(actual, mutationStep(u"split-text-at-2").expected,
                           u"mutation/split-text-at-2"));

        struct SplitProbe {
            std::u16string_view name;
            double offset;
            std::string_view jsonOffset;
        };
        const std::array probes{
            SplitProbe{u"split-attached-negative", -1, "-1"},
            SplitProbe{u"split-attached-fractional", 1.5, "1.5"},
            SplitProbe{u"split-attached-nan", std::numeric_limits<double>::quiet_NaN(), "\"NaN\""},
            SplitProbe{u"split-attached-past-end", 9, "9"}};
        for (const auto &probe : probes) {
            const auto edgeParent = adaptor.node(u"div");
            const auto edgeText = adaptor.text(u"wxyz");
            adaptor.append(edgeParent, edgeText);
            std::string result;
            try {
                const auto tail = adaptor.split(edgeText, probe.offset);
                result = "{\"threw\":false,\"left\":" + quote(adaptor.value(edgeText)) +
                         ",\"right\":" + quote(adaptor.value(tail)) +
                         ",\"tree\":" + nodeJson(adaptor, edgeParent) +
                         ",\"leftParent\":" + (adaptor.parent(edgeText) ? "true" : "false") +
                         ",\"rightParent\":" + (adaptor.parent(tail) ? "true" : "false") + '}';
            } catch (const std::exception &error) {
                result = "{\"nativeException\":" + quote(utf16::decode(error.what()).text) +
                         ",\"valueAfter\":" + quote(adaptor.value(edgeText)) +
                         ",\"tree\":" + nodeJson(adaptor, edgeParent) + '}';
            }
            actual = "{\"step\":" + quote(probe.name) +
                     ",\"offset\":" + std::string(probe.jsonOffset) + ",\"result\":" + result + '}';
            expect(sourceMatch(actual, mutationStep(probe.name).expected, probe.name));
        }

        const std::array<DOMNodeRef, 1> identityChildInput{adaptor.text(u"x")};
        const auto identityTarget = adaptor.node(u"u", {}, identityChildInput);
        adaptor.append(parent, identityTarget);
        const auto identityChild = adaptor.childNode(identityTarget, 0);
        const auto identityReplacement = adaptor.node(u"b");
        const auto identityOld = adaptor.replace(identityReplacement, identityTarget);
        actual =
            step(u"replace-identity-and-lifecycle",
                 std::string(",\"sameOld\":") + (identityOld == identityTarget ? "true" : "false") +
                     ",\"detachedParent\":" + (adaptor.parent(identityOld) ? "false" : "true") +
                     ",\"replacementInParent\":" +
                     (adaptor.parent(identityReplacement) ? "false" : "true") +
                     ",\"childIdentitySurvives\":" +
                     (adaptor.childNode(identityOld, 0) == identityChild ? "true" : "false") +
                     ",\"oldParentBeforeWasNull\":false");
        expect(sourceMatch(actual, mutationStep(u"replace-identity-and-lifecycle").expected,
                           u"mutation/replace-identity-and-lifecycle"));
    };
};

suite lite_source_ownership_lifetimes = [] {
    "Lite weak parents and independent trees release nodes at their final owner"_test = [] {
        std::weak_ptr<DOMNode> weakRoot;
        std::weak_ptr<DOMNode> weakChild;
        std::weak_ptr<DOMNode> weakBody;
        std::weak_ptr<DOMDocument> weakDocument;
        std::weak_ptr<DOMWindow> weakWindow;
        lite::LiteDocumentRef retainedDocument;
        lite::LiteDocumentRef windowDocument;
        {
            lite::LiteAdaptor adaptor;
            const auto document = adaptor.parse(u"<div><span>text</span></div>");
            weakDocument = document;
            weakBody = adaptor.body(document);
            retainedDocument = std::dynamic_pointer_cast<lite::LiteDocument>(document);
            weakWindow = adaptor.window();
            windowDocument = adaptor.window()->document();
            expect(!retainedDocument->defaultView())
                << "parse creates a new document without Source defaultView";
            expect(windowDocument != retainedDocument)
                << "parse does not reuse window.document";
            expect(windowDocument->defaultView() == adaptor.window())
                << "window.document refers weakly back to its owning window";
            auto root = adaptor.getElement(u"div", adaptor.body(document));
            auto child = adaptor.getElement(u"span", root);
            weakRoot = root;
            weakChild = child;
            expect(adaptor.parent(child) == root)
                << "nested child parent() should identify its owning root";
            root.reset();
            child.reset();
            expect(!weakRoot.expired()) << "document owns nested Lite nodes";
            expect(!weakChild.expired()) << "element child vector owns nested Lite nodes";
        }
        expect(weakWindow.expired()) << "LiteWindow should expire after its adaptor releases it";
        expect(!windowDocument->defaultView())
            << "window.document does not keep its owning window alive";
        windowDocument.reset();
        retainedDocument.reset();
        expect(weakRoot.expired()) << "released document should release root element";
        expect(weakChild.expired()) << "released parent should release nested child";
        expect(weakBody.expired()) << "released document should release body element";
        expect(weakDocument.expired()) << "last LiteDocument owner should release document";

        std::weak_ptr<DOMNode> weakDetached;
        {
            lite::LiteAdaptor adaptor;
            const auto parent = adaptor.node(u"div");
            const auto child = adaptor.text(u"detached");
            adaptor.append(parent, child);
            weakDetached = child;
            const auto removed = adaptor.remove(child);
            expect(!adaptor.parent(removed));
        }
        expect(weakDetached.expired())
            << "detached node should expire after local owners leave scope";

        std::weak_ptr<DOMNode> weakThrowing;
        {
            lite::LiteAdaptor adaptor;
            auto text = adaptor.text(u"throwing text");
            weakThrowing = text;
            expect(throws<std::invalid_argument>([&] { adaptor.insert({}, text); }));
            text.reset();
        }
        expect(weakThrowing.expired()) << "a throwing Lite operation leaves no hidden text owner";

        std::weak_ptr<DOMNode> weakListed;
        {
            const auto node = lite::LiteElement::create(u"p");
            weakListed = node;
            {
                lite::LiteList list(std::array<DOMNodeRef, 1>{node});
                expect(list.size() == 1_u);
                expect(!weakListed.expired());
            }
        }
        expect(weakListed.expired()) << "LiteList destruction should release its final node owner";

        std::weak_ptr<DOMDocument> weakFirst;
        std::weak_ptr<DOMDocument> weakSecond;
        {
            lite::LiteAdaptor first, second;
            auto one = first.parse(u"<p>one</p>");
            auto two = second.parse(u"<p>two</p>");
            weakFirst = one;
            weakSecond = two;
            expect(first.textContent(first.body(one)) == u"one");
            expect(second.textContent(second.body(two)) == u"two");
        }
        expect(weakFirst.expired()) << "first adaptor/document tree should release independently";
        expect(weakSecond.expired()) << "second adaptor/document tree should release independently";
    };
};

suite lite_source_node_mixin_metrics = [] {
    "Lite NodeMixin badCSS and badSizes agree with pinned measurement values"_test = [] {
        const auto &row = fixture(u"measurement/NodeMixin-text-and-element");
        lite::LiteAdaptor adaptor;
        const auto document = adaptor.parse(row.html);
        const auto element = adaptor.getElement(u"#sized", adaptor.body(document));
        expect(adaptor.fontSize(element) == liteSourceElementMetric.fontSize);
        expect(adaptor.fontFamily(element) == liteSourceElementMetric.fontFamily);
        const auto [width, height] = adaptor.nodeSize(element);
        expect(width == liteSourceElementMetric.width);
        expect(height == liteSourceElementMetric.height);
        const auto [emWidth, emHeight] = adaptor.nodeSize(element, 2);
        expect(emWidth == liteSourceElementMetric.width);
        expect(emHeight == liteSourceElementMetric.height);
        const auto [localWidth, localHeight] = adaptor.nodeSize(element, 1, false);
        expect(localWidth == liteSourceElementMetric.width);
        expect(localHeight == liteSourceElementMetric.height);
        const auto box = adaptor.nodeBBox(element);
        expect(box.left == liteSourceElementMetric.left);
        expect(box.top == liteSourceElementMetric.top);
        expect(box.right == liteSourceElementMetric.right);
        expect(box.bottom == liteSourceElementMetric.bottom);
        expect(liteSourceTextNodeSizeThrows)
            << "Source text receiver error " << utf16::encode(liteSourceTextNodeSizeErrorName)
            << ": " << utf16::encode(liteSourceTextNodeSizeErrorMessage)
            << " is Source-only; native text nodes use typed Lite APIs";
    };
};

suite lite_source_entities = [] {
    // Source Promise/loader failures and JavaScript invalid-object exceptions
    // stay in the fixture as Source evidence; native checks compare final
    // synchronous translations and native std::range_error ordering only.
    "Lite parser entity retry results match Source"_test = [] {
        const auto &row = fixture(u"entities/async-load-and-parser-retry");
        auto entities = std::make_shared<folia::math::entities::EntityModule>();
        lite::LiteAdaptor adaptor({}, {}, entities);
        const auto document = adaptor.parse(row.html);
        expect(sourceMatch(utf16::encode(adaptor.textContent(adaptor.body(document))),
                           row.expectedText, row.id, row.html));
        expect(sourceMatch(documentJson(adaptor, document), row.expectedDocument, row.id,
                           row.html));
        const auto sourceLoaded = sourceLoads(row.id);
        const auto expectedKeys = nativeShardKeys(sourceLoaded);
        const auto actualKeys = nativeShardKeys(*entities);
        expect(actualKeys == expectedKeys)
            << "loaded shard keys differ after Source retry; expected="
            << shardKeysJson(expectedKeys)
            << " actual=" << shardKeysJson(actualKeys) << " input=" << utf16::encode(row.html);
    };
    "Lite entity numeric failures preserve shard load order"_test = [] {
        const auto &first = fixture(u"entities/invalid-numeric-before-missing-shard-retry");
        const auto &second = fixture(u"entities/missing-shard-retry-before-invalid-numeric");
        const auto firstSourceLoads = sourceLoads(first.id);
        const auto secondSourceLoads = sourceLoads(second.id);
        folia::math::entities::EntityModule numericFirst;
        expect(throws<std::range_error>([&] { (void)numericFirst.translate(first.rawInput); }));
        const auto expectedFirstKeys = nativeShardKeys(firstSourceLoads);
        const auto actualFirstKeys = nativeShardKeys(numericFirst);
        expect(actualFirstKeys == expectedFirstKeys)
            << "loaded shards after numeric-first failure; expected="
            << shardKeysJson(expectedFirstKeys)
            << " actual=" << shardKeysJson(actualFirstKeys)
            << " input=" << utf16::encode(first.rawInput);

        folia::math::entities::EntityModule shardFirst;
        expect(throws<std::range_error>([&] { (void)shardFirst.translate(second.rawInput); }));
        const auto expectedSecondKeys = nativeShardKeys(secondSourceLoads);
        const auto actualSecondKeys = nativeShardKeys(shardFirst);
        expect(actualSecondKeys == expectedSecondKeys)
            << "loaded shards after entity-first retry; expected="
            << shardKeysJson(expectedSecondKeys)
            << " actual=" << shardKeysJson(actualSecondKeys)
            << " input=" << utf16::encode(second.rawInput);
    };
    "Lite entity loading disabled preserves an unknown named reference as Source does"_test = [] {
        const auto &row = fixture(u"entities/loading-disabled");
        folia::math::entities::EntityModule entities;
        entities.options().loadMissingEntities = false;
        expect(entities.translate(row.text) == row.expectedText);
        expect(entities.loadedFiles().empty());
    };
};
