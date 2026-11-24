#include "math/support/lite_test.hpp"

suite lite_source_node_mixin_metrics = [] {
    "Lite NodeMixin badCSS and badSizes agree with pinned measurement values"_test = [] {
        const auto &row = fixture(u"measurement/NodeMixin-text-and-element");
        lite::LiteAdaptor adaptor;
        const auto document = adaptor.parse(row.html);
        const auto element = adaptor.getElement(u"#sized", adaptor.body(document));
        expect(adaptor.fontSize(element) == liteSourceElementMetric().fontSize);
        expect(adaptor.fontFamily(element) == liteSourceElementMetric().fontFamily);
        const auto [width, height] = adaptor.nodeSize(element);
        expect(width == liteSourceElementMetric().width);
        expect(height == liteSourceElementMetric().height);
        const auto [emWidth, emHeight] = adaptor.nodeSize(element, 2);
        expect(emWidth == liteSourceElementMetric().width);
        expect(emHeight == liteSourceElementMetric().height);
        const auto [localWidth, localHeight] = adaptor.nodeSize(element, 1, false);
        expect(localWidth == liteSourceElementMetric().width);
        expect(localHeight == liteSourceElementMetric().height);
        const auto box = adaptor.nodeBBox(element);
        expect(box.left == liteSourceElementMetric().left);
        expect(box.top == liteSourceElementMetric().top);
        expect(box.right == liteSourceElementMetric().right);
        expect(box.bottom == liteSourceElementMetric().bottom);
        expect(liteSourceTextNodeSizeThrows())
            << "Source text receiver error " << utf16::encode(liteSourceTextNodeSizeErrorName())
            << ": " << utf16::encode(liteSourceTextNodeSizeErrorMessage())
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
