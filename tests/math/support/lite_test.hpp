#pragma once

#include "support/folia_test.hpp"
#include "math/support/lite_fixtures.hpp"
import folia.math.adaptors.lite_adaptor;
import folia.math.util.utf16;

namespace {
using namespace boost::ut;
using namespace folia::math;
namespace lite = folia::math::adaptors::lite;
using namespace math_test::fixtures;

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
    for (const auto &row : liteSourceFixtures())
        if (row.id == id)
            return row;
    throw std::runtime_error("missing Lite Source fixture");
}

LiteSourceMutationStep const &mutationStep(std::u16string_view name) {
    for (const auto &row : liteSourceMutationSteps())
        if (row.name == name)
            return row;
    throw std::runtime_error("missing Lite Source mutation step");
}

std::vector<LiteSourceLoadStep> sourceLoads(std::u16string_view recordId) {
    std::vector<LiteSourceLoadStep> result;
    for (const auto &load : liteSourceEntityLoadSteps())
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
    for (std::size_t i = 0; i < liteSourceFixtureCount(); ++i) {
        const auto &row = liteSourceFixtures()[i];
        if (row.kind == LiteSourceFixtureKind::query) {
            for (std::size_t j = row.selectorOffset; j < row.selectorOffset + row.selectorCount;
                 ++j)
                result.push_back(liteSourceSelectors()[j].value);
            break;
        }
    }
    return result;
}
} // namespace

