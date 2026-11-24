#include "math/support/lite_test.hpp"

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

