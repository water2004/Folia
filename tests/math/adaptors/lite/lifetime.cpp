#include "math/support/lite_test.hpp"

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

